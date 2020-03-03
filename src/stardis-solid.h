/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef SDIS_SOLID_H
#define SDIS_SOLID_H

#include <rsys/rsys.h>
#include <rsys/str.h>

struct sdis_device;
struct darray_media_ptr;

/*******************************************************************************
 * Solid data
 ******************************************************************************/
struct solid {
  struct str name;
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
   struct mem_allocator* allocator,
   const struct str* name,
   const double lambda,
   const double rho,
   const double cp,
   const double delta,
   const int is_green,
   const int is_outside,
   const double tinit,
   const double imposed_temperature,
   const double vpower,
   struct darray_media_ptr* media_ptr,
   unsigned* out_id);

#endif
