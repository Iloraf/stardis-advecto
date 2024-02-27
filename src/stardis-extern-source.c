/* Copyright (C) 2018-2023 |Méso|Star> (contact@meso-star.com)
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

#include "stardis-app.h"
#include "stardis-extern-source.h"

#include <sdis.h>
#include <rsys/cstr.h>
#include <rsys/logger.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
sphere_get_position(const double time, double pos[3], struct sdis_data* data)
{
  const struct extern_source* src = NULL;
  ASSERT(pos);
  (void)time;

  src = sdis_data_cget(data);
  ASSERT(src->type == EXTERN_SOURCE_SPHERE);

  pos[0] = src->data.sphere.position[0];
  pos[1] = src->data.sphere.position[1];
  pos[2] = src->data.sphere.position[2];
}

static double
sphere_get_power(const double time, struct sdis_data* data)
{
  const struct extern_source* src = NULL;
  (void)time;

  src = sdis_data_cget(data);
  ASSERT(src->type == EXTERN_SOURCE_SPHERE);

  return src->data.sphere.power; /* [W] */
}

static res_T
create_solver_source_sphere
  (struct extern_source* src,
   struct stardis* stardis)
{
  struct sdis_spherical_source_create_args args =
    SDIS_SPHERICAL_SOURCE_CREATE_ARGS_NULL;
  struct sdis_data* data = NULL;
  res_T res = RES_OK;
  ASSERT(src && src->type == EXTERN_SOURCE_SPHERE && stardis);

  /* Register with the solver source a pointer to the external source */
  res = sdis_data_create(stardis->dev, sizeof(struct extern_source*),
    ALIGNOF(struct extern_source*), NULL, &data);
  if(res != RES_OK) goto error;
  *((struct extern_source**)sdis_data_get(data)) = src;

  /* Create the spherical source */
  args.position = sphere_get_position;
  args.power = sphere_get_power;
  args.data = data;
  args.radius = src->data.sphere.radius;
  res = sdis_spherical_source_create(stardis->dev, &args, &src->sdis_src);
  if(res != RES_OK) goto error;

exit:
  /* Release the local reference on the data: it is kept by the sdis source */
  if(data) SDIS(data_ref_put(data));
  return res;
error:
  logger_print(stardis->logger, LOG_ERROR,
    "Error when creating the external spherical source for the Solver -- %s\n",
    res_to_cstr(res));
  goto exit;
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
void
extern_source_release(struct extern_source* src)
{
  ASSERT(src);
  if(src->sdis_src) SDIS(source_ref_put(src->sdis_src));
}

res_T
extern_source_create_solver_source
  (struct extern_source* src,
   struct stardis* stardis)
{
  res_T res = RES_OK;
  ASSERT(src);

  switch(src->type) {
    case EXTERN_SOURCE_SPHERE:
      res = create_solver_source_sphere(src, stardis);
      if(res != RES_OK) goto error;
      break;
    default: FATAL("Unreachable code\n"); break;
  }

exit:
  return res;
error:
  goto exit;
}
