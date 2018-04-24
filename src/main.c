/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com) */

#include "stardis-app.h"

static void
dump_boundary(FILE* stream, struct boundary* bound)
{
  int i = 0;
  fprintf(stream,"# bound_id hc T\n");
  for (i=0; i<sa_size(bound); ++i){
    fprintf(stream,"%i  %g %s\n", i, bound[i].hc, bound[i].T);
  }
}

static void 
dump_vertex(FILE* stream, struct vertex* vert_list)
{
  int i = 0;
  fprintf(stream,"# vertex_id x y z\n");
  for (i=0; i<sa_size(vert_list); ++i){
    fprintf(stream,"%i  %g %g %g\n",i ,SPLIT3(vert_list[i].xyz));
  }
}

static void 
dump_triangle(FILE* stream, struct triangle* tri_list)
{
  int i = 0;
  fprintf(stream,"# triangle_id   vertex_id vertex_id vertex_id");
  fprintf(stream,"  medium_front medium_back");
  fprintf(stream,"  bound_id\n"); 
  for (i=0; i<sa_size(tri_list); ++i){
    fprintf(stream,"%i  %i %i %i   %i %i   %i\n",
        i, 
        SPLIT3(tri_list[i].indices), 
        tri_list[i].medium_front, 
        tri_list[i].medium_back,
        tri_list[i].bound_id);
  }
}

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

  res =  stardis_compute(&stardis);
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
