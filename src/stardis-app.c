/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#include <string.h>
#include "stardis-app.h"
#include <tinyexpr.h>


#define MEDIUM 0
#define BOUNDARY 1

/*******************************************************************************
 *
 ******************************************************************************/

static inline char*
read_line(char** pb, FILE* stream)
{
  const int chunk = 32;
  char* b = *pb;

  if(!b) b = sa_add(b, 128);

  do {
    if(!fgets(b, (int)sa_size(b), stream)) return NULL;

    /* Ensure that he whole line is read */
    while(!strrchr(b, '\n') && !feof(stream)) {
      fgets(sa_add(b, (size_t)chunk), chunk, stream);
    }

    b[strcspn(b, "#\n\r")] = '\0'; /* Rm new line & comments */
  } while(strspn(b, " \t") == strlen(b)); /* Empty line */
  *pb = b;
  return b;
}

static res_T
parse_medium_line(char* line, char** stl_filename, struct material* mat)
{
  char* tk = NULL;
  res_T res = RES_OK;

  tk = strtok(line," ");
  *stl_filename = malloc(strlen(tk) + 1);
  strcpy(*stl_filename,tk);  
  tk = strtok(NULL," ");
  res = cstr_to_double(tk, &mat->lambda);
  if (res != RES_OK) {
    fprintf(stderr,"invalid lambda\n");
    goto error;
  }
  tk = strtok(NULL," ");
  res = cstr_to_double(tk, &mat->rho);
  if (res != RES_OK) {
    fprintf(stderr,"invalid rho\n");
    goto error;
  }
  tk = strtok(NULL," ");
  res = cstr_to_double(tk, &mat->cp);
  if (res != RES_OK) {
    fprintf(stderr,"invalid cp\n");
    goto error;
  }
  tk = strtok(NULL," ");
  res = cstr_to_double(tk, &mat->delta);
  if (res != RES_OK) {
    fprintf(stderr,"invalid delta\n");
    goto error;
  }
  tk = strtok(NULL,"\"");
  tk = strtok(NULL,"\"");
  mat->Tinit = malloc(strlen(tk) + 1);
  strcpy(mat->Tinit,tk);  

exit:
  return res;
error:
  goto exit;
}

static res_T
parse_boundary_line(char* line, char** stl_filename, struct boundary* bound)
{
  char* tk = NULL;
  res_T res = RES_OK;

  tk = strtok(line," ");
  *stl_filename = malloc(strlen(tk) + 1);
  strcpy(*stl_filename,tk);  

  tk = strtok(NULL," ");
  if (strcmp(tk,"h")==0 || strcmp(tk,"H")==0  ){
    tk = strtok(NULL," ");
    res = cstr_to_double(tk, &bound->hc);
    if (res != RES_OK ) {
      fprintf(stderr,"invalid value parameter: %g \n", bound->hc);
      goto error;
    }

    tk = strtok(NULL,"\"");
    tk = strtok(NULL,"\"");
    bound->T = malloc(strlen(tk) + 1);
    strcpy(bound->T,tk);  

  } else  if (strcmp(tk,"t")==0 || strcmp(tk,"T")==0  ){

    tk = strtok(NULL,"\"");
    tk = strtok(NULL,"\"");
    bound->T = malloc(strlen(tk) + 1);
    strcpy(bound->T,tk);  

  } else {
    fprintf(stderr,"unknown boundary type: %s\n", tk);
    goto error;
  }

exit:
  return res;
error:
  goto exit;
}
static res_T
read_vertices
(struct sstl_desc* desc,
 struct htable_vertex* vertex2id,
 unsigned** id2id,
 struct vertex** vert_list)
{
  res_T res = RES_OK;
  int vtx_index = 0;

  for (vtx_index = 0; vtx_index < desc->vertices_count; ++vtx_index){
    struct vertex vtx = NULL_VERTEX;
    unsigned *found_id = NULL;

    vtx.xyz[0] = desc->vertices[3*vtx_index + 0];
    vtx.xyz[1] = desc->vertices[3*vtx_index + 1];
    vtx.xyz[2] = desc->vertices[3*vtx_index + 2];

    found_id = htable_vertex_find(vertex2id, &vtx);

    if (found_id){
      sa_push(*id2id, *found_id);
    } else {
      unsigned size = htable_vertex_size_get(vertex2id);
      htable_vertex_set(vertex2id, &vtx, &size);
      sa_push(*vert_list, vtx);
      sa_push(*id2id, size);
    }
  }

  return res;
}

static res_T 
read_triangles
(int FLAG,
 struct sstl_desc* desc,
 struct htable_triangle* triangle2id,
 unsigned* id2id, char** cl_type,
 const unsigned* id,
 struct triangle** tri_list)
{
  res_T res = RES_OK;
  int tri_index = 0;
  for (tri_index = 0; tri_index < desc->triangles_count; ++tri_index){
    struct triangle tri = NULL_TRIANGLE;
    unsigned *found_id = NULL;

    tri.sids[0] = tri.indices[0] = id2id[desc->indices[3*tri_index + 0]];
    tri.sids[1] = tri.indices[1] = id2id[desc->indices[3*tri_index + 1]];
    tri.sids[2] = tri.indices[2] = id2id[desc->indices[3*tri_index + 2]];

    if(tri.sids[0] > tri.sids[1]) SWAP(unsigned, tri.sids[0], tri.sids[1]);
    if(tri.sids[1] > tri.sids[2]) SWAP(unsigned, tri.sids[1], tri.sids[2]);
    if(tri.sids[0] > tri.sids[1]) SWAP(unsigned, tri.sids[0], tri.sids[1]);
    found_id = htable_triangle_find(triangle2id, &tri);

    if (found_id){
      if (FLAG==MEDIUM) (*tri_list)[*found_id].medium_back = *id;
      if (FLAG==BOUNDARY) (*tri_list)[*found_id].bound_id = *id;
    } else {
      unsigned size = htable_triangle_size_get(triangle2id);
      if (FLAG==MEDIUM) tri.medium_front = *id;
      htable_triangle_set(triangle2id, &tri, &size);
      /*printf("triangle  %i %i %i\n  added at index %i\n", SPLIT3(tri.indices), size);*/
      sa_push(*tri_list, tri);
    }
  }
  return res;
}


static res_T
read_stl
(char FLAG,
 unsigned id,
 char** stl_filename,
 struct htable_vertex* vertex2id,
 struct htable_triangle* triangle2id,
 struct geometry* geometry)
{
  res_T res = RES_OK; 
  struct sstl* sstl = NULL;
  struct sstl_desc desc;
  unsigned* id2id = NULL;

  SSTL(create(NULL, NULL, 0, &sstl));


  SSTL(load(sstl, *stl_filename));
  SSTL(get_desc(sstl, &desc));

  read_vertices(&desc, vertex2id, &id2id, &geometry->vertex);
  read_triangles(FLAG, &desc, triangle2id, id2id, NULL, &id, &geometry->triangle);
  sa_release(id2id);


exit:
   SSTL(ref_put(sstl));
  return res;
error:
  goto exit;
}
  

/*******************************************************************************
 *
 ******************************************************************************/

static res_T
geometry_analyse
(const char* medium_filename,
 const char* bc_filename,
 struct geometry* geometry,
 struct material** material,
 struct boundary** boundary)
{
  res_T res = RES_OK;
  FILE* input;
  char* line = NULL;
  struct htable_vertex vertex2id;
  struct htable_triangle triangle2id;

  /*init geometry*/
  *geometry = NULL_GEOMETRY;

  /*parse medium file*/
  input = fopen(medium_filename,"r");
  if (!input){
    fprintf(stderr,"Can not open %s\n", medium_filename);
    res = RES_IO_ERR;
    goto error;
  }

  htable_vertex_init(NULL, &vertex2id);
  htable_triangle_init(NULL, &triangle2id);
  /* loop on media */
  while (read_line(&line, input)){
    char* stl_filename = NULL;
    struct material mat = NULL_MATERIAL;

    res = parse_medium_line(line, &stl_filename, &mat);
    sa_push(*material,mat);

    /*analyse medium stl*/
    res = read_stl(MEDIUM, geometry->medium_count, &stl_filename,
        &vertex2id, &triangle2id, geometry);

    geometry->medium_count += 1;
  }
  fclose(input);

  /*parse boundary file*/
  input = fopen(bc_filename,"r");
  if (!input){
    fprintf(stderr,"Can not open %s\n", bc_filename);
    res = RES_IO_ERR;
    goto error;
  }

  /* loop on boundaries */
  while (read_line(&line, input)){
    char* stl_filename = NULL;
    struct boundary bound = NULL_BOUNDARY;

    res = parse_boundary_line(line, &stl_filename, &bound);
    sa_push(*boundary,bound);

    /*analyse medium stl*/
    res = read_stl(BOUNDARY, geometry->boundary_count, &stl_filename,
        &vertex2id, &triangle2id, geometry);

    geometry->boundary_count += 1;
  }

  fclose(input);

exit:
  htable_vertex_init(NULL, &vertex2id);
  htable_triangle_init(NULL, &triangle2id);
  sa_release(line);
  return res;
error:
  goto exit;
}


/*******************************************************************************
 *
 ******************************************************************************/

res_T 
stardis_init
  (const struct args* args,
   struct stardis* stardis)
{
  res_T res = RES_OK;

  res = geometry_analyse(args->medium_filename, 
      args->bc_filename, 
      &stardis->geometry, 
      &stardis->material, 
      &stardis->boundary); 

  stardis->N = args->N;
  stardis->nthreads = args->nthreads;
  stardis->probe[0] = args->probe[0];
  stardis->probe[1] = args->probe[1];
  stardis->probe[2] = args->probe[2];
  stardis->probe[3] = args->probe[3];

exit:
  return res;
error:
  goto exit;
}
 

res_T
stardis_release(struct stardis* stardis)
{
  res_T res = RES_OK;

  res = release_geometry(&stardis->geometry);
  sa_release(stardis->material);
  sa_release(stardis->boundary);
  
exit:
  return res;
error:
  goto exit;
}
