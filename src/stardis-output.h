/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef SDIS_OUTPUT_H
#define SDIS_OUTPUT_H

#include <rsys/rsys.h>
#include <stdio.h>

struct sdis_green_path;
struct sdis_heat_path;
struct sdis_estimator_buffer;
struct sdis_green_function;
struct counts;
struct description;
struct mem_allocator;
struct sdis_estimator;
struct stardis;
struct geometry;
struct vertex;
struct darray_descriptions;
struct darray_estimators;

extern res_T
print_sample
  (struct sdis_green_path* path,
   void* ctx);

extern res_T
dump_image
  (const struct sdis_estimator_buffer* buf);

extern res_T
dump_green
  (struct sdis_green_function* green,
   const struct counts* counts,
   const struct darray_descriptions* descriptions,
   const double radiative_temps[2]);

extern int
has_holes(const struct stardis* stardis);

extern res_T
dump_holes_at_the_end_of_vtk
  (const struct stardis* stardis,
   FILE* stream);

extern res_T
print_single_MC_result
  (const int mode,
   struct sdis_estimator* estimator,
   struct stardis* stardis);

extern void
dump_map
  (const struct stardis* stardis,
   const struct darray_estimators* estimators);

extern res_T
dump_compute_region_at_the_end_of_vtk
  (const struct stardis* stardis,
   FILE* stream);

#endif
