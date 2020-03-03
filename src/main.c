/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com) */

#include "stardis-app.h"
#include "stardis-parsing.h"
#include "stardis-output.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>

#include <stdio.h>

int
main
  (int argc,
   char** argv)
{
  struct args* args = NULL;
  struct stardis stardis;
  int allocator_initialized = 0, stardis_initialized = 0;
  int err = 0;
  struct mem_allocator allocator;
  res_T res = RES_OK;

  ERR(mem_init_proxy_allocator(&allocator, &mem_default_allocator));
  allocator_initialized = 1;
  ERR(init_args(&allocator, &args));
  ERR(parse_args(argc, argv, args));
  if(args->just_help) goto exit;

  if(args->mode == UNDEF_MODE) {
    fprintf(stderr, "Nothing to do.\n"
      "One of the following options should be used:");
    print_multiple_modes(stderr, EXCLUSIVE_MODES);
    fprintf(stderr, "\n"
      "Use the -h option to get help.\n");
    goto exit;
  }

  ERR(stardis_init(args, &stardis));
  stardis_initialized = 1;

  if(args->mode & DUMP_VTK) {
    ERR(sg3d_geometry_dump_as_vtk(stardis.geometry.sg3d, stdout));
    /* If it applies dump the compute region */
    if(args->mode & REGION_COMPUTE_MODES)
      ERR(dump_compute_region_at_the_end_of_vtk(&stardis, stdout));
    /* If it applies dump holes */
    if(has_holes(&stardis))
      ERR(dump_holes_at_the_end_of_vtk(&stardis, stdout));
    goto exit; /* If dump flag set, then dump and exit */
  }
  else if(args->mode & COMPUTE_MODES) {
    ERR(stardis_compute(&stardis, args->mode));
  } else {
    fprintf(stderr, "Unknown mode: %c.\n", mode_option(args->mode));
  }

exit:
  release_args(args);
  if(stardis_initialized) stardis_release(&stardis);
  if(allocator_initialized) {
    if(MEM_ALLOCATED_SIZE(&allocator) != 0) {
      char dump[4096] = { '\0' };
      MEM_DUMP(&allocator, dump, sizeof(dump));
      fprintf(stderr, "%s\n", dump);
      fprintf(stderr, "\nMemory leaks: %zu Bytes\n",
        MEM_ALLOCATED_SIZE(&allocator));
    }
    mem_shutdown_proxy_allocator(&allocator);
  }
  return err;
error:
  err = -1;
  goto exit;
}
