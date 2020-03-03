/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef SDIS_INTFACE_H
#define SDIS_INTFACE_H

#include <rsys/hash_table.h>

#include <limits.h>

struct sdis_data;
struct sdis_device;
struct geometry;
struct description;
struct sdis_medium;
struct darray_descriptions;
struct darray_media_ptr;

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

static INLINE size_t
hash_desc(struct int_descs const* key)
{
  return (size_t)hash_fnv64(key, 3 * sizeof(unsigned));
}

#define HTABLE_NAME intface
#define HTABLE_DATA struct sdis_interface*
#define HTABLE_KEY struct int_descs
#define HTABLE_KEY_FUNCTOR_EQ eq_desc
#define HTABLE_KEY_FUNCTOR_HASH hash_desc
#include <rsys/hash_table.h>

extern res_T
create_intface
  (struct sdis_device* dev,
   unsigned tr_idx,
   struct htable_intface* htable_interfaces,
   struct geometry* geometry,
   const struct darray_descriptions* descriptions,
   struct darray_media_ptr* media);

#endif
