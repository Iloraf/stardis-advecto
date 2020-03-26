/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef SDIS_INTFACE_H
#define SDIS_INTFACE_H

#include <rsys/hash_table.h>

#include <limits.h>

struct stardis;
struct dummies;

/*******************************************************************************
 * Interface data
 ******************************************************************************/
struct intface {
  double hc;
  double emissivity;
  double alpha;
  /* Imposed compute temperature & flux */
  double imposed_temperature;
  double imposed_flux;
  /* IDs */
  unsigned front_boundary_id, back_boundary_id;
};

/* Declare the hash table that map an interface to its descriptor */
struct int_descs {
  unsigned front, back, connect;
};
#define INT_DESCS_NULL__ { UINT_MAX, UINT_MAX, UINT_MAX }
static const struct int_descs INT_DESCS_NULL = INT_DESCS_NULL__;

static INLINE char
eq_desc(const struct int_descs* a, const struct int_descs* b)
{
  return (char)(a->front == b->front && a->back == b->back
    && a->connect == b->connect);
}

#define HTABLE_NAME intface
#define HTABLE_DATA struct sdis_interface*
#define HTABLE_KEY struct int_descs
#define HTABLE_KEY_FUNCTOR_EQ eq_desc
#include <rsys/hash_table.h>

extern res_T
create_intface
  (struct stardis* stardis,
   unsigned tr_idx,
   struct htable_intface* htable_interfaces);

#endif
