/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef SDIS_COMPUTE_H
#define SDIS_COMPUTE_H

#include <rsys/rsys.h>
#include <rsys/hash_table.h>

#define HTABLE_NAME weigth
#define HTABLE_DATA double
#define HTABLE_KEY unsigned
#include <rsys/hash_table.h>

struct stardis;
struct sdis_medium;

#define DARRAY_NAME media_ptr
#define DARRAY_DATA struct sdis_medium*
#include <rsys/dynamic_array.h>

#define DARRAY_NAME estimators
#define DARRAY_DATA struct sdis_estimator*
#include <rsys/dynamic_array.h>

struct counts {
  size_t smed_count, fmed_count, tbound_count, hbound_count,
    fbound_count, sfconnect_count;
};
#define COUNTS_NULL__ {\
  0, 0, 0, 0, 0, 0\
 }
static const struct counts COUNTS_NULL = COUNTS_NULL__;

extern LOCAL_SYM res_T
stardis_compute
  (struct stardis* stardis,
   const int mode);

extern LOCAL_SYM res_T
read_compute_region(struct stardis* stardis);

#endif