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

#ifndef STARDIS_DESCRIPTION_H
#define STARDIS_DESCRIPTION_H

#include "stardis-prog.h"

#include <rsys/rsys.h>
#include <rsys/dynamic_array.h>
#include <rsys/str.h>

#include <sdis.h>

#include <limits.h>

/* Forward declarations */
struct mem_allocator;

/* Different types of descriptions */
enum description_type {
  DESC_MAT_SOLID,
  DESC_MAT_SOLID_PROG,
  DESC_MAT_FLUID,
  DESC_MAT_FLUID_PROG,
  DESC_BOUND_H_FOR_FLUID,
  DESC_BOUND_H_FOR_FLUID_PROG,
  DESC_BOUND_H_FOR_SOLID,
  DESC_BOUND_H_FOR_SOLID_PROG,
  DESC_BOUND_T_FOR_SOLID,
  DESC_BOUND_T_FOR_SOLID_PROG,
  DESC_BOUND_F_FOR_SOLID,
  DESC_BOUND_F_FOR_SOLID_PROG,
  DESC_SOLID_FLUID_CONNECT,
  DESC_SOLID_FLUID_CONNECT_PROG,
  DESC_SOLID_SOLID_CONNECT,
  DESC_SOLID_SOLID_CONNECT_PROG,
  DESCRIPTION_TYPE_COUNT__,
  DESC_OUTSIDE
};

#define DESC_IS_H(D) \
  ((D)->type == DESC_BOUND_H_FOR_SOLID || (D)->type == DESC_BOUND_H_FOR_FLUID \
    || (D)->type == DESC_BOUND_H_FOR_SOLID_PROG \
    || (D)->type == DESC_BOUND_H_FOR_FLUID_PROG)
#define DESC_IS_T(D) \
  ((D)->type == DESC_BOUND_T_FOR_SOLID \
    || (D)->type == DESC_BOUND_T_FOR_SOLID_PROG)
#define DESC_IS_F(D) \
  ((D)->type == DESC_BOUND_F_FOR_SOLID \
    || (D)->type == DESC_BOUND_F_FOR_SOLID_PROG)
#define DESC_IS_SOLID(D) \
  ((D)->type == DESC_MAT_SOLID || (D)->type == DESC_MAT_SOLID_PROG)
#define DESC_IS_FLUID(D) \
  ((D)->type == DESC_MAT_FLUID || (D)->type == DESC_MAT_FLUID_PROG)
#define DESC_IS_MEDIUM(D) \
  (DESC_IS_SOLID(D) || DESC_IS_FLUID(D))
#define DESC_IS_BOUNDARY(D) \
 (DESC_IS_H(D) || DESC_IS_T(D) || DESC_IS_F(D))
#define DESC_IS_SOLID_FLUID(D) \
  ((D)->type == DESC_SOLID_FLUID_CONNECT \
    || (D)->type == DESC_SOLID_FLUID_CONNECT_PROG)
#define DESC_IS_SOLID_SOLID(D) \
  ((D)->type == DESC_SOLID_SOLID_CONNECT \
    || (D)->type == DESC_SOLID_SOLID_CONNECT_PROG)

/******************************************************************************/

struct fluid;
struct fluid_prog;
struct solid;
struct solid_prog;
struct t_boundary;
struct t_boundary_prog;
struct f_boundary;
struct f_boundary_prog;
struct h_boundary;
struct h_boundary_prog;
struct solid_fluid_connect;
struct solid_fluid_connect_prog;
struct solid_solid_connect;
struct solid_solid_connect_prog;

struct description {
  enum description_type type;
  union {
    struct fluid* fluid;
    struct fluid_prog* fluid_prog;
    struct solid* solid;
    struct solid_prog* solid_prog;
    struct t_boundary* t_boundary;
    struct t_boundary_prog* t_boundary_prog;
    struct f_boundary* f_boundary;
    struct f_boundary_prog* f_boundary_prog;
    struct h_boundary* h_boundary;
    struct h_boundary_prog* h_boundary_prog;
    struct solid_fluid_connect* sf_connect;
    struct solid_fluid_connect_prog* sf_connect_prog;
    struct solid_solid_connect* ss_connect;
    struct solid_solid_connect_prog* ss_connect_prog;
  } d;
};

/******************************************************************************/

struct h_boundary {
  struct str name;
  double ref_temperature;
  double emissivity;
  double specular_fraction;
  double hc;
  double imposed_temperature;
  unsigned mat_id;
  struct fluid* possible_external_fluid; /* if H for solid */
};

res_T
init_h
  (struct mem_allocator* allocator,
   struct h_boundary** dst);

void
release_h_boundary
  (struct h_boundary* bound,
   struct mem_allocator* allocator);

res_T
str_print_h_boundary
  (struct str* str,
   const struct description* desc);

struct t_boundary {
  struct str name;
  double imposed_temperature;
  unsigned mat_id;
};

res_T
init_t
  (struct mem_allocator* allocator,
   struct t_boundary** dst);

void
release_t_boundary
  (struct t_boundary* bound,
   struct mem_allocator* allocator);

res_T
str_print_t_boundary
  (struct str* str,
   const struct t_boundary* b);

struct f_boundary {
  struct str name;
  double imposed_flux;
  unsigned mat_id;
};

res_T
init_f
  (struct mem_allocator* allocator,
   struct f_boundary** dst);

void
release_f_boundary
  (struct f_boundary* bound,
   struct mem_allocator* allocator);

res_T
str_print_f_boundary
  (struct str* str,
   const struct f_boundary* b);

struct solid_fluid_connect {
  struct str name;
  double ref_temperature;
  double emissivity;
  double specular_fraction;
  double hc;
  unsigned connection_id;
};

void
release_sf_connect
  (struct solid_fluid_connect* connect,
   struct mem_allocator* allocator);

res_T
init_sf
  (struct mem_allocator* allocator,
   struct solid_fluid_connect** dst);

res_T
str_print_sf_connect
  (struct str* str,
   const struct solid_fluid_connect* c);

struct solid_solid_connect {
  struct str name;
  double tcr;
  unsigned connection_id;
};

void
release_ss_connect
  (struct solid_solid_connect* connect,
   struct mem_allocator* allocator);

res_T
init_ss
  (struct mem_allocator* allocator,
   struct solid_solid_connect** dst);

res_T
str_print_ss_connect
  (struct str* str,
   const struct solid_solid_connect* c);

res_T
init_description
  (struct mem_allocator* alloc,
   struct description* desc);

void
release_description
  (struct description* desc,
   struct mem_allocator* allocator);

res_T
str_print_description
  (struct str* str,
   const unsigned rank,
   const struct description* desc);

const struct str*
get_description_name
  (const struct description* desc);

void
description_get_medium_id
  (const struct description* desc,
   unsigned* id);

#endif
