/* Copyright (C) 2018-2020 |Meso|Star> (contact@meso-star.com)
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

#ifndef SDIS_FLUID_H
#define SDIS_FLUID_H

#include <sdis.h>

#include <rsys/rsys.h>
#include <rsys/str.h>

struct stardis;

/*******************************************************************************
 * Fluid data
 ******************************************************************************/
struct fluid {
  struct str name;
  double cp; /* Calorific capacity */
  double rho; /* Volumic mass */
  /* Compute mode */
  int is_green, is_outside;
  double t0; /* End time of tinit */
  double tinit;
  double imposed_temperature;
  /* ID */
  unsigned id;
};

extern res_T
create_stardis_fluid
  (struct stardis* stardis,
   const struct str* name,
   const double rho,
   const double cp,
   const int is_green,
   const int is_outside,
   const double tinit,
   const double imposed_temperature,
   unsigned* out_id);

#endif
