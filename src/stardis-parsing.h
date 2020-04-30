/* Copyright (C) 2018-2020 |Meso|Star> (contact@meso-star.com)
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

#ifndef STARDIS_PARSING_H
#define STARDIS_PARSING_H

#include <sdis.h>

#include <rsys/rsys.h>
#include <rsys/dynamic_array_str.h>

#include <star/sstl.h>

struct camera;
struct logger;
struct mem_allocator;
struct stardis;

/* Utility macros */
#define CHK_TOK(x, Name) if((tk = (x)) == NULL) {\
   logger_print(stardis->logger, LOG_ERROR,\
     "Invalid data (missing token '" Name "')\n");\
   res = RES_BAD_ARG;\
   goto error;\
  } (void)0

#ifdef COMPILER_CL
#define strtok_r strtok_s
#endif

enum stardis_mode {
  /* Ordered so that print_multiple_modes() prints in alphabetical order */
  UNDEF_MODE = 0,
  MODE_DUMP_PATHS = BIT(0), /* -D */
  MODE_DUMP_VTK = BIT(1), /* -d */
  MODE_EXTENDED_RESULTS = BIT(2), /* -e */
  MODE_FLUX_BOUNDARY_COMPUTE = BIT(3), /* -F */
  MODE_BIN_GREEN = BIT(4), /* -G */
  MODE_GREEN = BIT(5), /* -g */
  MODE_DUMP_HELP = BIT(6), /* -h */
  MODE_MEDIUM_COMPUTE = BIT(7), /* -m */
  MODE_PROBE_COMPUTE_ON_INTERFACE = BIT(8), /* -P */
  MODE_PROBE_COMPUTE = BIT(9), /* -p */
  MODE_IR_COMPUTE = BIT(10), /* -R */
  MODE_MAP_COMPUTE = BIT(11), /* -S */
  MODE_BOUNDARY_COMPUTE = BIT(12), /* -s */
  MODE_VERBOSITY = BIT(13), /* -V */
  MODE_DUMP_VERSION = BIT(14), /* -v */

  GREEN_COMPATIBLE_MODES
    = MODE_PROBE_COMPUTE | MODE_PROBE_COMPUTE_ON_INTERFACE | MODE_MEDIUM_COMPUTE
    | MODE_BOUNDARY_COMPUTE,

  SURFACE_COMPUTE_MODES
    = MODE_BOUNDARY_COMPUTE | MODE_FLUX_BOUNDARY_COMPUTE | MODE_MAP_COMPUTE,

  EXT_COMPATIBLE_MODES = GREEN_COMPATIBLE_MODES | MODE_MEDIUM_COMPUTE,

  REGION_COMPUTE_MODES = SURFACE_COMPUTE_MODES | MODE_MEDIUM_COMPUTE,

  COMPUTE_MODES = GREEN_COMPATIBLE_MODES | MODE_IR_COMPUTE | SURFACE_COMPUTE_MODES,

  EXCLUSIVE_MODES = COMPUTE_MODES,

  SHORT_EXIT_MODES = MODE_DUMP_HELP | MODE_DUMP_VERSION,

  USE_STDOUT_MODES
  = MODE_DUMP_VTK | MODE_DUMP_HELP | MODE_DUMP_VERSION | MODE_IR_COMPUTE | MODE_GREEN
};

STATIC_ASSERT(GREEN_COMPATIBLE_MODES == (COMPUTE_MODES & GREEN_COMPATIBLE_MODES), 
  Cannot_have_a_GREEN_COMPATIBLE_MODE_that_is_not_a_COMPUTE_MODE);

enum dump_path_type {
  DUMP_NONE = 0,
  DUMP_SUCCESS = BIT(0),
  DUMP_ERROR = BIT(1),
  DUMP_ALL = DUMP_SUCCESS | DUMP_ERROR,
};

struct args {
  struct logger* logger;
  struct mem_allocator* allocator;
  struct darray_str model_files;
  char* medium_name;
  char* solve_filename;
  char* bin_green_filename;
  char* paths_filename;
  size_t samples;
  unsigned nthreads;
  double probe[4];
  enum stardis_mode mode;
  double scale_factor;
  double ambient_temp, ref_temp;
  char* camera;
  enum dump_path_type dump_paths;
  int verbose;
};

/* Same ctx used for both media and interface add (some unused parts) */
struct add_geom_ctx {
  struct sstl_desc stl_desc;
  unsigned properties[3];
  void* custom;
};

/* Possible callbacks for sg3d_geometry_add calls
 * when void* context is a struct add_geom_ctx */
extern LOCAL_SYM  void
add_geom_ctx_indices
  (const unsigned itri,
   unsigned ids[3],
   void* context);

extern LOCAL_SYM  void
add_geom_ctx_properties
  (const unsigned itri,
   unsigned prop[3],
   void* context);

extern LOCAL_SYM  void
add_geom_ctx_position
  (const unsigned ivert,
   double pos[3],
   void* context);

#ifdef NDEBUG
#define DEFAULT_NTHREADS SDIS_NTHREADS_DEFAULT
#else
#define DEFAULT_NTHREADS 1
#endif

extern LOCAL_SYM res_T
init_args
  (struct logger* logger,
   struct mem_allocator* mem,
   struct args** args);

extern LOCAL_SYM void
release_args
  (struct args* args);

extern char
mode_option
  (const int m);

extern void
print_multiple_modes
  (char* buf,
   const size_t sz,
   const int modes,
   const int dont);

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
   struct args* args);

extern res_T
parse_camera
  (struct logger* logger, 
   char* cam_param,
   struct camera* cam);

extern LOCAL_SYM res_T
process_model_line
  (const char* file_name,
   char* line,
   struct stardis* stardis);

#endif /*ARGS_H*/
