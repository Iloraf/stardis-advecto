#include "stardis-solid.h"
#include "stardis-compute.h"

#include <sdis.h>
#include <tinyexpr.h>
#include<rsys/stretchy_array.h>

#include <limits.h>

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

static double
solid_get_calorific_capacity
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->cp;
}

static double
solid_get_thermal_conductivity
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->lambda;
}

static double
solid_get_volumic_mass
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->rho;
}

static double
solid_get_delta
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->delta;
}

#if Stardis_VERSION_MINOR == 3
static double
solid_get_delta_boundary
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return solid_props->delta
    /* Emperical scale factor that ensures that delta_boundary > delta withouht
     * being an exact multiple of delta. */
    /** 2.1 */;
}
#endif

static double
solid_get_temperature
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  char msg[128];
  ASSERT(solid_props->t_init || solid_props->temp);
  if (solid_props->is_green || vtx->time > solid_props->t0) {
    /* Always use temp for Green mode, regardless of time */
    if (solid_props->temp)
      return te_eval(solid_props->temp, vtx);
    else return -1;
  }
  /* Time is t0: use t_init */
  if (solid_props->t_init)
    return te_eval(solid_props->t_init, vtx);
  /* Must have had t_init defined: error! */
  if (solid_props->name[0])
    snprintf(msg, sizeof(msg),
      "solid_get_temperature: getting undefined Tinit (solid '%s')\n",
      solid_props->name);
  else
    snprintf(msg, sizeof(msg), "solid_get_temperature: getting undefined Tinit\n");
  FATAL(msg);
}

static double
solid_get_power
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return te_eval(solid_props->power, vtx);
}

static void
release_solid_data
  (void* s)
{
  struct solid* solid = (struct solid*)s;
  te_free(solid->t_init);
  te_free(solid->power);
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
create_solid
  (struct sdis_device* dev,
   const char* name,
   const double lambda,
   const double rho,
   const double cp,
   const double delta,
   const int is_green,
   const int is_outside,
   const char* tinit_expr, /* Can be NULL if not used by getter */
   const char* t_expr, /* Can be NULL if not used by getter */
   const char* power_expr, /* Can be NULL */
   struct sdis_medium*** media_ptr,
   int* out_has_power, /* Can be NULL */
   unsigned* out_id)
{
  res_T res = RES_OK;
  struct sdis_solid_shader solid_shader = SDIS_SOLID_SHADER_NULL;
  struct sdis_data* data = NULL;
  struct solid* solid_props;
  size_t sz;
  /* Could be less restrictive if green output included positions/dates */
  int prohibited = is_green ? NO_VAR_ALLOWED : ALL_VARS_ALLOWED;

  ASSERT(dev && lambda >= 0 && rho >= 0 && cp >= 0 && delta > 0
    && media_ptr && out_id);
  solid_shader.calorific_capacity = solid_get_calorific_capacity;
  solid_shader.thermal_conductivity = solid_get_thermal_conductivity;
  solid_shader.volumic_mass = solid_get_volumic_mass;
  solid_shader.delta_solid = solid_get_delta;
#if Stardis_VERSION_MINOR == 3
  solid_shader.delta_boundary = solid_get_delta_boundary;
#endif
  solid_shader.temperature = solid_get_temperature;
  res = sdis_data_create(dev, sizeof(struct solid), ALIGNOF(struct solid),
    release_solid_data, &data);
  if (res != RES_OK) goto error;

  sz = sa_size(*media_ptr);
  ASSERT(sz < INT_MAX);
  solid_props = sdis_data_get(data); /* Fetch the allocated memory space */
  if (name) strncpy(solid_props->name, name, sizeof(solid_props->name));
  else solid_props->name[0] = '\0';
  solid_props->lambda = lambda;
  solid_props->rho = rho;
  solid_props->cp = cp;
  solid_props->delta = delta;
  solid_props->t_init = NULL;
  solid_props->temp = NULL;
  solid_props->power = NULL;
  solid_props->is_green = is_green;
  solid_props->is_outside = is_outside;
  solid_props->t0 = 0;
  solid_props->id = (unsigned)sz;
  if (tinit_expr) {
    res = compile_expr_to_fn(&solid_props->t_init, tinit_expr,
      prohibited | T_PROHIBITED, NULL);
    if (res != RES_OK) {
      if (res == RES_BAD_ARG)
        fprintf(stderr, "Invalid initial temperature expression: %s\n",
          tinit_expr);
      goto error;
    }
  }
  if (t_expr) {
    res = compile_expr_to_fn(&solid_props->temp, t_expr, prohibited, NULL);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid temperature expression: %s\n",
        t_expr);
      goto error;
    }
  }
  if (power_expr) {
    int has_power;
    res = compile_expr_to_fn(&solid_props->power, power_expr, prohibited, &has_power);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid volumic power expression: %s\n",
        power_expr);
      goto error;
    }
    if (out_has_power) *out_has_power = has_power;
    if (has_power) solid_shader.volumic_power = solid_get_power;
  } else {
    if (out_has_power) *out_has_power = 0;
  }
  res = sdis_solid_create(dev, &solid_shader, data, sa_add(*media_ptr, 1));
  if (res != RES_OK) goto error;
  *out_id = solid_props->id;

end:
  if (data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}
