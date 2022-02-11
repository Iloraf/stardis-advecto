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

#ifndef SDIS_COMPUTE_H
#define SDIS_COMPUTE_H

#include <rsys/rsys.h>
#include <rsys/dynamic_array.h>

struct stardis;
struct time;
struct str;

#define DARRAY_NAME estimators
#define DARRAY_DATA struct sdis_estimator*
#include <rsys/dynamic_array.h>

struct sdis_medium*
find_medium_by_name
  (struct stardis* stardis,
   const struct str* name,
   unsigned* id); /* Can be NULL */

extern LOCAL_SYM res_T
stardis_compute
  (struct stardis* stardis,
   struct time* start);

extern LOCAL_SYM res_T
read_compute_surface
  (struct stardis* stardis);

#endif