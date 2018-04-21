/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef ARGS_H
#define ARGS_H

#include <unistd.h>
#include <rsys/rsys.h>
#include <rsys/cstr.h>

static void
print_help(char* prog)
{
  printf("usage %s -m MEDIUM.txt -b BOUNDARY.txt [-p X:Y:Z:TIME] [-n NUM_OF_REALIZATIONS] [-t NUM_OF_THREADS]\n", prog);  
}

struct args{
  char* medium_filename;
  char* bc_filename;
  size_t N;
  unsigned nthreads;
  double probe[4];
};
#define ARGS_DEFAULT {NULL, NULL, 10000, SDIS_NTHREADS_DEFAULT, {0,0,0,INF}}

static res_T
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

  while((opt = getopt(argc, argv, "hn:t:b:m:p:")) != -1) {
    switch(opt) {
      case 'h':
        print_help(argv[0]);
        res = RES_BAD_ARG;
        goto error;
        break;

      case 'n':
        res = cstr_to_ulong(optarg, &args->N);
        if (res != RES_OK){
          fprintf(stderr, "Invalid argument -n %s\n", optarg);
          goto error;
        }
        break;

      case 'b':
        args->bc_filename = optarg;
        break;

      case 'm':
        args->medium_filename = optarg;
        break;

      case 't':
        res = cstr_to_uint(optarg, &args->nthreads);
        if (res != RES_OK){
          fprintf(stderr, "Invalid argument -t %s\n", optarg);
          goto error;
        }
        break;

      case 'p':
        cstr_to_list_double(optarg, ':', args->probe, &len, 4);
        if(res == RES_OK && len != 4) res = RES_BAD_ARG;
        if (res != RES_OK){
          fprintf(stderr, "Invalid argument -p\n", optarg);
          goto error;
        }
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
