/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef SDIS_SOLID_H
#define SDIS_SOLID_H

#include <rsys/rsys.h>

struct sdis_device;
struct sdis_medium;

/*******************************************************************************
 * Solid data
 ******************************************************************************/
struct solid {
  char name[32];
  double cp; /* Calorific capacity */
  double lambda; /* Conductivity */
  double rho; /* Volumic mass */
  double delta; /* Numerical parameter */
  double tinit; /* Initial temperature */
  double imposed_temperature; /* allow to impose a T; -1 if unset */
  double vpower;
  /* Compute mode */
  int is_green, is_outside;
  double t0; /* End time of tinit */
  /* ID */
  unsigned id;
};

extern res_T
create_solid
  (struct sdis_device* dev,
   const char* name,
   const double lambda,
   const double rho,
   const double cp,
   const double delta,
   const int is_green,
   const int is_outside,
   const double tinit,
   const double imposed_temperature,
   const double vpower,
   struct sdis_medium*** media_ptr,
   unsigned* out_id);

#endif
