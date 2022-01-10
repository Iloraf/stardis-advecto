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

struct stardis_vertex {
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

extern void*
stardis_create_data
  (char* args);

extern void
stardis_release_data
  (void* data);

extern double
stardis_calorific_capacity
  (const struct stardis_vertex* vtx,
   void* data);

extern double
stardis_volumic_mass
  (const struct stardis_vertex* vtx,
   void* data);

extern double
stardis_emissivity
  (const struct stardis_interface_fragment* frag,
   void* data);

extern double
stardis_specular_fraction
  (const struct stardis_interface_fragment* frag,
   void* data);

extern double
stardis_conductivity
  (const struct stardis_vertex* vtx,
   void* data);

extern double
stardis_delta_solid
  (const struct stardis_vertex* vtx,
   void* data);

extern double
stardis_volumic_power
  (const struct stardis_vertex* vtx,
   void* data);

extern double
stardis_boundary_temperature
  (const struct stardis_interface_fragment* frag,
   void* data);

extern double
stardis_boundary_flux
  (const struct stardis_interface_fragment* frag,
   void* data);

extern double
stardis_medium_temperature
  (const struct stardis_vertex* vtx,
   void* data);

extern double
stardis_convection_coefficient
  (const struct stardis_interface_fragment* frag,
   void* data);

extern double
stardis_reference_temperature
  (const struct stardis_interface_fragment* frag,
   void* data);

extern double
stardis_thermal_contact_resistance
  (const struct stardis_interface_fragment* frag,
   void* data);

extern double
stardis_max_convection_coefficient
  (void* data);

extern double*
stardis_t_range
  (void* data,
   double range[2]);

#endif

