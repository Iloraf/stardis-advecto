/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef STARDIS_PARSING_H
#define STARDIS_PARSING_H

#include <sdis.h>

#include <rsys/rsys.h>

struct camera;
struct description;

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

  GREEN_COMPATIBLE_MODES = PROBE_COMPUTE | PROBE_COMPUTE_ON_INTERFACE | MEDIUM_COMPUTE
  | BOUNDARY_COMPUTE,

  COMPUTE_MODES = GREEN_COMPATIBLE_MODES | IR_COMPUTE | MAP_COMPUTE,

  NON_COMPUTE_MODES = UNDEF_MODE | DUMP_VTK | GREEN_MODE,

  EXCLUSIVE_MODES = COMPUTE_MODES | DUMP_VTK
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
  char* medium_filename;
  char* bc_filename;
  char* medium_name;
  char* solve_boundary_filename;
  size_t N;
  unsigned nthreads;
  union u { /* Trick to allow static initialization with INF */
    struct pt { double p[3]; uint64_t t; } pt;
    double probe[4];
  } u;
  enum stardis_mode mode;
  double scale_factor;
  double radiative_temp[2];
  char* camera;
  enum dump_path_type dump_paths;
  int just_help;
};

#ifdef NDEBUG
#define DEFAULT_NTHREADS SDIS_NTHREADS_DEFAULT
#else
#define DEFAULT_NTHREADS 1
#endif

#define ARGS_DEFAULT__ {\
  NULL, NULL, NULL, NULL, 10000, DEFAULT_NTHREADS,\
  { { {0.0, 0.0, 0.0}, 0x7FF0000000000000 /* probe[3]=INF */}},\
  UNDEF_MODE, 1.0, {300.0, 300.0}, NULL, DUMP_NONE, 0}
static const struct args ARGS_DEFAULT = ARGS_DEFAULT__;

extern char
mode_option
  (const enum stardis_mode m);

extern void
print_multiple_modes
  (FILE* stream,
   const int modes);

extern void
print_help
  (char* prog);

extern res_T
parse_args
  (const int argc,
   char** argv,
   struct args* args);

extern res_T
parse_camera
  (char* cam_param,
   struct camera* cam);

extern res_T
parse_medium_line
  (char* line,
   char** stl_filename,
   struct description* desc);

extern res_T
parse_boundary_line
  (char* line,
   char** stl_filename,
   struct description* desc);

#endif /*ARGS_H*/
