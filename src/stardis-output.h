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
struct sdis_scene;
struct mem_allocator;
struct sdis_estimator;
struct stardis;
struct geometry;
struct vertex;

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
   const struct description* descriptions,
   const double radiative_temps[2]);

extern res_T
create_edge_file_if
  (struct sdis_scene* scn,
   struct mem_allocator* allocator);

extern res_T
print_single_MC_result
  (const int mode,
   struct sdis_estimator* estimator,
   struct stardis* stardis);

extern void
dump_vtk
  (const struct geometry* geometry);

extern void
dump_map
  (const struct stardis* stardis,
   struct sdis_estimator** estimators);

extern void
print_trg_as_obj
  (FILE* stream,
   const struct vertex* vertices,
   const unsigned* indices);

#endif
