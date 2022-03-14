/* Copyright (C) 2018-2022 |Meso|Star> (contact@meso-star.com)
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
#include "stardis-sfconnect-prog.h"
#include "stardis-prog-properties.h"
#include "stardis-intface.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/str.h>

#include <sdis.h>

#include <limits.h>

/*******************************************************************************
 * Public Functions
 ******************************************************************************/
res_T
init_sf_connect_prog
  (struct mem_allocator* allocator,
   struct solid_fluid_connect_prog** dst)
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
  (*dst)->connection_id = UINT_MAX;
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
release_sf_connect_prog
  (struct solid_fluid_connect_prog* connect,
   struct mem_allocator* allocator)
{
  ASSERT(connect && allocator);
  str_release(&connect->name);
  str_release(&connect->prog_name);
  str_release(&connect->args);
  if(connect->prog_data)
    connect->release(connect->prog_data);
  /* library_close call is managed at lib_data level */
  MEM_RM(allocator, connect);
}

res_T
str_print_sf_connect_prog
  (struct str* str,
   const struct solid_fluid_connect_prog* connect)
{
  res_T res = RES_OK;
  ASSERT(str && connect);
  ERR(str_append_printf(str,
    "programmed Solid-Fluid connection '%s': lib='%s', args=[%s]",
    str_cget(&connect->name), str_cget(&connect->prog_name),
    str_cget(&connect->args)));
end:
  return res;
error:
  goto end;
}

double
sf_connect_prog_get_hmax
  (const struct solid_fluid_connect_prog* connect)
{
  return connect->hmax(connect->prog_data);
}

