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

#include "stardis-app.h"
#include "stardis-hbound-prog.h"
#include "stardis-fluid-prog.h"
#include "stardis-prog.h"
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
init_h_boundary_prog
  (struct mem_allocator* allocator,
   struct h_boundary_prog** dst)
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
  (*dst)->mat_id = UINT_MAX;
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
release_h_boundary_prog
  (struct h_boundary_prog* bound,
   struct mem_allocator* allocator)
{
  ASSERT(bound && allocator);
  str_release(&bound->name);
  str_release(&bound->prog_name);
  str_release(&bound->args);
  if(bound->prog_data)
    bound->release(bound->prog_data);
  if(bound->possible_external_fluid)
    release_fluid_prog(bound->possible_external_fluid, allocator);
  MEM_RM(allocator, bound);
}

res_T
str_print_h_boundary_prog
  (struct str* str,
   const struct description* desc)
{
  res_T res = RES_OK;
  const struct h_boundary_prog* b;
  ASSERT(str && desc && DESC_IS_H(desc));
  b = desc->d.h_boundary_prog;
  ERR(str_append_printf(str,
    "programmed H boundary for %s '%s': lib='%s', args=[%s] "
    "(using medium %u as external medium)",
    (desc->type == DESC_BOUND_H_FOR_SOLID_PROG ? "solid" : "fluid"),
    str_cget(&b->name), str_cget(&b->prog_name), str_cget(&b->args),
    b->mat_id));
end:
  return res;
error:
  goto end;
}

double
h_bound_prog_get_hmax
  (struct h_boundary_prog* h_boundary_props)
{
  return h_boundary_props->hmax(h_boundary_props->prog_data);
}
