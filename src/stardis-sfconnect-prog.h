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

#ifndef SDIS_SF_CONNECT_PROG_H
#define SDIS_SF_CONNECT_PROG_H

#include <rsys/rsys.h>
#include <rsys/str.h>

#include "stardis-prog.h"

struct stardis;
struct mem_allocator;

/*******************************************************************************
 * Solid-Fluid prog data
 ******************************************************************************/
struct solid_fluid_connect_prog {
  void* prog_data; /* result of the init_fluid() call */
  struct str name;
  struct str prog_name;
  struct str args;
  int is_outside; /* the fluid is used for a boundary */
  unsigned desc_id; /* id of the boundary; meaningful if is_outside */
  unsigned connection_id;
  /* lib handle and function ptrs */
  void* lib;
  void* (*create)(char*);
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
};

res_T
create_solver_sf_connect_prog
  (struct stardis* stardis,
   const struct solid_fluid_connect_prog* connect);

res_T
init_sf_connect_prog
  (struct mem_allocator* allocator,
   struct solid_fluid_connect_prog** dst);

void
release_sf_connect_prog
  (struct solid_fluid_connect_prog* connect,
   struct mem_allocator* allocator);

res_T
str_print_sf_connect_prog
  (struct str* str,
   const struct solid_fluid_connect_prog* connect);

double
sf_connect_prog_get_hmax
  (const struct solid_fluid_connect_prog* connect);

#endif
