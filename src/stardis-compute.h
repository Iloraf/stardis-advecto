/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef SDIS_COMPUTE_H
#define SDIS_COMPUTE_H

#include <rsys/rsys.h>
#include <rsys/hash_table.h>

#define HTABLE_NAME weigth
#define HTABLE_DATA double
#define HTABLE_KEY unsigned
#include <rsys/hash_table.h>

struct te_expr;
struct stardis;
enum stardis_mode;

struct counts {
  size_t smed_count, fmed_count, tbound_count, hbound_count,
    fbound_count, sfconnect_count;
};
#define COUNTS_NULL__ {\
  0, 0, 0, 0, 0, 0\
 }
static const struct counts COUNTS_NULL = COUNTS_NULL__;

/* A type to limit variable use in tinyexpr expressions */
enum var_prohibited_t {
  ALL_VARS_ALLOWED = 0,
  T_PROHIBITED = BIT(4),
  XYZ_PROHIBITED = BIT(5),
  NO_VAR_ALLOWED = T_PROHIBITED | XYZ_PROHIBITED
};

extern res_T
compile_expr_to_fn
  (struct te_expr** f,
   const char* math_expr,
   const int prohibited,
   int* is_zero);

extern res_T
stardis_compute
  (struct stardis* stardis,
   enum stardis_mode mode);

#endif