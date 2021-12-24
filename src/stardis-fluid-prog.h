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

#ifndef SDIS_PROG_H
#define SDIS_PROG_H

#include <sdis.h>

#include <rsys/rsys.h>
#include <rsys/str.h>

struct stardis;

/*******************************************************************************
 * Fluid prog data
 ******************************************************************************/
struct fluid_prog {
  /* This field must come first as a struct fluid_prog* must be usable as 
   * an struct custom_data** */
  void* prog_data; /* result of the init_fluid() call */
  struct str name;
  struct str prog_name;
  struct str args;
  int is_outside; /* the fluid is used for a boundary */
  unsigned desc_id; /* id of the boundary; meaningful if is_outside */
  unsigned fluid_id;
  /* lib handle and function ptrs */
  void* lib;
  double (*cp)(const struct sdis_rwalk_vertex*, struct sdis_data*);
  double (*rho)(const struct sdis_rwalk_vertex*, struct sdis_data*);
  double (*temp)(const struct sdis_rwalk_vertex*, struct sdis_data*);
  void* (*create)();
  int (*init)(void*, const char*);
  void (*release)(void*);
};

res_T
create_solver_fluid_prog
  (struct stardis* stardis,
   const struct fluid_prog* fluid_props);

LOCAL_SYM res_T
init_fluid_prog(struct mem_allocator* allocator, struct fluid_prog** dst);

LOCAL_SYM void
release_fluid_prog
  (struct fluid_prog* fluid,
   struct mem_allocator* allocator);

LOCAL_SYM res_T
str_print_fluid_prog(struct str* str, const struct fluid_prog* s);

#endif
