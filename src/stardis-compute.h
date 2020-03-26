/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef SDIS_COMPUTE_H
#define SDIS_COMPUTE_H

#include <rsys/rsys.h>
#include <rsys/dynamic_array.h>

struct stardis;
struct str;

#define DARRAY_NAME estimators
#define DARRAY_DATA struct sdis_estimator*
#include <rsys/dynamic_array.h>

struct sdis_medium*
find_medium_by_name
  (struct stardis* stardis,
   const struct str* name,
   unsigned* id); /* Can be NULL */

extern LOCAL_SYM res_T
stardis_compute
  (struct stardis* stardis);

extern LOCAL_SYM res_T
read_compute_surface
  (struct stardis* stardis);

#endif