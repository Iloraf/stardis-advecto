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

#ifndef SDIS_HBOUND_PROG_H
#define SDIS_HBOUND_PROG_H

#include <rsys/rsys.h>
#include <rsys/str.h>

#include "stardis-prog.h"

struct mem_allocator;
struct fluid_prog;
struct description;

/*******************************************************************************
 * H boundary prog data
 ******************************************************************************/
struct h_boundary_prog {
  void* prog_data; /* result of the init_h_prog() call */
  struct str name;
  struct str prog_name;
  struct str args;
  /* lib handle and function ptrs */
  void* lib;
  void* (*create)(void*, char*);
  void (*release)(void*);
  const char* (*get_copyright_notice)(void*);
  const char* (*get_license_short)(void*);
  const char* (*get_license_text)(void*);
  double (*ref_temp)(const struct stardis_interface_fragment*, void*);
  double (*emissivity)(const struct stardis_interface_fragment*, void*);
  double (*alpha)(const struct stardis_interface_fragment*, void*);
  double (*hc)(const struct stardis_interface_fragment*, void*);
  double (*hmax)(void*);
  double* (*t_range)(void*, double trange[2]);
  /* for h for solid */
  double (*boundary_temp)(const struct stardis_interface_fragment*, void*);
  /* for h for fluid */
  double (*fluid_temp)(const struct stardis_vertex*, void*);
  unsigned mat_id;
  struct fluid_prog* possible_external_fluid; /* if H for solid */
};

res_T
init_h_boundary_prog
  (struct mem_allocator* allocator,
   struct h_boundary_prog** dst);

void
release_h_boundary_prog
  (struct h_boundary_prog* bound,
   struct mem_allocator* allocator);

res_T
str_print_h_boundary_prog
  (struct str* str,
   const struct description* bound);

double
h_bound_prog_get_hmax
  (struct h_boundary_prog* h_boundary_props);

#endif
