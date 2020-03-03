/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef SDIS_FLUID_H
#define SDIS_FLUID_H

#include <sdis.h>

#include <rsys/rsys.h>
#include <rsys/str.h>

struct sdis_device;
struct darray_media_ptr;

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
create_fluid
  (struct sdis_device* dev,
   struct mem_allocator* allocator,
   const struct str* name,
   const double rho,
   const double cp,
   const int is_green,
   const int is_outside,
   const double tinit,
   const double imposed_temperature,
   struct darray_media_ptr* media_ptr,
   unsigned* out_id);

#endif
