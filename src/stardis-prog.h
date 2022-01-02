
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

#ifndef STARDIS_PROG_H__
#define STARDIS_PROG_H__

enum stardis_return_code {
  STARDIS_SUCCESS,
  STARDIS_FAILURE
};

struct stardis_walk_vertex {
  double P[3]; /* World space position */
  double time; /* "Time" of the vertex */
};

enum stardis_side {
  FRONT,
  BACK
};

struct stardis_interface_fragment {
  double P[3]; /* World space position */
  double Ng[3]; /* Normalized world space geometry normal at the interface */
  double uv[2]; /* Parametric coordinates of the interface */
  double time; /* Current time */
  enum stardis_side side;
};

#endif

