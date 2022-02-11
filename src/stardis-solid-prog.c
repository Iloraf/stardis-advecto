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

#include "stardis-solid-prog.h"
#include "stardis-prog.h"
#include "stardis-app.h"

#include <rsys/mem_allocator.h>

#include <sdis.h>

#include <limits.h>

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

static double
solid_prog_get_thermal_conductivity
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid_prog* const* solid_props = sdis_data_cget(data);
  struct stardis_vertex v;
  d3_set(v.P, vtx->P);
  v.time = vtx->time;
  return (*solid_props)->lambda(&v, (*solid_props)->prog_data);
}

static double
solid_prog_get_volumic_mass
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid_prog* const* solid_props = sdis_data_cget(data);
  struct stardis_vertex v;
  d3_set(v.P, vtx->P);
  v.time = vtx->time;
  return (*solid_props)->rho(&v, (*solid_props)->prog_data);
}

static double
solid_prog_get_calorific_capacity
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid_prog* const* solid_props = sdis_data_cget(data);
  struct stardis_vertex v;
  d3_set(v.P, vtx->P);
  v.time = vtx->time;
  return (*solid_props)->cp(&v, (*solid_props)->prog_data);
}

static double
solid_prog_get_delta
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid_prog* const* solid_props = sdis_data_cget(data);
  struct stardis_vertex v;
  d3_set(v.P, vtx->P);
  v.time = vtx->time;
  return (*solid_props)->delta(&v, (*solid_props)->prog_data);
}

static double
solid_prog_get_volumic_power
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid_prog* const* solid_props = sdis_data_cget(data);
  struct stardis_vertex v;
  d3_set(v.P, vtx->P);
  v.time = vtx->time;
  return (*solid_props)->vpower(&v, (*solid_props)->prog_data);
}

static double
solid_prog_get_temperature
  (const struct sdis_rwalk_vertex* vtx,
   struct sdis_data* data)
{
  const struct solid_prog* const* solid_props = sdis_data_cget(data);
  struct stardis_vertex v;
  d3_set(v.P, vtx->P);
  v.time = vtx->time;
  return (*solid_props)->temp(&v, (*solid_props)->prog_data);
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
create_solver_solid_prog
  (struct stardis* stardis,
   const struct solid_prog* solid_props)
{
  res_T res = RES_OK;
  struct sdis_solid_shader solid_shader = SDIS_SOLID_SHADER_NULL;
  struct sdis_data* data = NULL;
  const struct solid_prog** props;

  ASSERT(stardis && solid_props);
  solid_shader.calorific_capacity = solid_prog_get_calorific_capacity;
  solid_shader.thermal_conductivity = solid_prog_get_thermal_conductivity;
  solid_shader.volumic_mass = solid_prog_get_volumic_mass;
  solid_shader.delta= solid_prog_get_delta;
  solid_shader.volumic_power = solid_prog_get_volumic_power;
  solid_shader.temperature = solid_prog_get_temperature;
  ERR(sdis_data_create(stardis->dev, sizeof(struct solid_prog*),
    ALIGNOF(struct solid_prog*), NULL, &data));

  props = sdis_data_get(data); /* Fetch the allocated memory space */
  *props = solid_props;
  if(solid_props->solid_id >= darray_media_ptr_size_get(&stardis->media)) {
    ERR(darray_media_ptr_resize(&stardis->media, solid_props->solid_id + 1));
  }
  ASSERT(!darray_media_ptr_data_get(&stardis->media)[solid_props->solid_id]);
  ERR(sdis_solid_create(stardis->dev, &solid_shader, data,
    darray_media_ptr_data_get(&stardis->media) + solid_props->solid_id));

end:
  if(data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}

res_T
init_solid_prog(struct mem_allocator* allocator, struct solid_prog** dst)
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
  (*dst)->solid_id = UINT_MAX;
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
release_solid_prog
  (struct solid_prog* solid,
   struct mem_allocator* allocator)
{
  ASSERT(solid && allocator);
  str_release(&solid->name);
  str_release(&solid->prog_name);
  str_release(&solid->args);

  if(solid->prog_data)
    solid->release(solid->prog_data);
  /* library_close call is managed at lib_data level */
  MEM_RM(allocator, solid);
}

res_T
str_print_solid_prog(struct str* str, const struct solid_prog* f)
{
  res_T res = RES_OK;
  ASSERT(str && f);
  ERR(str_append_printf(str,
    "programmed solid '%s': lib='%s', args=[%s] (it is medium %u)",
    str_cget(&f->name), str_cget(&f->prog_name), str_cget(&f->args),
    f->solid_id));
end:
  return res;
error:
  goto end;
}
