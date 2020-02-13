/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef SDIS_FLUID_H
#define SDIS_FLUID_H

#include <sdis.h>
#include <rsys/rsys.h>

struct sdis_device;
struct sdis_medium;

/*******************************************************************************
 * Fluid data
 ******************************************************************************/
struct fluid {
  char name[32];
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
create_fluid
  (struct sdis_device* dev,
   const char* name,
   const double rho,
   const double cp,
   const int is_green,
   const int is_outside,
   const double tinit,
   const double imposed_temperature,
   struct sdis_medium*** media_ptr,
   unsigned* out_id);

#endif
