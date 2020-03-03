#include "stardis-fluid.h"
#include "stardis-compute.h"
#include "stardis-app.h"

#include <sdis.h>

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
  if(fluid_props->is_green || vtx->time > fluid_props->t0) {
    /* Always use temp for Green mode, regardless of time */
    if(fluid_props->imposed_temperature >= 0)
      return fluid_props->imposed_temperature;
    else return -1;
  }
  /* Time is <= 0: use tinit */
  if(fluid_props->tinit >= 0) return fluid_props->tinit;
  /* Must have had tinit defined: error! */
  if(str_is_empty(&fluid_props->name)) {
    FATAL("fluid_get_temperature: getting undefined Tinit\n");
  } else {
    fprintf(stderr,
      "fluid_get_temperature: getting undefined Tinit (fluid '%s')\n",
      str_cget(&fluid_props->name));
    ASSERT(0);
    abort();
  }
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
create_fluid
  (struct sdis_device* dev,
   struct mem_allocator* allocator,
   const struct str* name,
   const double rho,
   const double cp,
   const int is_green,
   const int is_outside,
   const double tinit,
   const double imposed_temperature,
   struct darray_media_ptr* media_ptr,
   unsigned* out_id)
{
  res_T res = RES_OK;
  struct sdis_fluid_shader fluid_shader = SDIS_FLUID_SHADER_NULL;
  struct sdis_data* data = NULL;
  struct fluid* fluid_props;
  size_t sz;

  ASSERT(dev && rho >= 0 && cp >= 0 && media_ptr && out_id);
  ASSERT(tinit >= 0 || imposed_temperature >= 0);
  fluid_shader.calorific_capacity = fluid_get_calorific_capacity;
  fluid_shader.volumic_mass = fluid_get_volumic_mass;
  fluid_shader.temperature = fluid_get_temperature;
  ERR(sdis_data_create(dev, sizeof(struct fluid), ALIGNOF(struct fluid),
    NULL, &data));

  sz = darray_media_ptr_size_get(media_ptr);
  ASSERT(sz < INT_MAX);
  fluid_props = sdis_data_get(data); /* Fetch the allocated memory space */
  str_init(allocator, &fluid_props->name);
  if(name) str_copy(&fluid_props->name, name);
  fluid_props->cp = cp;
  fluid_props->rho = rho;
  fluid_props->tinit = tinit;
  fluid_props->imposed_temperature = imposed_temperature;
  fluid_props->is_green = is_green;
  fluid_props->is_outside = is_outside;
  fluid_props->id = (unsigned)sz;
 
  ERR(darray_media_ptr_resize(media_ptr, sz+1));
  ERR(sdis_fluid_create(dev, &fluid_shader, data,
    darray_media_ptr_data_get(media_ptr) + sz));
  *out_id = fluid_props->id;

end:
  if(data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}
