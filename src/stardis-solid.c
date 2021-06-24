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
  if(solid_props->imposed_temperature >= 0)
    /* If there is an imposed temp, it is imposed regardless of time */
    return solid_props->imposed_temperature;
  if(vtx->time <= solid_props->t0) {
    /* If time is <= t0: use tinit */
    if(solid_props->tinit < 0) {
      if(str_is_empty(&solid_props->name)) {
        FATAL("solid_get_temperature: getting undefined Tinit\n");
      } else {
        VFATAL("solid_get_temperature: getting undefined Tinit (solid '%s')\n",
          ARG1(str_cget(&solid_props->name)));
      }
    }
    return solid_props->tinit;
  }
  return -1; /* Unknown temperature */
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
create_solver_solid
  (struct stardis* stardis,
   const struct solid* solid_props)
{
  res_T res = RES_OK;
  struct sdis_solid_shader solid_shader = SDIS_SOLID_SHADER_NULL;
  struct sdis_data* data = NULL;
  struct solid* props;
  /* Could be less restrictive if green output included positions/dates */

  ASSERT(stardis && solid_props);
  solid_shader.calorific_capacity = solid_get_calorific_capacity;
  solid_shader.thermal_conductivity = solid_get_thermal_conductivity;
  solid_shader.volumic_mass = solid_get_volumic_mass;
  solid_shader.delta_solid = solid_get_delta;
#if Stardis_VERSION_MINOR == 3
  solid_shader.delta_boundary = solid_get_delta_boundary;
#endif
  solid_shader.temperature = solid_get_temperature;
  ERR(sdis_data_create(stardis->dev, sizeof(struct solid), ALIGNOF(struct solid),
    NULL, &data));

  props = sdis_data_get(data); /* Fetch the allocated memory space */
  init_solid(stardis->allocator, props);
  cp_solid(props, solid_props);
  if(solid_props->vpower != 0) solid_shader.volumic_power = solid_get_power;
  if(solid_props->solid_id >= darray_media_ptr_size_get(&stardis->media)) {
    ERR(darray_media_ptr_resize(&stardis->media, solid_props->solid_id + 1));
  }
  ASSERT(darray_media_ptr_data_get(&stardis->media)[solid_props->solid_id] == NULL);
  ERR(sdis_solid_create(stardis->dev, &solid_shader, data,
    darray_media_ptr_data_get(&stardis->media) + solid_props->solid_id));

end:
  if(data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}

void
init_solid(struct mem_allocator* allocator, struct solid* dst)
{
  str_init(allocator, &dst->name);
  dst->lambda = 1;
  dst->rho = 1;
  dst->cp = 1;
  dst->delta = 1;
  dst->tinit = -1;
  dst->imposed_temperature = -1;
  dst->vpower = 0;
  dst->t0 = 0;
  dst->is_outside = 0;
  dst->is_green = 0;
  dst->desc_id = UINT_MAX;
  dst->solid_id = UINT_MAX;
}

void
release_solid(struct solid* solid)
{
  str_release(&solid->name);
}

res_T
str_print_solid(struct str* str, const struct solid* s)
{
  res_T res = RES_OK;
  ASSERT(str && s);
  STR_APPEND_PRINTF(str, "Solid '%s': lambda=%g rho=%g cp=%g delta=%g",
    ARG5( str_cget(&s->name), s->lambda, s->rho, s->cp, s->delta ) );
  if(s->vpower != 0) {
    STR_APPEND_PRINTF(str, " VPower=%g", ARG1( s->vpower ) );
  }
  if(s->tinit >= 0) {
    STR_APPEND_PRINTF(str, " Tinit=%g", ARG1( s->tinit ) );
  }
  if(s->imposed_temperature >= 0) {
    STR_APPEND_PRINTF(str, " Temp=%g", ARG1( s->imposed_temperature ) );
  }
  STR_APPEND_PRINTF(str, " (it is medium %u)", ARG1( s->solid_id ) );
end:
  return res;
error:
  goto end;
}

res_T
cp_solid(struct solid* dst, const struct solid* src)
{
  dst->lambda = src->lambda;
  dst->rho = src->rho;
  dst->cp = src->cp;
  dst->delta = src->delta;
  dst->tinit = src->tinit;
  dst->imposed_temperature = src->imposed_temperature;
  dst->vpower = src->vpower;
  dst->t0 = src->t0;
  dst->is_outside = src->is_outside;
  dst->is_green = src->is_green;
  dst->desc_id = src->desc_id;
  dst->solid_id = src->solid_id;
  return str_copy(&dst->name, &src->name);
}
