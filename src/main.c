/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com) */

#include "stardis-app.h"
#include "stardis-parsing.h"
#include "stardis-output.h"

#include <rsys/rsys.h>

#include <stdio.h>

#if !defined(ARCH_64BITS)
#error "Please write hash functions for the current arch"
#endif

int
main
  (int argc,
   char** argv)
{
  struct args args = ARGS_DEFAULT;
  struct stardis stardis = NULL_STARDIS;
  int err = 0;
  res_T res = RES_OK;

  /* Check proper init for ARGS_DEFAULT */
  ASSERT(ARGS_DEFAULT.u.probe[3] == INF);

  res = parse_args(argc, argv, &args);
  if (res != RES_OK) goto error;
  if (args.just_help) goto exit;

  if (args.mode == UNDEF_MODE) {
    fprintf(stderr, "Nothing to do.\n"
      "One of the following options should be used:");
    print_multiple_modes(stderr, EXCLUSIVE_MODES);
    fprintf(stderr, "\n"
      "Use the -h option to get help.\n");
    goto exit;
  }

  res = stardis_init(&args, &stardis);
  if (res != RES_OK) goto error;

  if (args.mode & DUMP_VTK) {
    res = dump_vtk(stdout, &stardis.geometry);
    if (res != RES_OK) goto error;
    goto exit;
  }
  else if (args.mode & COMPUTE_MODES) {
    res = stardis_compute(&stardis, args.mode);
    if (res != RES_OK) goto error;
  } else {
    fprintf(stderr, "Unknown mode: %c.\n", mode_option(args.mode));
  }

exit:
  stardis_release(&stardis);
  CHK(mem_allocated_size() == 0);
  return err;
error:
  err = -1;
  goto exit;
}
