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

#ifndef SDIS_SOLID_PROG_H
#define SDIS_SOLID_PROG_H

#include <rsys/rsys.h>
#include <rsys/str.h>

#include "stardis-prog.h"

struct stardis;
struct sdis_rwalk_vertex;
struct mem_allocator;

/*******************************************************************************
 * Solid prog data
 ******************************************************************************/
struct solid_prog {
  void* prog_data; /* result of the init_solid() call */
  struct str name;
  struct str prog_name;
  struct str args;
  int is_outside; /* the solid is used for a boundary */
  unsigned desc_id; /* id of the boundary; meaningful if is_outside */
  unsigned solid_id;
  /* lib handle and function ptrs */
  void* lib;
  void* (*create)();
  enum stardis_return_code (*init)(void*, const char*);
  void (*release)(void*);
  double (*lambda)(const struct sdis_rwalk_vertex*, void*);
  double (*rho)(const struct sdis_rwalk_vertex*, void*);
  double (*cp)(const struct sdis_rwalk_vertex*, void*);
  double (*delta)(const struct sdis_rwalk_vertex*, void*);
  double (*temp)(const struct sdis_rwalk_vertex*, void*);
  double (*vpower)(const struct sdis_rwalk_vertex*, void*);
  double* (*t_range)(void*, double trange[2]);
};

res_T
create_solver_solid_prog
  (struct stardis* stardis,
   const struct solid_prog* solid_props);

res_T
init_solid_prog
  (struct mem_allocator* allocator,
   struct solid_prog** dst);

void
release_solid_prog
  (struct solid_prog* solid,
   struct mem_allocator* allocator);

res_T
str_print_solid_prog
  (struct str* str,
   const struct solid_prog* s);

#endif
