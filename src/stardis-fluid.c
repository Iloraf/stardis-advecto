/* Copyright (C) 2018-2020 |Meso|Star> (contact@meso-star.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>. */

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
    return fluid_props->imposed_temperature;
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
create_stardis_fluid
  (struct stardis* stardis,
   const struct str* name, /* Can be NULL */
   const double rho,
   const double cp,
   const int is_green,
   const int is_outside,
   const double tinit,
   const double imposed_temperature,
   unsigned* out_id)
{
  res_T res = RES_OK;
  struct sdis_fluid_shader fluid_shader = SDIS_FLUID_SHADER_NULL;
  struct sdis_data* data = NULL;
  struct fluid* fluid_props;
  size_t sz;

  ASSERT(stardis && rho >= 0 && cp >= 0 && out_id);
  fluid_shader.calorific_capacity = fluid_get_calorific_capacity;
  fluid_shader.volumic_mass = fluid_get_volumic_mass;
  fluid_shader.temperature = fluid_get_temperature;
  ERR(sdis_data_create(stardis->dev, sizeof(struct fluid), ALIGNOF(struct fluid),
    NULL, &data));

  sz = darray_media_ptr_size_get(&stardis->media);
  ASSERT(sz < INT_MAX);
  fluid_props = sdis_data_get(data); /* Fetch the allocated memory space */
  str_init(stardis->allocator, &fluid_props->name);
  if(name) str_copy(&fluid_props->name, name);
  fluid_props->t0 = 0;
  fluid_props->cp = cp;
  fluid_props->rho = rho;
  fluid_props->tinit = tinit;
  fluid_props->imposed_temperature = imposed_temperature;
  fluid_props->is_green = is_green;
  fluid_props->is_outside = is_outside;
  fluid_props->id = (unsigned)sz;
 
  ERR(darray_media_ptr_resize(&stardis->media, sz+1));
  ERR(sdis_fluid_create(stardis->dev, &fluid_shader, data,
    darray_media_ptr_data_get(&stardis->media) + sz));
  *out_id = fluid_props->id;

end:
  if(data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}
