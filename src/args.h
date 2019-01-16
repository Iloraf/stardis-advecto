/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef ARGS_H
#define ARGS_H

#include <getopt.h>
#include <stdlib.h>

#include <rsys/rsys.h>
#include <rsys/cstr.h>
#include <sdis_version.h>

enum stardis_mode{
  PROBE_COMPUTE,
  IR_COMPUTE,
  DUMP_VTK
};

struct args{
  char* medium_filename;
  char* bc_filename;
  size_t N;
  unsigned nthreads;
  union u {
    struct pt { double p[3]; uint64_t t; } pt;
    double probe[4];
  } u;
  enum stardis_mode mode;
  double scale_factor;
  double radiative_temp[2];
  char* camera;
};
#define ARGS_DEFAULT__ {\
  NULL, NULL, 10000, 1,\
  { { {0.0, 0.0, 0.0}, 0x7FF0000000000000 /* probe[3]=INF */}},\
  PROBE_COMPUTE, 1.0, {300.0, 300.0}, NULL}
static const struct args ARGS_DEFAULT = ARGS_DEFAULT__;

static void
print_help(char* prog)
{
  printf("stardis-app built-on stardis library version %i.%i.%i\n",
      Stardis_VERSION_MAJOR,Stardis_VERSION_MINOR,Stardis_VERSION_PATCH);
  printf("usage : \n%s -m MEDIUM.txt -b BOUNDARY.txt [-p X,Y,Z,TIME]\n", prog);  
  printf("[-s SCALE_FACTOR] [-d] [-n NUM_OF_REALIZATIONS] [-t NUM_OF_THREADS]\n");  
  printf("[-r AMBIENT_RAD_TEMP:REFERENCE_TEMP]\n");  
  printf("\n  -h : print this help.\n");
  printf("\nArguments\n");
  printf("---------\n");
  printf("\n  -m MEDIUM.txt :\n");
  printf("     MEDIUM.txt is a text file which contains the description of the\n");
  printf("     media.\n");
  printf("\n  -b BOUNDARY.txt :\n");
  printf("     BOUNDARY.txt is a text file which contains the description of the\n");
  printf("     boundary conditions.\n");
  printf("\nOptionnal arguments\n");
  printf("-------------------\n");
  printf("\n -p X:Y:Z:TIME :\n");
  printf("    this is the position and the time of the probe.\n");
  printf("    default value: 0.0,0.0,0.0,INF.\n");
  printf("    The probe must be in a medium. If some boundary conditions are\n");
  printf("    time-dependant, TIME can not be INF.\n");
  printf("\n -s SCALE_FACTOR :\n");
  printf("    default value: 1.0.\n");
  printf("\n -d :\n");
  printf("    dump geometry in VTK format with medium front/back id and\n");
  printf("    boundary id.\n");
  printf("\n -n NUM_OF_REALIZATIONS :\n");
  printf("    number of Monte-Carlo realizations.\n");
  printf("    default value: 10000.\n");
  printf("\n -t NUM_OF_THREADS :\n");
  printf("    default value: all threads available.\n");
  printf("\n -r AMBIENT_RAD_TEMP,REFERENCE_TEMP :\n");
  printf("    Parameters for the radiative transfer.\n");
  printf("    AMBIENT_RAD_TEMP is the ambient radiative temperature .\n");
  printf("    REFERENCE_TEMP is the temperature used for the linearization.\n");
  printf("    of theradiative transfer.\n");
  printf("\n -R spp=SPP:fov=FOV:up=XUP,YUP,ZUP:pos=X,Y,Z:tgt=XT,YT,ZT:img=WxH :\n");
  printf("    The infra-red rendering mode.\n");
  printf("    spp is the number of samples per pixel (default: 4).\n");
  printf("    fov is the field of view (default: 70 degrees).\n");
  printf("    up is the up direction (default: 0,0,1).\n");
  printf("    pos is the position of camera (default: 1,1,1).\n");
  printf("    tgt is the target (default: 0,0,0).\n");
  printf("    img is the resolution (default: 640x480).\n");
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

  while((opt = getopt(argc, argv, "hn:t:b:m:p:ds:r:R:")) != -1) {
    switch(opt) {
      case 'h':
        print_help(argv[0]);
        res = RES_BAD_ARG;
        goto error;
        break;

      case 'n': {
        args->N = strtoull(optarg, NULL, 10);
        if (args->N <= 0) {
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -n %s\n", optarg);
          goto error;
        }
        break;
      }

      case 'b':
        args->bc_filename = optarg;
        break;

      case 'm':
        args->medium_filename = optarg;
        break;

      case 't':
        res = cstr_to_uint(optarg, &args->nthreads);
        if (res != RES_OK
            || args->nthreads <= 0){
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -t %s\n", optarg);
          goto error;
        }
        break;

      case 'p':
        cstr_to_list_double(optarg, ',', args->u.probe, &len, 4);
        if(len != 4 
           || res != RES_OK){
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -p %s\n", optarg);
          goto error;
        }
        break;

      case 'd':
        args->mode = DUMP_VTK;
        break;

      case 's':
        cstr_to_double(optarg, &args->scale_factor);
        if (res != RES_OK
            || args->scale_factor <=0){
          res = RES_BAD_ARG;
          fprintf(stderr, "Invalid argument -s %s\n", optarg);
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
          fprintf(stderr, "Invalid argument -r %s\n", optarg);
          goto error;
        }
        break;

      case 'R':
        args->mode = IR_COMPUTE;
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
