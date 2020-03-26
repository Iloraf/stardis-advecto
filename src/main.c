/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com) */

#include "stardis-app.h"
#include "stardis-parsing.h"
#include "stardis-output.h"
#include "stardis-compute.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/logger.h>

#include <stdio.h>

int
main
  (int argc,
   char** argv)
{
  struct args* args = NULL;
  struct stardis stardis;
  int logger_initialized = 0, allocator_initialized = 0,
    args_initialized = 0, stardis_initialized = 0;
  int err = EXIT_SUCCESS;
  struct mem_allocator allocator;
  struct logger logger;
  res_T res = RES_OK;

  ERR(mem_init_proxy_allocator(&allocator, &mem_default_allocator));
  allocator_initialized = 1;

  ERR(logger_init(&allocator, &logger));
  logger_initialized = 1;
  /* Active loggin for args pasing */
  logger_set_stream(&logger, LOG_OUTPUT, log_prt, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_err, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_warn, NULL);

  ERR(init_args(&logger, &allocator, &args));
  args_initialized = 1;
  ERR(parse_args(argc, argv, args));

  if(args->mode & DUMP_HELP) {
    print_help(stdout, argv[0]);
    goto exit;
  }
  else if(args->mode & DUMP_VERSION) {
    print_version(stdout);
    goto exit;
  }

  /* Deactivate some loggin according to the -V arg */
  if(args->verbose < 1)
    logger_set_stream(&logger, LOG_ERROR, NULL, NULL);
  if(args->verbose < 2)
    logger_set_stream(&logger, LOG_WARNING, NULL, NULL);
  if(args->verbose < 3)
    logger_set_stream(&logger, LOG_OUTPUT, NULL, NULL);

  ERR(stardis_init(args, &logger, &allocator, &stardis));
  stardis_initialized = 1;
  release_args(args);
  args_initialized = 0;

  if(stardis.mode & DUMP_VTK) {
    /* Dump all the app-independent information */
    ERR(sg3d_geometry_dump_as_vtk(stardis.geometry.sg3d, stdout));
    /* Dump the compute region if any */
    if(stardis.mode & REGION_COMPUTE_MODES)
      ERR(dump_compute_region_at_the_end_of_vtk(&stardis, stdout));
    /* Dump possible holes
     * TODO: as this extracts enclosures, it could dump front/back
     * enclosure IDs too */
    ERR(dump_enclosure_related_stuff_at_the_end_of_vtk(&stardis, stdout));

    /* If dump flag set, then dump and exit */
    goto exit;
  }

  ASSERT(stardis.mode & COMPUTE_MODES);
  ERR(stardis_compute(&stardis));

exit:
  if(args_initialized) release_args(args);
  if(stardis_initialized) stardis_release(&stardis);
  if(logger_initialized) logger_release(&logger);
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
  err = EXIT_FAILURE;
  goto exit;
}
