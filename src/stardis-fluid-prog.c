/* Copyright (C) 2018-2021 |Meso|Star> (contact@meso-star.com)
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

#include "stardis-fluid-prog.h"
#include "stardis-app.h"
#include "stardis-prog.h"
#include "stardis-app.h"

#include <rsys/mem_allocator.h>

#include <sdis.h>

#include <limits.h>


/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static double
fluid_prog_get_calorific_capacity
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct fluid_prog* const* fluid_props = sdis_data_cget(data);
  struct stardis_vertex v;
  d3_set(v.P, vtx->P);
  v.time = vtx->time;
  return (*fluid_props)->cp(&v, (*fluid_props)->prog_data);
}

static double
fluid_prog_get_volumic_mass
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct fluid_prog* const* fluid_props = sdis_data_cget(data);
  struct stardis_vertex v;
  d3_set(v.P, vtx->P);
  v.time = vtx->time;
  return (*fluid_props)->rho(&v, (*fluid_props)->prog_data);
}

static double
fluid_prog_get_temperature
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct fluid_prog* const* fluid_props = sdis_data_cget(data);
  struct stardis_vertex v;
  d3_set(v.P, vtx->P);
  v.time = vtx->time;
  return (*fluid_props)->temp(&v, (*fluid_props)->prog_data);
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/
res_T
create_solver_fluid_prog
  (struct stardis* stardis,
   const struct fluid_prog* fluid_props)
{
  res_T res = RES_OK;
  struct sdis_fluid_shader fluid_shader = SDIS_FLUID_SHADER_NULL;
  struct sdis_data* data = NULL;
  const struct fluid_prog** props;

  ASSERT(stardis && fluid_props);
  fluid_shader.calorific_capacity = fluid_prog_get_calorific_capacity;
  fluid_shader.volumic_mass = fluid_prog_get_volumic_mass;
  fluid_shader.temperature = fluid_prog_get_temperature;
  ERR(sdis_data_create(stardis->dev, sizeof(struct fluid_prog*),
    ALIGNOF(struct fluid_prog*), NULL, &data));

  props = sdis_data_get(data); /* Fetch the allocated memory space */
  *props = fluid_props;
  if(fluid_props->fluid_id >= darray_media_ptr_size_get(&stardis->media)) {
    ERR(darray_media_ptr_resize(&stardis->media, fluid_props->fluid_id + 1));
  }
  ASSERT(!darray_media_ptr_data_get(&stardis->media)[fluid_props->fluid_id]);
  ERR(sdis_fluid_create(stardis->dev, &fluid_shader, data,
    darray_media_ptr_data_get(&stardis->media) + fluid_props->fluid_id));

end:
  if(data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}

res_T
create_solver_external_fluid_prog
  (struct stardis* stardis,
   const struct fluid_prog* fluid_props)
{
  res_T res = RES_OK;
  struct sdis_fluid_shader fluid_shader = SDIS_FLUID_SHADER_NULL;
  struct sdis_data* data = NULL;
  const struct fluid_prog** props;

  ASSERT(stardis && fluid_props);
  fluid_shader.calorific_capacity = fluid_prog_get_calorific_capacity;
  fluid_shader.volumic_mass = fluid_prog_get_volumic_mass;
  fluid_shader.temperature = fluid_prog_get_temperature;
  /* temperature has to be provided by h_boundary_prog */
  ERR(sdis_data_create(stardis->dev, sizeof(struct fluid_prog*),
    ALIGNOF(struct fluid_prog*), NULL, &data));

  props = sdis_data_get(data); /* Fetch the allocated memory space */
  *props = fluid_props;
  if(fluid_props->fluid_id >= darray_media_ptr_size_get(&stardis->media)) {
    ERR(darray_media_ptr_resize(&stardis->media, fluid_props->fluid_id + 1));
  }
  ASSERT(!darray_media_ptr_data_get(&stardis->media)[fluid_props->fluid_id]);
  ERR(sdis_fluid_create(stardis->dev, &fluid_shader, data,
    darray_media_ptr_data_get(&stardis->media) + fluid_props->fluid_id));

end:
  if(data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}

res_T
init_fluid_prog(struct mem_allocator* allocator, struct fluid_prog** dst)
{
  res_T res = RES_OK;
  int str_initialized = 0;
  ASSERT(allocator && dst && *dst == NULL);
  *dst = MEM_CALLOC(allocator, 1, sizeof(**dst));
  if(! *dst) {
    res = RES_MEM_ERR;
    goto error;
  }
  str_init(allocator, &(*dst)->name);
  str_init(allocator, &(*dst)->prog_name);
  str_init(allocator, &(*dst)->args);
  str_initialized = 1;
  (*dst)->desc_id = UINT_MAX;
  (*dst)->fluid_id = UINT_MAX;
end:
  return res;
error:
  if(str_initialized) {
    str_release(&(*dst)->name);
    str_release(&(*dst)->prog_name);
    str_release(&(*dst)->args);
  }
  if(*dst) MEM_RM(allocator, *dst);
  goto end;
}

void
release_fluid_prog
  (struct fluid_prog* fluid,
   struct mem_allocator* allocator)
{
  ASSERT(fluid && allocator);
  str_release(&fluid->name);
  str_release(&fluid->prog_name);
  str_release(&fluid->args);

  if(fluid->prog_data
    && fluid->release) /* can be NULL if external fluid */
  {
    fluid->release(fluid->prog_data);
  }
  /* library_close call is managed at lib_data level */
  MEM_RM(allocator, fluid);
}

res_T
str_print_fluid_prog(struct str* str, const struct fluid_prog* f)
{
  res_T res = RES_OK;
  ASSERT(str && f);
  ERR(str_append_printf(str,
     "programmed fluid '%s': lib='%s', args=[%s] (it is medium %u)",
    str_cget(&f->name), str_cget(&f->prog_name), str_cget(&f->args),
    f->fluid_id));
end:
  return res;
error:
  goto end;
}
