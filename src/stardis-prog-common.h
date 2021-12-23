
/* Copyright (C) 2018-2021 |Meso|Star> (contact@meso-star.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>. */

#ifndef STARDIS_PROG_COMMON_H__
#define STARDIS_PROG_COMMON_H__

enum stardis_return_codes {
  STARDIS_SUCCESS,
  STARDIS_FAILURE
};

struct walk_vertex {
  double P[3]; /* World space position */
  double time; /* "Time" of the vertex */
};

#endif

