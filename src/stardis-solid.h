/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef SDIS_SOLID_H
#define SDIS_SOLID_H

#include <rsys/rsys.h>

struct te_expr;
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
  /* Compute mode */
  int is_green, is_outside;
  double t0;
  /* TinyExpr stuff to compute temperature & power */
  struct te_expr *temp;
  struct te_expr *t_init;
  struct te_expr *power;
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
   const char* tinit_expr, /* Can be NULL if not used by getter */
   const char* t_expr, /* Can be NULL if not used by getter */
   const char* power_expr, /* Can be NULL */
   struct sdis_medium*** media_ptr,
   int* out_has_power, /* Can be NULL */
   unsigned* out_id);

#endif
