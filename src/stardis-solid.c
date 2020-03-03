#include "stardis-solid.h"
#include "stardis-compute.h"
#include "stardis-app.h"

#include <sdis.h>

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
  return solid_props->delta;
}
#endif

static double
solid_get_temperature
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  if(solid_props->is_green || vtx->time > solid_props->t0) {
    /* Always use temp for Green mode, regardless of time */
    if(solid_props->imposed_temperature >= 0)
      return solid_props->imposed_temperature;
    else return -1;
  }
  /* Time is <= 0: use tinit */
  if(solid_props->tinit >= 0) return solid_props->tinit;
  /* Must have had t_init defined: error! */
  if(str_is_empty(&solid_props->name)) {
    FATAL("solid_get_temperature: getting undefined Tinit\n");
  } else {
    fprintf(stderr,
      "solid_get_temperature: getting undefined Tinit (solid '%s')\n",
      str_cget(&solid_props->name));
    ASSERT(0);
    abort();
  }
}

static double
solid_get_power
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->vpower;
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
create_solid
  (struct sdis_device* dev,
   struct mem_allocator* allocator,
   const struct str* name,
   const double lambda,
   const double rho,
   const double cp,
   const double delta,
   const int is_green,
   const int is_outside,
   const double tinit,
   const double imposed_temperature,
   const double vpower,
   struct darray_media_ptr* media_ptr,
   unsigned* out_id)
{
  res_T res = RES_OK;
  struct sdis_solid_shader solid_shader = SDIS_SOLID_SHADER_NULL;
  struct sdis_data* data = NULL;
  struct solid* solid_props;
  size_t sz;
  /* Could be less restrictive if green output included positions/dates */

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
  ERR(sdis_data_create(dev, sizeof(struct solid), ALIGNOF(struct solid),
    NULL, &data));

  sz = darray_media_ptr_size_get(media_ptr);
  ASSERT(sz < INT_MAX);
  solid_props = sdis_data_get(data); /* Fetch the allocated memory space */
  str_init(allocator, &solid_props->name);
  if(name) str_copy(&solid_props->name, name);
  solid_props->lambda = lambda;
  solid_props->rho = rho;
  solid_props->cp = cp;
  solid_props->delta = delta;
  solid_props->tinit = tinit;
  solid_props->imposed_temperature = imposed_temperature;
  solid_props->vpower = vpower;
  solid_props->is_green = is_green;
  solid_props->is_outside = is_outside;
  solid_props->id = (unsigned)sz;
  if(vpower != 0) solid_shader.volumic_power = solid_get_power;
  ERR(darray_media_ptr_resize(media_ptr, sz + 1));
  ERR(sdis_solid_create(dev, &solid_shader, data,
    darray_media_ptr_data_get(media_ptr) + sz));
  *out_id = solid_props->id;

end:
  if(data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}
