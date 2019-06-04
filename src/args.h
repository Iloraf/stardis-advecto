/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef ARGS_H
#define ARGS_H

#include <getopt.h>
#include <stdlib.h>

#include <rsys/rsys.h>
#include <rsys/cstr.h>
#include <sdis_version.h>

enum stardis_mode {
  UNDEF_MODE = 0,
  PROBE_COMPUTE = BIT(0),
  MEDIUM_COMPUTE = BIT(1),
  BOUNDARY_COMPUTE = BIT(2),
  GREEN_MODE = BIT(3),
  IR_COMPUTE = BIT(4),
  DUMP_VTK = BIT(5),
  PROBE_COMPUTE_ON_INTERFACE = BIT(6),
  COMPUTE_MODES = PROBE_COMPUTE | MEDIUM_COMPUTE | BOUNDARY_COMPUTE | IR_COMPUTE | DUMP_VTK | PROBE_COMPUTE_ON_INTERFACE
};

static char mode_option(const enum stardis_mode m) {
  int found = 0;
  char res = '?';
  if (m & PROBE_COMPUTE) { found++; res = 'p'; }
  if (m & MEDIUM_COMPUTE) { found++; res = 'm'; }
  if (m & BOUNDARY_COMPUTE) { found++; res = 'b'; }
  if (m & IR_COMPUTE) { found++; res = 'R'; }
  if (m & DUMP_VTK) { found++; res = 'd'; }
  if (m & PROBE_COMPUTE_ON_INTERFACE) { found++; res = 'P'; }
  ASSERT(found == 1);
  return res;
}

enum dump_path_type {
  DUMP_NONE = 0,
  DUMP_SUCCESS = BIT(0),
  DUMP_ERROR = BIT(1),
  DUMP_ALL = DUMP_SUCCESS | DUMP_ERROR,
};

struct args{
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

static void
print_help(char* prog)
{
  printf("stardis-app built-on stardis library version %i.%i.%i\n",
      Stardis_VERSION_MAJOR,Stardis_VERSION_MINOR,Stardis_VERSION_PATCH);
  printf("usage: \n%s -M MEDIUM.txt -B BOUNDARY.txt\n", prog);
  printf("[ -p X,Y,Z,TIME | -m MEDIUM_NAME,TIME | -d | -b SURFACE.vtk,TIME\n");
  printf("  | -R t=TIME:spp=SPP:fov=FOV:up=XUP,YUP,ZUP:pos=X,Y,Z:tgt=XT,YT,ZT:img=WxH ]\n");
  printf("[ -g] [-s SCALE_FACTOR] [-D {all | error | success}] -d\n");
  printf("[-n NUM_OF_REALIZATIONS] [-t NUM_OF_THREADS]\n");  
  printf("[-r AMBIENT_RAD_TEMP:REFERENCE_TEMP]\n");  
  printf("\n  -h: print this help.\n");
  printf("\nMandatory arguments\n");
  printf("---------\n");
  printf("\n  -M MEDIUM.txt:\n");
  printf("     MEDIUM.txt is a text file which contains the description of the\n");
  printf("     media.\n");
  printf("\n  -B BOUNDARY.txt:\n");
  printf("     BOUNDARY.txt is a text file which contains the description of the\n");
  printf("     boundary conditions.\n");
  printf("\nExclusive arguments\n");
  printf("-------------------\n");
  printf("\n -p X:Y:Z:TIME:\n");
  printf("    Compute the temperature at the given probe.\n");
  printf("    The probe must be in a medium.\n");
  printf("    If some boundary conditions are time-dependant, TIME cannot be INF.\n");
  printf("\n -P X:Y:Z:TIME:\n");
  printf("    Compute the temperature at the given probe on an interface.\n");
  printf("    The probe must be in a medium and is moved to the closest interface.\n");
  printf("    If some boundary conditions are time-dependant, TIME cannot be INF.\n");
  printf("\n -m MEDIUM_NAME,TIME:\n");
  printf("    Compute the mean temperature in a given medium at a given time.\n");
  printf("    If some boundary conditions are time-dependant, TIME cannot be INF.\n");
  printf("\n -d:\n");
  printf("    Dump the geometry in VTK format with medium front/back id and\n");
  printf("    boundary id.\n");
  printf("\n -b SURFACE.vtk,TIME:\n");
  printf("    Compute the mean temperature on a given 2D shape at a given time.\n");
  printf("    Mean temperature is computed on the front sides of the primitives\n");
  printf("    listed in SURFACE.vtk. These primitives are not added to the geometry,\n");
  printf("    but must be already known. The shape doesn't need to be connex.\n");
  printf("    If some boundary conditions are time-dependant, TIME cannot be INF.\n");
  printf("\n -R t=TIME:spp=SPP:fov=FOV:up=XUP,YUP,ZUP:pos=X,Y,Z:tgt=XT,YT,ZT:img=WxH:\n");
  printf("    The infra-red rendering mode.\n");
  printf("    t is the rendering time (default: INF).\n");
  printf("    spp is the number of samples per pixel (default: 4).\n");
  printf("    fov is the field of view (default: 70 degrees).\n");
  printf("    up is the up direction (default: 0,0,1).\n");
  printf("    pos is the position of camera (default: 1,1,1).\n");
  printf("    tgt is the target (default: 0,0,0).\n");
  printf("    img is the resolution (default: 640x480).\n");
  printf("\nOptionnal arguments\n");
  printf("-------------------\n");
  printf("\n -g:\n");
  printf("    Compute the green function.\n");
  printf("    Must be used in conjonction with one the -p, -m or -b options.\n");
  printf("    Provided TIME is silently ignored.\n");
  printf("\n -s SCALE_FACTOR:\n");
  printf("    default value: 1.0.\n");
  printf("\n -D { all | error | success }:\n");
  printf("    dump paths in VTK format.\n");
  printf("\n -n NUM_OF_REALIZATIONS:\n");
  printf("    Number of Monte-Carlo realizations.\n");
  printf("    default value: 10000.\n");
  printf("\n -t NUM_OF_THREADS:\n");
  printf("    Number of threads; default value is all threads available.\n");
  printf("\n -r AMBIENT_RAD_TEMP,REFERENCE_TEMP:\n");
  printf("    Parameters for the radiative transfer.\n");
  printf("    AMBIENT_RAD_TEMP is the ambient radiative temperature .\n");
  printf("    REFERENCE_TEMP is the temperature used for the linearization\n");
  printf("    of the radiative transfer.\n");
}


static INLINE res_T
parse_args(const int argc, char** argv, struct args* args)
{
  int opt = 0;
  size_t len = 0;
  res_T res = RES_OK;

  if (argc == 1) {
    print_help(argv[0]);
    res = RES_BAD_ARG;
    goto error;
  }

  opterr = 0; /* No default error messages */
  while((opt = getopt(argc, argv, "hgn:t:b:B:M:m:p:P:dD:s:r:R:")) != -1) {
    switch(opt) {
      case '?': /* Unreconised option */
        res = RES_BAD_ARG;
        fprintf(stderr, "Invalid option -%c\n", optopt);
        goto error;

      case 'h':
        print_help(argv[0]);
        args->just_help = 1;
        goto exit;

      case 'g':
        args->mode |= GREEN_MODE;
        break;

      case 'n': {
        long long n = strtoll(optarg, NULL, 10);
        if (n <= 0 || n > SIZE_MAX) {
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -%c %s\n", opt, optarg);
          goto error;
        }
        args->N = n;
        break;
      }

      case 'B':
        args->bc_filename = optarg;
        break;

      case 'M':
        args->medium_filename = optarg;
        break;

      case 't':
        res = cstr_to_uint(optarg, &args->nthreads);
        if (res != RES_OK
            || args->nthreads <= 0){
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -%c %s\n", opt, optarg);
          goto error;
        }
        break;

      case 'p':
        if (args->mode & COMPUTE_MODES) {
          res = RES_BAD_ARG;
          fprintf(stderr, "Cannot specify multiple modes: -%c and -%c\n",
            (char)opt, mode_option(args->mode));
          goto error;
        }
        args->mode |= PROBE_COMPUTE;
        cstr_to_list_double(optarg, ',', args->u.probe, &len, 4);
        if (len != 4
          || res != RES_OK
          || args->u.probe[3] < 0)
        {
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -%c %s\n", opt, optarg);
          goto error;
        }
        break;

      case 'P':
        if (args->mode & COMPUTE_MODES) {
          res = RES_BAD_ARG;
          fprintf(stderr, "Cannot specify multiple modes: -%c and -%c\n",
            (char)opt, mode_option(args->mode));
          goto error;
        }
        args->mode |= PROBE_COMPUTE_ON_INTERFACE;
        cstr_to_list_double(optarg, ',', args->u.probe, &len, 4);
        if(len != 4 
           || res != RES_OK
           || args->u.probe[3] < 0)
        {
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -%c %s\n", opt, optarg);
          goto error;
        }
        break;

      case 'b': {
        int err = 0;
        char *ptr;
        if (args->mode & COMPUTE_MODES) {
          res = RES_BAD_ARG;
          fprintf(stderr, "Cannot specify multiple modes: -%c and -%c\n",
            (char)opt, mode_option(args->mode));
          goto error;
        }
        args->mode |= BOUNDARY_COMPUTE;
        ptr = strrchr(optarg, ',');
        /* Single ',' allowed */
        if (!ptr || ptr != strchr(optarg, ','))
          err = 1;
        else {
          *ptr = '\0';
          ptr++;
          args->solve_boundary_filename = optarg;
          err = (RES_OK != cstr_to_double(ptr, args->u.probe + 3) || args->u.probe[3] < 0);
        }
        if (err) {
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -%c %s\n", opt, optarg);
          goto error;
        }
        break;
      }

      case 'm': {
        char* ptr;
        if (args->mode & COMPUTE_MODES) {
          res = RES_BAD_ARG;
          fprintf(stderr, "Cannot specify multiple modes: -%c and -%c\n",
            (char)opt, mode_option(args->mode));
          goto error;
        }
        args->mode |= MEDIUM_COMPUTE;
        ptr = strpbrk(optarg, ",");
        if (! ptr || ptr == optarg)
          res = RES_BAD_ARG;
        else res = cstr_to_double(ptr+1, args->u.probe+3);
        if (res != RES_OK) {
          fprintf(stderr, "Invalid argument -%c %s\n", opt, optarg);
          goto error;
        }
        *ptr = '\0';
        args->medium_name = optarg;
        break;
      }

      case 'd':
        if (args->mode & COMPUTE_MODES) {
          res = RES_BAD_ARG;
          fprintf(stderr, "Cannot specify multiple modes: -%c and -%c\n",
            (char)opt, mode_option(args->mode));
          goto error;
        }
        args->mode |= DUMP_VTK;
        break;

      case 'D':
        if (0 == strcmp(optarg, "all")) {
          args->dump_paths = DUMP_ALL;
        } else if(0 == strcmp(optarg, "error")) {
          args->dump_paths = DUMP_ERROR;
        } else if(0 == strcmp(optarg, "success")) {
          args->dump_paths = DUMP_SUCCESS;
        } else {
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -%c %s\n", opt, optarg);
          goto error;
        }
        break;

      case 's':
        cstr_to_double(optarg, &args->scale_factor);
        if (res != RES_OK
            || args->scale_factor <=0){
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -%c %s\n", opt, optarg);
          goto error;
        }
        break;

      case 'r':
        cstr_to_list_double(optarg, ',', args->radiative_temp, &len, 2);
        if (res != RES_OK 
            || len != 2
            || args->radiative_temp[0] <=0
            || args->radiative_temp[1] <=0){
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -%c %s\n", opt, optarg);
          goto error;
        }
        break;

      case 'R':
        if (args->mode & COMPUTE_MODES) {
          res = RES_BAD_ARG;
          fprintf(stderr, "Cannot specify multiple modes: -%c and -%c\n",
            (char)opt, mode_option(args->mode));
          goto error;
        }
        args->mode |= IR_COMPUTE;
        args->camera = optarg;
        break;
    }
  }

if (!args->medium_filename){ 
  fprintf(stderr, "Argument required -m\n");
  res = RES_BAD_ARG;
  goto error;
}

if (!args->bc_filename){ 
  fprintf(stderr, "Argument required -b\n");
  res = RES_BAD_ARG;
  goto error;
}

exit:
  return res;
error:
  goto exit;
}

#endif /*ARGS_H*/
