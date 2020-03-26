/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef STARDIS_PARSING_H
#define STARDIS_PARSING_H

#include <sdis.h>

#include <rsys/rsys.h>
#include <rsys/dynamic_array_str.h>

#include <star/sstl.h>

struct camera;
struct logger;
enum log_type;
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
  UNDEF_MODE = 0,
  PROBE_COMPUTE = BIT(0),
  PROBE_COMPUTE_ON_INTERFACE = BIT(1),
  MEDIUM_COMPUTE = BIT(2),
  BOUNDARY_COMPUTE = BIT(3),
  IR_COMPUTE = BIT(4),
  MAP_COMPUTE = BIT(5),
  DUMP_VTK = BIT(6),
  GREEN_MODE = BIT(7),
  FLUX_BOUNDARY_COMPUTE = BIT(8),
  DUMP_PATHS = BIT(9),
  DUMP_HELP = BIT(10),
  DUMP_VERSION = BIT(11),

  GREEN_COMPATIBLE_MODES
    = PROBE_COMPUTE | PROBE_COMPUTE_ON_INTERFACE | MEDIUM_COMPUTE | BOUNDARY_COMPUTE,

  SURFACE_COMPUTE_MODES
    = BOUNDARY_COMPUTE | FLUX_BOUNDARY_COMPUTE | MAP_COMPUTE,

  REGION_COMPUTE_MODES = SURFACE_COMPUTE_MODES | MEDIUM_COMPUTE,

  COMPUTE_MODES = GREEN_COMPATIBLE_MODES | IR_COMPUTE | SURFACE_COMPUTE_MODES,

  EXCLUSIVE_MODES = COMPUTE_MODES,

  SHORT_EXIT_MODES = DUMP_HELP | DUMP_VERSION,

  USE_STDOUT_MODES
  = DUMP_VTK | DUMP_PATHS | DUMP_HELP | DUMP_VERSION | IR_COMPUTE | GREEN_MODE
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
  size_t samples;
  unsigned nthreads;
  double probe[4];
  enum stardis_mode mode;
  double scale_factor;
  double radiative_temp[2];
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
  (const enum stardis_mode m);

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
print_help
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
