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

#include "stardis-app.h"
#include "stardis-parsing.h"
#include "stardis-output.h"
#include "stardis-compute.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/logger.h>
#include <rsys/clock_time.h>

#include <stdlib.h>
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
  int mode = 0;
  struct time start;
  res_T res = RES_OK;

  time_current(&start);

  ERR(mem_init_proxy_allocator(&allocator, &mem_default_allocator));
  allocator_initialized = 1;

  ERR(logger_init(&allocator, &logger));
  logger_initialized = 1;
  /* Active loggin for args pasing */
  logger_set_stream(&logger, LOG_ERROR, log_err_fn, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_warn_fn, NULL);
  logger_set_stream(&logger, LOG_OUTPUT, log_prt_fn, NULL);

  ERR(init_args(&logger, &allocator, &args));
  args_initialized = 1;
  ERR(parse_args(argc, argv, args));
  mode = args->mode;

  if(mode & MODE_DUMP_HELP) {
    short_help(stdout, argv[0]);
    goto exit;
  }
  else if(mode & MODE_DUMP_VERSION) {
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

  if(mode & MODE_DUMP_VTK) {
    /* Dump all the app-independent information */
    ERR(sg3d_geometry_dump_as_vtk(stardis.geometry.sg3d, stdout));
    /* Dump boundaries
     * Should we dump connections too? */
    ERR(dump_boundaries_at_the_end_of_vtk(&stardis, stdout));
    /* Dump the compute region if any */
    if(mode & REGION_COMPUTE_MODES) {
      ERR(dump_compute_region_at_the_end_of_vtk(&stardis, stdout));
    }
    ERR(dump_enclosure_related_stuff_at_the_end_of_vtk(&stardis, stdout));

    /* If dump, exit after dump done */
    goto exit;
  }
  else if(mode & MODE_DUMP_C_CHUNKS) {
    ERR(dump_model_as_c_chunks(&stardis, stdout));
    /* If dump, exit after dump done */
    goto exit;
  }

  ASSERT(mode & COMPUTE_MODES);
  ERR(stardis_compute(&stardis, &start));

exit:
  if(args_initialized) release_args(args);
  if(stardis_initialized) stardis_release(&stardis);
  if(logger_initialized) logger_release(&logger);
  if(allocator_initialized) {
    if(MEM_ALLOCATED_SIZE(&allocator) != 0) {
      char dump[4096] = { '\0' };
      MEM_DUMP(&allocator, dump, sizeof(dump));
      fprintf(stderr, "%s\n", dump);
      fprintf(stderr, "\nMemory leaks: %lu Bytes\n",
        (unsigned long)MEM_ALLOCATED_SIZE(&allocator));
    }
    mem_shutdown_proxy_allocator(&allocator);
  }
  return err;
error:
  if(mode & COMPUTE_MODES)
    logger_print(&logger, LOG_ERROR,
      "No computation possible.\n");
  err = EXIT_FAILURE;
  goto exit;
}
