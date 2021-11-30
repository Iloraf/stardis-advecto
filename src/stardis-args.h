/* Copyright (C) 2018-2021 |Meso|Star> (contact@meso-star.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>. */

#ifndef STARDIS_ARGS_H
#define STARDIS_ARGS_H

#include <sdis.h>

#include <rsys/rsys.h>
#include <rsys/dynamic_array_str.h>

struct camera;
struct logger;
struct mem_allocator;
struct stardis;
struct dummies;

enum stardis_mode {
  /* Ordered so that print_multiple_modes() prints in alphabetical order */
  UNDEF_MODE = 0,
  MODE_DUMP_C_CHUNKS = BIT(0), /* -c */
  MODE_DUMP_PATHS = BIT(1), /* -D */
  MODE_DUMP_VTK = BIT(2), /* -d */
  MODE_EXTENDED_RESULTS = BIT(3), /* -e */
  MODE_FLUX_BOUNDARY_COMPUTE = BIT(4), /* -F */
  MODE_BIN_GREEN = BIT(5), /* -G */
  MODE_GREEN = BIT(6), /* -g */
  MODE_DUMP_HELP = BIT(7), /* -h */
  MODE_MEDIUM_COMPUTE = BIT(8), /* -m */
  MODE_PROBE_COMPUTE_ON_INTERFACE = BIT(9), /* -P */
  MODE_PROBE_COMPUTE = BIT(10), /* -p */
  MODE_IR_COMPUTE = BIT(11), /* -R */
  MODE_MAP_COMPUTE = BIT(12), /* -S */
  MODE_BOUNDARY_COMPUTE = BIT(13), /* -s */
  MODE_VERBOSITY = BIT(14), /* -V */
  MODE_DUMP_VERSION = BIT(15), /* -v */

  GREEN_COMPATIBLE_MODES
    = MODE_PROBE_COMPUTE | MODE_PROBE_COMPUTE_ON_INTERFACE | MODE_MEDIUM_COMPUTE
    | MODE_BOUNDARY_COMPUTE,

  SURFACE_COMPUTE_MODES
    = MODE_BOUNDARY_COMPUTE | MODE_FLUX_BOUNDARY_COMPUTE | MODE_MAP_COMPUTE,

  EXT_COMPATIBLE_MODES
    = GREEN_COMPATIBLE_MODES | MODE_MEDIUM_COMPUTE | MODE_FLUX_BOUNDARY_COMPUTE,

  REGION_COMPUTE_MODES = SURFACE_COMPUTE_MODES | MODE_MEDIUM_COMPUTE,

  COMPUTE_MODES = GREEN_COMPATIBLE_MODES | MODE_IR_COMPUTE | SURFACE_COMPUTE_MODES,

  EXCLUSIVE_MODES = COMPUTE_MODES,

  SHORT_EXIT_MODES = MODE_DUMP_HELP | MODE_DUMP_VERSION,

  USE_STDOUT_MODES
  = MODE_DUMP_C_CHUNKS | MODE_DUMP_VTK | MODE_DUMP_HELP | MODE_DUMP_VERSION
  | MODE_IR_COMPUTE | MODE_GREEN,

  RANDOM_RW_MODES
    = MODE_PROBE_COMPUTE | MODE_PROBE_COMPUTE_ON_INTERFACE | MODE_MEDIUM_COMPUTE
    | MODE_BOUNDARY_COMPUTE | MODE_FLUX_BOUNDARY_COMPUTE
};

STATIC_ASSERT(GREEN_COMPATIBLE_MODES == (COMPUTE_MODES & GREEN_COMPATIBLE_MODES), 
  Cannot_have_a_GREEN_COMPATIBLE_MODE_that_is_not_a_COMPUTE_MODE);

enum dump_path_type {
  DUMP_NONE = 0,
  DUMP_SUCCESS = BIT(0),
  DUMP_ERROR = BIT(1),
  DUMP_ALL = DUMP_SUCCESS | DUMP_ERROR
};

struct args {
  struct logger* logger;
  struct mem_allocator* allocator;
  struct darray_str model_files;
  char* medium_name;
  char* solve_filename;
  char* bin_green_filename;
  char* end_paths_filename;
  char* paths_filename;
  char* rndgen_state_in_filename;
  char* rndgen_state_out_filename;
  char* chunks_prefix;
  char* camera;
  size_t samples;
  double pos_and_time[5];
  unsigned nthreads;
  unsigned picard_order;
  enum stardis_mode mode;
  enum dump_path_type dump_paths;
  int verbose;
};

extern LOCAL_SYM res_T
init_args
  (struct logger* logger,
   struct mem_allocator* mem,
   struct args** args);

extern LOCAL_SYM void
release_args
  (struct args* args);

extern void
print_version
  (FILE* stream);

extern void
short_help
  (FILE* stream,
   const char* prog);

extern res_T
parse_args
  (const int argc,
   char** argv,
   struct args* args,
   struct mem_allocator* allocator);

extern res_T
parse_camera
  (struct logger* logger, 
   char* cam_param,
   struct stardis* stardis);

#endif /* STRADIS_ARGS_H */
