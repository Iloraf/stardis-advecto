/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com) */

#include "stardis-app.h"

int main(int argc, char** argv){

  struct args args = ARGS_DEFAULT;
  struct stardis stardis = NULL_STARDIS;
  size_t memsz = 0;
  int err = 0;
  res_T res = RES_OK;

  /* Check proper init for ARGS_DEFAULT */
  ASSERT(ARGS_DEFAULT.u.probe[3] == INF);

  res = parse_args(argc, argv, &args);
  if (res != RES_OK) goto error;
  if (args.just_help) goto exit;

  res = stardis_init(&args, &stardis);
  if (res != RES_OK) goto error;

  if (args.mode == DUMP_VTK) {
    res = dump_vtk(stdout, &stardis.geometry);
    if (res != RES_OK) goto error;
    goto exit;
  }

  res =  stardis_compute(&stardis, args.mode);
  if (res != RES_OK) goto error;

exit:
  stardis_release(&stardis);
  if(stardis.allocator_initialized
    && (memsz = MEM_ALLOCATED_SIZE(&stardis.allocator)) != 0)
  {
    char dump[4096] = { '\0' };
    MEM_DUMP(&stardis.allocator, dump, sizeof(dump));
    fprintf(stderr, "%s\n", dump);
    fprintf(stderr, "Memory leaks: %lu Bytes\n", (unsigned long)memsz);
  }
  mem_shutdown_proxy_allocator(&stardis.allocator);
  CHK(mem_allocated_size() == 0);
  return err;
error:
  err = -1;
  goto exit;
}
