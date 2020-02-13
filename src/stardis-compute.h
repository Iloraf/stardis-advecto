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

struct counts {
  size_t smed_count, fmed_count, tbound_count, hbound_count,
    fbound_count, sfconnect_count;
};
#define COUNTS_NULL__ {\
  0, 0, 0, 0, 0, 0\
 }
static const struct counts COUNTS_NULL = COUNTS_NULL__;

extern res_T
stardis_compute
  (struct stardis* stardis,
   const int mode);

#endif