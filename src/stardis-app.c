/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com) */

#include "stardis-app.h"
#include "stardis-output.h"

#include <rsys/hash_table.h>
#define HTABLE_NAME descriptions
#define HTABLE_KEY struct description
#define HTABLE_DATA unsigned
#define HTABLE_KEY_FUNCTOR_EQ eq_description
#define HTABLE_KEY_FUNCTOR_HASH hash_description
#include <rsys/hash_table.h>

#define _GNU_SOURCE 1
#include <string.h>
#include <ctype.h>

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

static char*
read_line
  (char** pb,
   FILE* stream)
{
  const int chunk_sz = 32;
  char* buf = *pb;
  size_t idx;

  ASSERT(pb && stream);

  if(!buf) buf = sa_add(buf, 128);

  do {
    /* Read stream until EOL, EOF or up to sa_size(b) */
    if(!fgets(buf, (int)sa_size(buf) + 1, stream)) return NULL;

    /* Ensure that he whole line is read */
    while(!strrchr(buf, '\n') && !feof(stream)) {
      /* Add some room to buffer */
      char* more = sa_add(buf, (size_t)chunk_sz);
      /* Continue reading chunk_sz more characters until EOL or EOF */
      fgets(more, chunk_sz + 1, stream);
    }
    /* Search idx of first occurence of comment char or EOL */
    idx = strcspn(buf, "#\n\r");
    /* Remove meaningless text */
    buf[idx] = '\0';
    /* Find first text */
    idx = strspn(buf, " \t");
  } while(buf[idx] == '\0'); /* Skip empty lines */
  *pb = buf;
  return buf;
}

static res_T
read_vertices
  (struct sstl_desc* desc,
  struct htable_vertex* vertex2id,
  unsigned** id2id,
  struct geometry* geometry)
{
  res_T res = RES_OK;
  unsigned vtx_index = 0;

  ASSERT(desc && vertex2id && id2id && geometry);

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
      size_t sz = htable_vertex_size_get(vertex2id);
      unsigned size;
      ASSERT(sz < UINT_MAX);
      size = (unsigned)sz;
      res = htable_vertex_set(vertex2id, &vtx, &size);
      if (res != RES_OK) return res;
      sa_push(geometry->vertex, vtx);
      sa_push(*id2id, size);
    }
  }

  return res;
}

static int
indices_to_key
  (struct unsigned3* key,
   const struct unsigned3* indices)
{
  int i, reversed = 0;

  ASSERT(key && indices);

  FOR_EACH(i, 0, 3) key->data[i] = indices->data[i];
  if (key->data[0] > key->data[1]) {
    reversed = !reversed;
    SWAP(unsigned, key->data[0], key->data[1]);
  }
  if (key->data[1] > key->data[2]) {
    reversed = !reversed;
    SWAP(unsigned, key->data[1], key->data[2]);
  }
  if (key->data[0] > key->data[1]) {
    reversed = !reversed;
    SWAP(unsigned, key->data[0], key->data[1]);
  }
  return reversed;
}

static res_T
read_triangles
  (struct sstl_desc* stl_desc,
   struct htable_triangle* triangle2id,
   const char* stl_filename,
   const unsigned* id2id,
   const enum content_type file_type,
   const unsigned desc_id,
   struct geometry* geometry,
   struct solve_boundary* boundary)
{
  res_T res = RES_OK;
  unsigned* packed_indices = NULL;
  float* packed_normals = NULL;
  unsigned tri_index = 0, drop_cpt = 0;

  ASSERT(stl_desc && triangle2id && stl_filename && id2id && geometry && boundary);

  for (tri_index = 0; tri_index < stl_desc->triangles_count; ++tri_index) {
    struct triangle tri = NULL_TRIANGLE;
    const unsigned *found_id = NULL;
    unsigned i, id;
    struct unsigned3 key;
    int reversed;
    int drop = 0;
    enum sdis_side current_side = SDIS_FRONT;
    unsigned* side_desc_ptr = NULL;
    float coord[3][3];

    FOR_EACH(i, 0, 3)
      tri.indices.data[i] = id2id[stl_desc->indices[3*tri_index + i]];
    reversed = indices_to_key(&key, &tri.indices);

    FOR_EACH(i, 0, 3) {
      unsigned j;
      f3_set(coord[i], geometry->vertex[key.data[i]].xyz);
      FOR_EACH(j, 0, i) {
        if (f3_eq(coord[i], coord[j])) {
          drop = 1;
          drop_cpt++;
        }
      }
    }
    if (drop) continue;
    FOR_EACH(i, 0, 3) {
      sa_push(packed_indices, stl_desc->indices[3 * tri_index + i]);
      sa_push(packed_normals, stl_desc->normals[3 * tri_index + i]);
    }

    /* Find triangle if already known, or insert it in known list */
    found_id = htable_triangle_find(triangle2id, &key);
    if (found_id) {
      struct triangle* original_tri;
      int original_reversed;
      id = *found_id;
      original_tri = geometry->triangle + id;
      original_reversed = indices_to_key(&key, &original_tri->indices);
      current_side = (reversed == original_reversed) ? SDIS_FRONT : SDIS_BACK;
      ASSERT(id < sa_size(geometry->triangle));
    } else {
      size_t sz;
      if (file_type == CONTENT_BOUNDARY) {
        /* Cannot create new geometry! */
        fprintf(stderr,
          "Triangle %u from file %s cannot compute temperature on this surface\n"
          "Not part of the model\n",
          tri_index, stl_filename);
        return RES_BAD_ARG;
      }
      sz = htable_triangle_size_get(triangle2id);
      ASSERT(sz == sa_size(geometry->triangle));
      ASSERT(sz < UINT_MAX);
      id = (unsigned)sz;
      /* Store tri as described by stl_desc regardless of key order */
      res = htable_triangle_set(triangle2id, &key, &id);
      if (res != RES_OK) goto error;
      sa_push(geometry->triangle, tri);
    }

    switch (file_type) {
    case CONTENT_CONNECTION:
      side_desc_ptr = &geometry->triangle[id].connection_description;
      break;
    case CONTENT_GEOMETRY:
      /* Is the triangle in triangle list reversed wrt stl_desc? */
      side_desc_ptr = (current_side == SDIS_FRONT) ?
        &geometry->triangle[id].front_description
        : &geometry->triangle[id].back_description;
      break;
    case CONTENT_BOUNDARY:
      sa_push(boundary->primitives, id);
      sa_push(boundary->sides, current_side);
      continue; /* Next triangle */
      break;
    default: FATAL("Invalid type.\n");
    }
    if (*side_desc_ptr != UINT_MAX && *side_desc_ptr != desc_id) {
      /* Already described with a different description! */
      fprintf(stderr, "Triangle %u %s from file %s with 2 different descriptions\n",
        tri_index,
        (file_type == CONTENT_CONNECTION
          ? "connection" : (current_side == SDIS_FRONT ? "front" : "back")),
        stl_filename);
      print_trg_as_obj(stderr, (const struct vertex*)stl_desc->vertices, stl_desc->indices + (3 * tri_index));
      return RES_BAD_ARG;
    }
    /* Everithing is OK: store description */
    *side_desc_ptr = desc_id;
  }
  ASSERT(drop_cpt == 0 || packed_indices != NULL);
  if (drop_cpt) {
    fprintf(stderr, "File '%s' includes %u degenerate triangles (dropped)\n",
      stl_filename, drop_cpt);
    /* Keep only valid triangles */
    ASSERT(sa_size(packed_indices) % 3 == 0);
    ASSERT(sa_size(packed_indices) == sa_size(packed_normals));
    ASSERT(sa_size(packed_indices) + 3 * drop_cpt == sa_size(stl_desc->indices));
    /* Don't sa_release stl_desc->indices or stl_desc->normals as they have
     * their own release mechanism */
    stl_desc->indices = packed_indices;
    stl_desc->normals = packed_normals;
    stl_desc->triangles_count = sa_size(stl_desc->indices) / 3;
  }
  else {
    sa_release(packed_indices);
    sa_release(packed_normals);
    packed_indices = NULL;
    packed_normals = NULL;
  }

end:
  sa_release(packed_indices);
  sa_release(packed_normals);

  return res;
  error:
    goto end;
}

static res_T
read_stl
  (const unsigned desc_id,
   const enum content_type file_type,
   const char* stl_filename,
   struct htable_vertex* vertex2id,
   struct htable_triangle* triangle2id,
   struct stardis* stardis)
{
  res_T res = RES_OK;
  struct sstl* sstl = NULL;
  struct sstl_desc stl_desc;
  unsigned* id2id = NULL;

  ASSERT(stl_filename && vertex2id && triangle2id && stardis);

  if (!stl_filename) return RES_BAD_ARG;

  SSTL(create(NULL, &stardis->allocator, 0, &sstl));

  res = sstl_load(sstl, stl_filename);
  if (res != RES_OK) {
    fprintf(stderr, "Cannot read STL file: %s\n", stl_filename);
    goto error;
  }
  SSTL(get_desc(sstl, &stl_desc));

  res = read_vertices(&stl_desc, vertex2id, &id2id, &stardis->geometry);
  if (res != RES_OK) goto error;
  res = read_triangles(&stl_desc, triangle2id, stl_filename, id2id, file_type,
    desc_id, &stardis->geometry, &stardis->boundary);
  if (res != RES_OK) goto error;
  
end:
  sa_release(id2id);
  SSTL(ref_put(sstl));
  return res;
error:
  goto end;
}

static res_T
geometry_analyse
  (const char* medium_filename,
   const char* bc_filename,
   const char* bound_filename, /* Can be NULL */
   struct stardis* stardis)
{
  res_T res = RES_OK;
  FILE* input = NULL;
  char* line = NULL;
  unsigned sz;
  struct htable_vertex vertex2id;
  struct htable_triangle triangle2id;
  struct htable_descriptions descriptions;
  int tables_initialised = 0;
  unsigned desc_id;
  unsigned *p_desc;

  ASSERT(medium_filename && bc_filename && stardis);

  sz = (unsigned)sa_size(stardis->descriptions);
  ASSERT(sz == sa_size(stardis->descriptions));

  stardis->geometry = NULL_GEOMETRY;

  /* parse medium files */
  input = fopen(medium_filename,"r");
  if (!input){
    fprintf(stderr,"Cannot open %s\n", medium_filename);
    res = RES_IO_ERR;
    goto error;
  }

  htable_vertex_init(&stardis->allocator, &vertex2id);
  htable_triangle_init(&stardis->allocator, &triangle2id);
  htable_descriptions_init(&stardis->allocator, &descriptions);
  tables_initialised = 1;
  /* loop on media */
  while (read_line(&line, input)){
    char* stl_filename = NULL;
    struct description desc;
    
    init_description(&desc);
    res = parse_medium_line(line, &stl_filename, &desc);
    if (res != RES_OK) goto error;

    /* Deduplicate media: find if the same description already exists
     * (including name) */
    p_desc = htable_descriptions_find(&descriptions, &desc);
    if (!p_desc) {
      sa_push(stardis->descriptions, desc);
      desc_id = sz++;
      ASSERT(sz == sa_size(stardis->descriptions));
      /* Register new medium description */
      res = htable_descriptions_set(&descriptions, &desc, &desc_id);
      if (res != RES_OK) goto error;
    } else {
      fprintf(stderr, "Duplicate media description found: deduplicated.\n");
      desc_id = *p_desc;
    }

    res = read_stl(sz-1, CONTENT_GEOMETRY, stl_filename, &vertex2id,
      &triangle2id, stardis);
    if (stl_filename) free(stl_filename);
    if (res != RES_OK) goto error;
  }
  fclose(input);
  input = NULL;

  /* parse boundary files */
  input = fopen(bc_filename,"r");
  if (!input){
    fprintf(stderr,"Cannot open %s\n", bc_filename);
    res = RES_IO_ERR;
    goto error;
  }

  /* loop on boundaries */
  while (read_line(&line, input)){
    char* stl_filename = NULL;
    struct description desc;

    init_description(&desc);
    res = parse_boundary_line(line, &stl_filename, &desc);
    if (res != RES_OK) goto error;
    
    /* Deduplicate media: find if the same description already exists
     * (including name) */
    p_desc = htable_descriptions_find(&descriptions, &desc);
    if (!p_desc) {
      sa_push(stardis->descriptions, desc);
      desc_id = sz++;
      ASSERT(sz == sa_size(stardis->descriptions));
      /* Register new boundary description */
      res = htable_descriptions_set(&descriptions, &desc, &desc_id);
      if (res != RES_OK) goto error;
    } else {
      fprintf(stderr, "Duplicate boundary description found: deduplicated.\n");
      desc_id = *p_desc;
    }
    
    res = read_stl(desc_id, CONTENT_CONNECTION, stl_filename, &vertex2id,
      &triangle2id, stardis);
    if (stl_filename) free(stl_filename);
    if (res != RES_OK) goto error;
  }
  fclose(input);
  input = NULL;

  /* Process vtk file describing boundary compute */
  if (bound_filename) {
    res = read_stl(0, CONTENT_BOUNDARY, bound_filename, &vertex2id,
      &triangle2id, stardis);
    if (res != RES_OK) goto error;
  }

exit:
  if(input) fclose(input);
  if (tables_initialised) {
    htable_descriptions_release(&descriptions);
    htable_vertex_release(&vertex2id);
    htable_triangle_release(&triangle2id);
  }
  sa_release(line);
  return res;
error:
  goto exit;
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
stardis_init
  (const struct args* args,
   struct stardis* stardis)
{
  res_T res = RES_OK;

  ASSERT(args && stardis);

  res = mem_init_proxy_allocator(&stardis->allocator,
    &mem_default_allocator);
  if (res != RES_OK) goto error;
  stardis->allocator_initialized = 1;

  res = geometry_analyse(args->medium_filename,
      args->bc_filename, args->solve_boundary_filename, stardis);
  if (res != RES_OK) goto error;

  stardis->N = args->N;
  stardis->nthreads = args->nthreads;
  stardis->probe[0] = args->u.probe[0];
  stardis->probe[1] = args->u.probe[1];
  stardis->probe[2] = args->u.probe[2];
  stardis->probe[3] = args->u.probe[3];
  stardis->scale_factor = args->scale_factor;
  stardis->radiative_temp[0] = args->radiative_temp[0];
  stardis->radiative_temp[1] = args->radiative_temp[1];
  stardis->dump_paths = SDIS_HEAT_PATH_NONE;
  if (args->dump_paths & DUMP_ERROR)
    stardis->dump_paths |= SDIS_HEAT_PATH_FAILED;
  if (args->dump_paths & DUMP_SUCCESS)
    stardis->dump_paths |= SDIS_HEAT_PATH_SUCCEED;

 if (args->mode & IR_COMPUTE){
    res = parse_camera(args->camera, &stardis->camera);
    if (res != RES_OK) goto error;
 }
 else if (args->mode & MEDIUM_COMPUTE) {
   strcpy(stardis->solve_name, args->medium_name);
 }
 else if (args->mode & BOUNDARY_COMPUTE) {
   strcpy(stardis->solve_name, args->solve_boundary_filename);
 }

exit:
  return res;
error:
  goto exit;
}

void
stardis_release(struct stardis* stardis)
{
  size_t memsz, i = 0;

  if(!stardis) return;

  if (stardis->geometry.interfaces) {
    for (i = 0; i < sa_size(stardis->geometry.interfaces); ++i)
      SDIS(interface_ref_put(stardis->geometry.interfaces[i]));
    sa_release(stardis->geometry.interfaces);
    stardis->geometry.interfaces = NULL;
  }
  sa_release(stardis->geometry.interf_bytrg);
  stardis->geometry.interf_bytrg = NULL;
  for (i=0; i<sa_size(stardis->descriptions); ++i) {
    struct description* desc = &stardis->descriptions[i];
    switch (desc->type) {
    case DESC_MAT_SOLID:
      free(desc->d.solid.power);
      free(desc->d.solid.Tinit);
      free(desc->d.solid.Temp);
      break;
    case DESC_MAT_FLUID:
      free(desc->d.fluid.Tinit);
      free(desc->d.fluid.Temp);
      break;
    case DESC_BOUND_H_FOR_SOLID:
    case DESC_BOUND_H_FOR_FLUID:
      free(desc->d.h_boundary.T);
      break;
    case DESC_BOUND_T_FOR_SOLID:
    case DESC_BOUND_T_FOR_FLUID:
      free(desc->d.t_boundary.T);
      te_free(desc->d.t_boundary.te_temperature);
      break;
    case DESC_BOUND_F_FOR_SOLID:
      free(desc->d.f_boundary.flux);
      te_free(desc->d.f_boundary.te_flux);
      break;
    default: FATAL("Invalid type.\n");
    }
  }
  sa_release(stardis->descriptions);
  release_geometry(&stardis->geometry);
  sa_release(stardis->boundary.primitives);
  sa_release(stardis->boundary.sides);

  if (stardis->allocator_initialized
    && (memsz = MEM_ALLOCATED_SIZE(&stardis->allocator)) != 0)
  {
    char dump[4096] = { '\0' };
    MEM_DUMP(&stardis->allocator, dump, sizeof(dump));
    fprintf(stderr, "%s\n", dump);
    fprintf(stderr, "Memory leaks: %lu Bytes\n", (unsigned long)memsz);
  }
  if (stardis->allocator_initialized) {
    mem_shutdown_proxy_allocator(&stardis->allocator);
    stardis->allocator_initialized = 0;
  }
}
