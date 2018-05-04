/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com) */

#include "stardis-app.h"

int main(int argc, char** argv){

  struct args args = ARGS_DEFAULT;
  struct stardis stardis = NULL_STARDIS;
  size_t memsz = 0;
  int err = 0;
  res_T res = RES_OK;

  res = parse_args(argc, argv, &args);
  if (res != RES_OK) goto error;

  res = stardis_init(&args, &stardis);
  if (res != RES_OK) goto error;

  if (args.mode == DUMP_VTK){
    res = dump_vtk(stdout, &stardis.geometry, stardis.geometry.boundary_count);
    if (res != RES_OK) goto error;
    goto exit;
  }

  res =  stardis_compute(&stardis, args.mode);
  if (res != RES_OK) goto error;

exit:
  stardis_release(&stardis);
  if((memsz = mem_allocated_size()) != 0) {
    fprintf(stderr, "Memory leaks: %lu Bytes\n", (unsigned long)memsz);
  }

  return err;
error:
  err = -1;
  goto exit;
}
