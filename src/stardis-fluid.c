#include "stardis-fluid.h"
#include "stardis-compute.h"

#include <sdis.h>
#include <tinyexpr.h>
#include<rsys/stretchy_array.h>

#include <limits.h>

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

static double
fluid_get_calorific_capacity
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  (void)vtx;
  return fluid_props->cp;
}

static double
fluid_get_volumic_mass
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  (void)vtx;
  return fluid_props->rho;
}

static double
fluid_get_temperature
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  char msg[128];
  ASSERT(fluid_props->t_init || fluid_props->temp);
  if (fluid_props->is_green || vtx->time > fluid_props->t0) {
    /* Always use temp for Green mode, regardless of time */
    if (fluid_props->temp)
      return te_eval(fluid_props->temp, vtx);
    else return -1;
  }
  /* Time is t0: use t_init */
  if (fluid_props->t_init)
    return te_eval(fluid_props->t_init, vtx);
  /* Must have had t_init defined: error! */
  if (fluid_props->name[0])
    snprintf(msg, sizeof(msg),
      "fluid_get_temperature: getting undefined Tinit (fluid '%s')\n",
      fluid_props->name);
  else
    snprintf(msg, sizeof(msg), "fluid_get_temperature: getting undefined Tinit\n");
  FATAL(msg);
}

static void
release_fluid_data
  (void* f)
{
  struct fluid* fluid = (struct fluid*)f;
  te_free(fluid->t_init);
  te_free(fluid->temp);
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
create_fluid
  (struct sdis_device* dev,
   const char* name,
   const double rho,
   const double cp,
   const int is_green,
   const int is_outside,
   const char* tinit_expr,
   const char* t_expr,
   struct sdis_medium*** media_ptr,
   unsigned* out_id)
{
  res_T res = RES_OK;
  struct sdis_fluid_shader fluid_shader = SDIS_FLUID_SHADER_NULL;
  struct sdis_data* data = NULL;
  struct fluid* fluid_props;
  size_t sz;
  /* Could be less restrictive if green output included positions/dates */
  int prohibited = is_green ? NO_VAR_ALLOWED : ALL_VARS_ALLOWED;

  ASSERT(dev && rho >= 0 && cp >= 0 && media_ptr && out_id);
  fluid_shader.calorific_capacity = fluid_get_calorific_capacity;
  fluid_shader.volumic_mass = fluid_get_volumic_mass;
  fluid_shader.temperature = fluid_get_temperature;
  res = sdis_data_create(dev, sizeof(struct fluid), ALIGNOF(struct fluid),
    release_fluid_data, &data);
  if (res != RES_OK) goto error;

  sz = sa_size(*media_ptr);
  ASSERT(sz < INT_MAX);
  fluid_props = sdis_data_get(data); /* Fetch the allocated memory space */
  if (name) strncpy(fluid_props->name, name, sizeof(fluid_props->name));
  else fluid_props->name[0] = '\0';
  fluid_props->cp = cp;
  fluid_props->rho = rho;
  fluid_props->t_init = NULL;
  fluid_props->temp = NULL;
  fluid_props->is_green = is_green;
  fluid_props->is_outside = is_outside;
  fluid_props->t0 = 0;
  fluid_props->id = (unsigned)sz;

  if (tinit_expr) {
    res = compile_expr_to_fn(&fluid_props->t_init, tinit_expr,
      prohibited | T_PROHIBITED, NULL);
    if (res != RES_OK) {
      if (res == RES_BAD_ARG)
        fprintf(stderr, "Invalid initial temperature expression: %s\n",
          tinit_expr);
      goto error;
    }
  }
  if (t_expr) {
    res = compile_expr_to_fn(&fluid_props->temp, t_expr, prohibited, NULL);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid temperature expression: %s\n",
        t_expr);
      goto error;
    }
  }
  res = sdis_fluid_create(dev, &fluid_shader, data, sa_add(*media_ptr, 1));
  if (res != RES_OK) goto error;
  *out_id = fluid_props->id;

end:
  if (data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}
