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

/* This header file is intended for inclusion in shared libraries defining
 * programmed descriptions used in stardis model files.
 * Please refer to stardis(1) and stardis-input(5) man pages for additional
 * information. */

/*****************************************************************************/
/* API types.                                                                */
/* The various functions defining programmed descriptions receive arguments  */
/* of the following types when called from the stardis simulation.           */
/*****************************************************************************/

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



/*****************************************************************************/
/* Expected function declarations (sorted by description type).              */
/* Some functions appear multiple times as they are part of more than one    */
/* description requirement.                                                  */
/*****************************************************************************/

/******************************************************/
/* Mandatory functions for any programmed description */
/******************************************************/

/* Create the data attached to a given description.
 * A NULL result is interpreted as an error and ends the program. */
extern void*
stardis_create_data
  (char* args);

/* Release the data created by stardis_create_data. */
extern void
stardis_release_data
  (void* data);

/* Get the copyright notice */
const char*
get_copyright_notice
  (void*);

/* Get short (name and link to?) version of the license */
const char*
get_license_short
  (void*);

/* Get full license text */
const char*
get_license_text
  (void*);

/**********************************************/
/* Mandatory functions for a programmed solid */
/**********************************************/

/* Returns the calorific capacity at a given vertex.
 * This functions is called at every vertex of every path of the computation
 * crossing this solid.
 * Data is the pointer returned by stardis_create_data for this solid. */
extern double
stardis_calorific_capacity
  (const struct stardis_vertex* vtx,
   void* data);

/* Returns the volumic mass at a given vertex.
 * This functions is called at every vertex of every path of the computation
 * crossing this solid.
 * Data is the pointer returned by stardis_create_data for this solid. */
extern double
stardis_volumic_mass
  (const struct stardis_vertex* vtx,
   void* data);

/* Returns the conductivity at a given vertex.
 * This functions is called at every vertex of every path of the computation
 * crossing this solid.
 * Data is the pointer returned by stardis_create_data for this solid. */
extern double
stardis_conductivity
  (const struct stardis_vertex* vtx,
   void* data);

/* Returns the delta numerical parameter at a given vertex.
 * This functions is called at every vertex of every path of the computation
 * crossing this solid.
 * Data is the pointer returned by stardis_create_data for this solid. */
extern double
stardis_delta_solid
  (const struct stardis_vertex* vtx,
   void* data);

/* Returns the volumic power at a given vertex.
 * This functions is called at every vertex of every path of the computation
 * crossing this solid.
 * Data is the pointer returned by stardis_create_data for this solid. */
extern double
stardis_volumic_power
  (const struct stardis_vertex* vtx,
   void* data);

/* Returns the temperature at a given vertex.
 * If the temperature is not known/imposed the expected return value is -1.
 * This functions is called at every vertex of every path of the computation
 * crossing this solid.
 * Data is the pointer returned by stardis_create_data for this solid. */
extern double
stardis_medium_temperature
  (const struct stardis_vertex* vtx,
   void* data);

/* Returns the expected temperature range for this fluid.
 * This functions is called once when initializing the computation.
 * Data is the pointer returned by stardis_create_data for this solid. */
extern double*
stardis_t_range
  (void* data,
   double range[2]);

/**********************************************/
/* Mandatory functions for a programmed fluid */
/**********************************************/

/* Returns the calorific capacity at a given vertex.
 * This functions is called at every vertex of every path of the computation
 * crossing this fluid.
 * Data is the pointer returned by stardis_create_data for this fluid. */
extern double
stardis_calorific_capacity
  (const struct stardis_vertex* vtx,
   void* data);

/* Returns the volumic mass at a given vertex.
 * This functions is called at every vertex of every path of the computation
 * crossing this fluid.
 * Data is the pointer returned by stardis_create_data for this fluid. */
extern double
stardis_volumic_mass
  (const struct stardis_vertex* vtx,
   void* data);

/* Returns the temperature at a given vertex.
 * If the temperature is not known/imposed the expected return value is -1.
 * This functions is called at every vertex of every path of the computation
 * crossing this fluid.
 * Data is the pointer returned by stardis_create_data for this fluid. */
extern double
stardis_medium_temperature
  (const struct stardis_vertex* vtx,
   void* data);

/* Returns the expected temperature range for this solid.
 * This functions is called once when initializing the computation.
 * Data is the pointer returned by stardis_create_data for this fluid. */
extern double*
stardis_t_range
  (void* data,
   double range[2]);

/***************************************************************/
/* Mandatory functions for a programmed H boundary for a fluid */
/***************************************************************/

/* Returns the boundary temperature at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this boundary.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_boundary_temperature
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the emissivity at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this boundary.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_emissivity
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the specular fraction at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this boundary.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_specular_fraction
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the convection coefficient at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this boundary.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_convection_coefficient
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the reference temperature at a given fragment.
 * This temperature is used as a reference to linearize radiative transfer
 * in Picard computations.
 * This functions is called every time a path of the computation reaches
 * this boundary.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_reference_temperature
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the upper bound of the convection coefficient accross this boundary.
 * This functions is called once when initializing the computation.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_max_convection_coefficient
  (void* data);

/* Returns the expected temperature range for this boundary.
 * This functions is called once when initializing the computation.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double*
stardis_t_range
  (void* data,
   double range[2]);

/***************************************************************/
/* Mandatory functions for a programmed H boundary for a solid */
/***************************************************************/

/* Returns the emissivity at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this boundary.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_emissivity
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the specular fraction at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this boundary.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_specular_fraction
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the convection coefficient at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this boundary.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_convection_coefficient
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the reference temperature at a given fragment.
 * This temperature is used as a reference to linearize radiative transfer
 * in Picard computations.
 * This functions is called every time a path of the computation reaches
 * this boundary.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_reference_temperature
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the temperature at a given vertex.
 * The intent is to return the temperature in an implicit fluid enclosing this
 * solid.
 * This functions is called at every vertex of every path of the computation
 * crossing this fluid.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_medium_temperature
  (const struct stardis_vertex* vtx,
   void* data);

/* Returns the upper bound of the convection coefficient accross this boundary.
 * This functions is called once when initializing the computation.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_max_convection_coefficient
  (void* data);

/* Returns the expected temperature range for this boundary.
 * This functions is called once when initializing the computation.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double*
stardis_t_range
  (void* data,
   double range[2]);

/***************************************************/
/* Mandatory functions for a programmed T boundary */
/***************************************************/

/* Returns the boundary temperature at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this boundary.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_boundary_temperature
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the expected temperature range for this boundary.
 * This functions is called once when initializing the computation.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double*
stardis_t_range
  (void* data,
   double range[2]);

/***************************************************/
/* Mandatory functions for a programmed F boundary */
/***************************************************/

/* Returns the flux at the boundary at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this boundary.
 * Data is the pointer returned by stardis_create_data for this boundary. */
extern double
stardis_boundary_flux
  (const struct stardis_interface_fragment* frag,
   void* data);

/***************************************************************/
/* Mandatory functions for a programmed Solid-Solid connection */
/***************************************************************/

/* Returns the thermal contact resistance at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this connection.
 * Data is the pointer returned by stardis_create_data for this connection. */
extern double
stardis_thermal_contact_resistance
  (const struct stardis_interface_fragment* frag,
   void* data);

/***************************************************************/
/* Mandatory functions for a programmed Solid-Fluid connection */
/***************************************************************/

/* Returns the emissivity at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this connection.
 * Data is the pointer returned by stardis_create_data for this connection. */
extern double
stardis_emissivity
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the specular fraction at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this connection.
 * Data is the pointer returned by stardis_create_data for this connection. */
extern double
stardis_specular_fraction
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the convection coefficient at a given fragment.
 * This functions is called every time a path of the computation reaches
 * this connection.
 * Data is the pointer returned by stardis_create_data for this connection. */
extern double
stardis_convection_coefficient
  (const struct stardis_interface_fragment* frag,
   void* data);

/* Returns the temperature at a given vertex.
 * The intent is to return the temperature in an implicit fluid enclosing this
 * solid.
 * This functions is called at every vertex of every path of the computation
 * crossing this fluid.
 * Data is the pointer returned by stardis_create_data for this connection. */
extern double
stardis_medium_temperature
  (const struct stardis_vertex* vtx,
   void* data);

/* Returns the upper bound of the convection coefficient accross this connection.
 * This functions is called once when initializing the computation.
 * Data is the pointer returned by stardis_create_data for this connection. */
extern double
stardis_max_convection_coefficient
  (void* data);

/* Returns the expected temperature range for this connection.
 * This functions is called once when initializing the computation.
 * Data is the pointer returned by stardis_create_data for this connection. */
extern double*
stardis_t_range
  (void* data,
   double range[2]);

#endif

