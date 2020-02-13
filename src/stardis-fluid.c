#include "stardis-fluid.h"
#include "stardis-compute.h"

#include <sdis.h>
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
  if (fluid_props->is_green || vtx->time > fluid_props->t0) {
    /* Always use temp for Green mode, regardless of time */
    if (fluid_props->imposed_temperature >= 0)
      return fluid_props->imposed_temperature;
    else return -1;
  }
  /* Time is <= 0: use tinit */
  if (fluid_props->tinit >= 0) return fluid_props->tinit;
  /* Must have had tinit defined: error! */
  if (fluid_props->name[0]) {
    fprintf(stderr,
      "fluid_get_temperature: getting undefined Tinit (fluid '%s')\n",
      fluid_props->name);
    ASSERT(0);
    abort();
  } else
    FATAL("fluid_get_temperature: getting undefined Tinit\n");
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
   const double tinit,
   const double imposed_temperature,
   struct sdis_medium*** media_ptr,
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
  res = sdis_data_create(dev, sizeof(struct fluid), ALIGNOF(struct fluid),
    NULL, &data);
  if (res != RES_OK) goto error;

  sz = sa_size(*media_ptr);
  ASSERT(sz < INT_MAX);
  fluid_props = sdis_data_get(data); /* Fetch the allocated memory space */
  if (name) strncpy(fluid_props->name, name, sizeof(fluid_props->name));
  else fluid_props->name[0] = '\0';
  fluid_props->cp = cp;
  fluid_props->rho = rho;
  fluid_props->tinit = tinit;
  fluid_props->imposed_temperature = imposed_temperature;
  fluid_props->is_green = is_green;
  fluid_props->is_outside = is_outside;
  fluid_props->id = (unsigned)sz;

  res = sdis_fluid_create(dev, &fluid_shader, data, sa_add(*media_ptr, 1));
  if (res != RES_OK) goto error;
  *out_id = fluid_props->id;

end:
  if (data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}
