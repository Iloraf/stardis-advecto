/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com) */

#define _GNU_SOURCE 1
#include <string.h>
#include <ctype.h>
#include "stardis-app.h"


#include <rsys/hash_table.h>
#define HTABLE_NAME descriptions
#define HTABLE_KEY struct description
#define HTABLE_DATA unsigned
#define HTABLE_KEY_FUNCTOR_EQ eq_description
#define HTABLE_KEY_FUNCTOR_HASH hash_description
#include <rsys/hash_table.h>

/*******************************************************************************
 *
 ******************************************************************************/
static INLINE char*
read_line(char** pb, FILE* stream)
{
  const int chunk_sz = 32;
  char* buf = *pb;
  size_t idx;

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

static char** split_line(char* a_str, const char a_delim)
{
  char** result    = 0;
  size_t count     = 0;
  char* tmp        = a_str;
  char* last_comma = 0;
  char delim[2];
  delim[0] = a_delim;
  delim[1] = 0;

  /* Count how many elements will be extracted. */
  while (*tmp)
  {
    if (a_delim == *tmp)
    {
      count++;
      last_comma = tmp;
    }
    tmp++;
  }

  /* Add space for trailing token. */
  count += last_comma < (a_str + strlen(a_str) - 1);

  /* Add space for terminating null string so caller
     knows where the list of returned strings ends. */
  count++;

  result = malloc(sizeof(char*) * count);

  if (result)
  {
    size_t idx  = 0;
    char* token = strtok(a_str, delim);

    while (token)
    {
      ASSERT(idx < count);
#ifdef COMPILER_CL
      *(result + idx++) = _strdup(token);
#else
      *(result + idx++) = strdup(token);
#endif
      token = strtok(0, delim);
    }
    ASSERT(idx == count - 1);
    *(result + idx) = 0;
  }

  return result;
}

#ifdef COMPILER_GCC
static char*
_strupr(char* s)
{
  char* ptr;
  for (ptr = s; *ptr; ++ptr) {
    int tmp = toupper(*ptr);
    ASSERT(tmp == (char)tmp);
    *ptr = (char)tmp;
  }
  return s;
}
#endif


/* Read medium line; should be one of:
 * SOLID medium_name STL_filename lambda rho cp delta "Tinit(x,y,z)" [ "volumic_power(x,y,z,t)" ]
 * FLUID medium_name STL_filename rho cp "Tinit(x,y,z)"
*/
static res_T
parse_medium_line(char* line, char** stl_filename, struct description* desc)
{
  char* tk = NULL;
  res_T res = RES_OK;

#define CHK_TOK(x, Name) if ((tk = (x)) == NULL) {\
   fprintf(stderr, "Invalid data (missing token " Name ")\n");\
   res = RES_BAD_ARG;\
   goto exit;\
  } (void)0
  CHK_TOK(_strupr(strtok(line, " ")), "medium type");
  if (strcmp(tk, "SOLID") == 0) {
    desc->type = DESC_MAT_SOLID;

    CHK_TOK(strtok(NULL, " "), "medium name");
    if (strlen(tk) >= sizeof(desc->name)) {
      fprintf(stderr, "Medium name is too long: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    strncpy(desc->name, tk, sizeof(desc->name));

    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    CHK_TOK(strtok(NULL, " "), "lambda");
    res = cstr_to_double(tk, &desc->d.solid.lambda);
    if (res != RES_OK || desc->d.solid.lambda <= 0) {
      fprintf(stderr, "Invalid lambda: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "rho");
    res = cstr_to_double(tk, &desc->d.solid.rho);
    if (res != RES_OK || desc->d.solid.rho <= 0) {
      fprintf(stderr, "Invalid rho: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "cp");
    res = cstr_to_double(tk, &desc->d.solid.cp);
    if (res != RES_OK || desc->d.solid.cp <= 0) {
      fprintf(stderr, "Invalid cp: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "delta");
    res = cstr_to_double(tk, &desc->d.solid.delta);
    if (res != RES_OK || desc->d.solid.delta <= 0) {
      fprintf(stderr, "Invalid delta: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    /* Depending of the number of spaces before '"' strtok should
     * be called once or twice */
    CHK_TOK(strtok(NULL, "\""), "Tinit");
    if(*(tk + strspn(tk, " \t")) == '\0')
      CHK_TOK(strtok(NULL, "\""), "Tinit");
    desc->d.solid.Tinit = malloc(strlen(tk) + 1);
    strcpy(desc->d.solid.Tinit, tk);
    /* Closing " fot Tinit; can return NULL if line ends just after "Tinit" */
    tk = strtok(NULL, "\"");

    /* Volumic Power is optional */
    desc->d.solid.power = NULL;
    desc->d.solid.has_power = 0;
    if (tk && *(tk + strspn(tk, " \t")) == '\0') {
      /* Depending of the number of spaces before '"' strtok should
       * be called once or twice */
      tk = strtok(NULL, "\"");
    }
    if (tk) {
      desc->d.solid.power = malloc(strlen(tk) + 1);
      strcpy(desc->d.solid.power, tk);
      /* Can be changed aftwerwards if compiled to constant 0  */
      desc->d.solid.has_power = 1;
    }
  }
  else if (strcmp(tk, "FLUID") == 0) {
    desc->type = DESC_MAT_FLUID;

    CHK_TOK(strtok(NULL, " "), "medium name");
    if (strlen(tk) >= sizeof(desc->name)) {
      fprintf(stderr, "Medium name is too long: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    strncpy(desc->name, tk, sizeof(desc->name));

    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    CHK_TOK(strtok(NULL, " "), "rho");
    res = cstr_to_double(tk, &desc->d.fluid.rho);
    if (res != RES_OK || desc->d.fluid.rho <= 0) {
      fprintf(stderr, "Invalid rho: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "cp");
    res = cstr_to_double(tk, &desc->d.fluid.cp);
    if (res != RES_OK || desc->d.fluid.cp <= 0) {
      fprintf(stderr, "Invalid cp: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    /* Depending of the number of spaces before '"' strtok should
     * be called once or twice */
    CHK_TOK(strtok(NULL, "\""), "Tinit");
    if (*(tk + strspn(tk, " \t")) == '\0')
      CHK_TOK(strtok(NULL, "\""), "Tinit");
    desc->d.fluid.Tinit = malloc(strlen(tk) + 1);
    strcpy(desc->d.fluid.Tinit, tk);
  }
  else {
    res = RES_BAD_ARG;
    fprintf(stderr, "Invalid medium type: %s\n", tk);
  }

#undef CHK_TOK
exit:
  return res;
}

/* Read boundary line; should be one of:
#   H_BOUNDARY_FOR_SOLID STL_filename emissivity specular_fraction hc hc_max "T_env(x,y,z,t)"
# | H_BOUNDARY_FOR_FLUID STL_filename emissivity specular_fraction hc hc_max "T_env(x,y,z,t)"
# | T_BOUNDARY_FOR_SOLID STL_filename "T_value(x,y,z,t)"
# | T_BOUNDARY_FOR_FLUID STL_filename "T_value(x,y,z,t)" hc hc_max
# | F_BOUNDARY_FOR_SOLID STL_filename "F_value(x,y,z,t)"
# ; no F_BOUNDARY_FOR_FLUID
 */
static res_T
parse_boundary_line(char* line, char** stl_filename, struct description* desc)
{
  char* tk = NULL;
  int h_solid = 0, h_fluid = 0;
  int t_solid = 0, t_fluid = 0;
  int f_solid = 0;
  int sf_connect = 0;
  res_T res = RES_OK;

#define CHK_TOK(x, Name) if ((tk = (x)) == NULL) {\
   fprintf(stderr, "Invalid data (missing token " Name ")\n");\
   res = RES_BAD_ARG;\
   goto exit;\
  } (void)0
  CHK_TOK(strtok(line, " "), "boundary type");
  h_solid = (0 == strcmp(tk, "H_BOUNDARY_FOR_SOLID"));
  if (!h_solid) {
    h_fluid = (0 == strcmp(tk, "H_BOUNDARY_FOR_FLUID"));
    if (!h_fluid) {
      t_solid = (0 == strcmp(tk, "T_BOUNDARY_FOR_SOLID"));
      if (!t_solid) {
        t_fluid = (0 == strcmp(tk, "T_BOUNDARY_FOR_FLUID"));
        if (!t_fluid) {
          f_solid = (0 == strcmp(tk, "F_BOUNDARY_FOR_SOLID"));
          if (!f_solid) {
            sf_connect = (0 == strcmp(tk, "SOLID_FLUID_CONNECTION"));
          }
        }
      }
    }
  }
  if (h_solid || h_fluid) {
    desc->type = h_solid ? DESC_BOUND_H_FOR_SOLID : DESC_BOUND_H_FOR_FLUID;

    /* The description is parsed the same way for both types */
    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    CHK_TOK(strtok(NULL, " "), "emissivity");
    res = cstr_to_double(tk, &desc->d.h_boundary.emissivity);
    if (res != RES_OK ||
      desc->d.h_boundary.emissivity < 0 || desc->d.h_boundary.emissivity > 1) {
      fprintf(stderr, "Invalid emissivity: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    desc->d.h_boundary.has_emissivity = (desc->d.h_boundary.emissivity > 0);
    CHK_TOK(strtok(NULL, " "), "specular fraction");
    res = cstr_to_double(tk, &desc->d.h_boundary.specular_fraction);
    if (res != RES_OK
      || desc->d.h_boundary.specular_fraction < 0.0
      || desc->d.h_boundary.specular_fraction > 1.0) {
      fprintf(stderr, "Invalid specular_fraction: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "hc");
    res = cstr_to_double(tk, &desc->d.h_boundary.hc);
    if (res != RES_OK || desc->d.h_boundary.hc < 0) {
      fprintf(stderr, "Invalid hc value: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    desc->d.h_boundary.has_hc = (desc->d.h_boundary.hc > 0);
    CHK_TOK(strtok(NULL, " "), "hc max");
    res = cstr_to_double(tk, &desc->d.h_boundary.hc_max);
    if (res != RES_OK
      || desc->d.h_boundary.hc_max < desc->d.h_boundary.hc
      || desc->d.h_boundary.hc_max < 0) {
      fprintf(stderr, "Invalid hc_max value: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    /* Depending of the number of spaces before '"' strtok should
     * be called once or twice */
    CHK_TOK(strtok(NULL, "\""), "temperature");
    if (*(tk + strspn(tk, " \t")) == '\0')
      CHK_TOK(strtok(NULL, "\""), "temperature");
    desc->d.h_boundary.T = malloc(strlen(tk) + 1);
    strcpy(desc->d.h_boundary.T, tk);
  }
  else if (t_solid || t_fluid) {
    desc->type = t_solid ? DESC_BOUND_T_FOR_SOLID : DESC_BOUND_T_FOR_FLUID;

    /* This part of the description is parsed the same way for both types */
    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    /* Depending of the number of spaces before '"' strtok should
     * be called once or twice */
    CHK_TOK(strtok(NULL, "\""), "temperature");
    if (*(tk + strspn(tk, " \t")) == '\0')
      CHK_TOK(strtok(NULL, "\""), "temperature");
    desc->d.t_boundary.T = malloc(strlen(tk) + 1);
    strcpy(desc->d.t_boundary.T, tk);

    /* Parse hc + hc_max only if fluid */
    if (t_fluid) {
      CHK_TOK(strtok(NULL, " "), "hc");
      res = cstr_to_double(tk, &desc->d.t_boundary.hc);
      if (res != RES_OK || desc->d.t_boundary.hc < 0) {
        fprintf(stderr, "Invalid hc value: %s\n", tk);
        res = RES_BAD_ARG;
        goto exit;
      }
      desc->d.t_boundary.has_hc = (desc->d.t_boundary.hc > 0);
      CHK_TOK(strtok(NULL, " "), "hc max");
      res = cstr_to_double(tk, &desc->d.t_boundary.hc_max);
      if (res != RES_OK
        || desc->d.t_boundary.hc_max < desc->d.t_boundary.hc
        || desc->d.t_boundary.hc_max < 0) {
        fprintf(stderr, "Invalid hc_max value: %s\n", tk);
        res = RES_BAD_ARG;
        goto exit;
      }
    }
    else desc->d.t_boundary.has_hc = 0;
  }
  else if (f_solid) {
    desc->type = DESC_BOUND_F_FOR_SOLID;

    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    /* Depending of the number of spaces before '"' strtok should
     * be called once or twice */
    CHK_TOK(strtok(NULL, "\""), "flux");
    if (*(tk + strspn(tk, " \t")) == '\0')
      CHK_TOK(strtok(NULL, "\""), "flux");
    desc->d.f_boundary.flux = malloc(strlen(tk) + 1);
    strcpy(desc->d.f_boundary.flux, tk);

    /* hc and hc_max are optional */
    tk = strtok(NULL, " ");
    if (tk) {
      res = cstr_to_double(tk, &desc->d.f_boundary.hc);
      if (res != RES_OK || desc->d.f_boundary.hc < 0) {
        fprintf(stderr, "Invalid hc value: %s\n", tk);
        res = RES_BAD_ARG;
        goto exit;
      }
      desc->d.f_boundary.has_hc = (desc->d.f_boundary.hc > 0);
      CHK_TOK(strtok(NULL, " "), "hc max");
      res = cstr_to_double(tk, &desc->d.f_boundary.hc_max);
      if (res != RES_OK
        || desc->d.f_boundary.hc_max < desc->d.f_boundary.hc
        || desc->d.f_boundary.hc_max < 0) {
        fprintf(stderr, "Invalid hc_max value: %s\n", tk);
        res = RES_BAD_ARG;
        goto exit;
      }
    }
  }
  else if (sf_connect) {
    desc->type = DESC_SOLID_FLUID_CONNECT;

    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    CHK_TOK(strtok(NULL, " "), "emissivity");
    res = cstr_to_double(tk, &desc->d.sf_connect.emissivity);
    if (res != RES_OK ||
      desc->d.sf_connect.emissivity < 0 || desc->d.sf_connect.emissivity > 1) {
      fprintf(stderr, "Invalid emissivity: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    desc->d.sf_connect.has_emissivity = (desc->d.sf_connect.emissivity > 0);
    CHK_TOK(strtok(NULL, " "), "specular fraction");
    res = cstr_to_double(tk, &desc->d.sf_connect.specular_fraction);
    if (res != RES_OK
      || desc->d.sf_connect.specular_fraction < 0.0
      || desc->d.sf_connect.specular_fraction > 1.0) {
      fprintf(stderr, "Invalid specular_fraction: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "hc");
    res = cstr_to_double(tk, &desc->d.sf_connect.hc);
    if (res != RES_OK || desc->d.sf_connect.hc < 0) {
      fprintf(stderr, "Invalid hc value: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    desc->d.sf_connect.has_hc = (desc->d.sf_connect.hc > 0);
    CHK_TOK(strtok(NULL, " "), "hc max");
    res = cstr_to_double(tk, &desc->d.h_boundary.hc_max);
    if (res != RES_OK
      || desc->d.h_boundary.hc_max < desc->d.h_boundary.hc
      || desc->d.h_boundary.hc_max < 0) {
      fprintf(stderr, "Invalid hc_max value: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
  }
  else {
  fprintf(stderr, "Invalid boundary type: %s", tk);
    res = RES_BAD_ARG;
    goto exit;
  }

#undef CHK_TOK
exit:
  return res;
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
      htable_vertex_set(vertex2id, &vtx, &size);
      sa_push(geometry->vertex, vtx);
      sa_push(*id2id, size);
    }
  }

  return res;
}

static int
indices_to_key(struct unsigned3* key, const struct unsigned3* indices)
{
  int i, reversed = 0;
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
      htable_triangle_set(triangle2id, &key, &id);
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
      fprintf(stderr, "Triangle %u %s with 2 different descriptions\n",
        tri_index,
        (file_type == CONTENT_CONNECTION
          ? "connection" : (current_side == SDIS_FRONT ? "front" : "back")));
      print_trg_as_obj(stderr, (const struct vertex*)stl_desc->vertices, &tri);
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
  }

  return res;
}

res_T
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

/*******************************************************************************
 *
 ******************************************************************************/

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
  unsigned sz = (unsigned)sa_size(stardis->descriptions);
  struct htable_vertex vertex2id;
  struct htable_triangle triangle2id;
  struct htable_descriptions descriptions;
  int tables_initialised = 0;
  unsigned desc_id;
  unsigned *p_desc;
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
      htable_descriptions_set(&descriptions, &desc, &desc_id);
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
      htable_descriptions_set(&descriptions, &desc, &desc_id);
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

static res_T
parse_camera
  (char* cam_param, struct camera* cam)
{
  char** line = NULL;
  char** opt = NULL;
  res_T res = RES_OK;

  line = split_line(cam_param,':');
  
  if (line)
  {
    int i = 0;
    for (i = 0; *(line + i); i++)
    {
      size_t len = 0;
      opt = split_line(line[i],'=');
      if (strcmp(opt[0], "t") == 0) {
        res = cstr_to_double(opt[1], &cam->u.time);
        if (res != RES_OK) goto error;
      } else if (strcmp(opt[0],"fov")==0){
        res = cstr_to_double(opt[1], &cam->fov);
        if (res != RES_OK) goto error;
      } else if (strcmp(opt[0],"up")==0) {
        res = cstr_to_list_double(opt[1], ',', cam->up, &len, 3);
        if (res != RES_OK || len != 3) goto error;
      } else if (strcmp(opt[0],"tgt")==0) {
        res = cstr_to_list_double(opt[1], ',', cam->tgt, &len, 3);
        if (res != RES_OK || len != 3) goto error;
      } else if (strcmp(opt[0],"pos")==0) {
        res = cstr_to_list_double(opt[1], ',', cam->pos, &len, 3);
        if (res != RES_OK || len != 3) goto error;
      } else if (strcmp(opt[0],"img")==0) {
        res = cstr_to_list_uint(opt[1], 'x', cam->img, &len, 2);
        if (res != RES_OK || len != 2) goto error;
      } else if (strcmp(opt[0],"spp")==0) {
        res = cstr_to_uint(opt[1], &cam->spp);
        if (res != RES_OK) goto error;
      } 
    }
  }

exit:
  FREE_AARRAY(line)
  FREE_AARRAY(opt)
 
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

  res = mem_init_proxy_allocator(&stardis->allocator,
    &mem_default_allocator);
  if (res != RES_OK) goto error;

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

 if (args->mode == IR_COMPUTE){
    res = parse_camera(args->camera, &stardis->camera);
    if (res != RES_OK) goto error;
 }
 else if (args->mode == MEDIUM_COMPUTE) {
   strcpy(stardis->solve_name, args->medium_name);
 }
 else if (args->mode == BOUNDARY_COMPUTE) {
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
  size_t i = 0;

  for (i=0; i<sa_size(stardis->descriptions); ++i) {
    struct description* desc = &stardis->descriptions[i];
    switch (desc->type) {
    case DESC_MAT_SOLID:
      free(desc->d.solid.power);
      free(desc->d.solid.Tinit);
      break;
    case DESC_MAT_FLUID:
      free(desc->d.fluid.Tinit);
      break;
    case DESC_BOUND_H_FOR_SOLID:
    case DESC_BOUND_H_FOR_FLUID:
      free(desc->d.h_boundary.T);
      break;
    case DESC_BOUND_T_FOR_SOLID:
    case DESC_BOUND_T_FOR_FLUID:
      free(desc->d.t_boundary.T);
      break;
    case DESC_BOUND_F_FOR_SOLID:
      free(desc->d.f_boundary.flux);
      break;
    default: FATAL("Invalid type.\n");
    }
  }
  sa_release(stardis->descriptions);
  release_geometry(&stardis->geometry);
  sa_release(stardis->boundary.primitives);
  sa_release(stardis->boundary.sides);
}

/* TODO: rewrite dump! */
res_T
dump_vtk
  (FILE* output,
   const struct geometry* geometry)
{
  res_T res = RES_OK;
  unsigned i;
  struct vertex* vtx = geometry->vertex;
  struct triangle* tri = geometry->triangle;
  
  if (!output){
    fprintf(stderr, "Invalid output file to dump vtk.\n");
    res = RES_BAD_ARG;
    goto error;
  }

  fprintf(output,"# vtk DataFile Version 2.0\nvtk output\nASCII\nDATASET POLYDATA\n");
  fprintf(output,"POINTS %i float\n\n", (int)sa_size(vtx));
  for (i=0; i<sa_size(vtx); ++i){
    fprintf(output,"%f %f %f\n",SPLIT3(vtx[i].xyz));
  }
  fprintf(output,"\nPOLYGONS %i %i\n", (int)sa_size(tri), (int)(4*sa_size(tri)));
  for (i=0; i<sa_size(tri); ++i){
    fprintf(output,"3 %i %i %i\n",SPLIT3(tri[i].indices.data));
  }
  fprintf(output,"\nCELL_DATA %i \n", (int)sa_size(tri));
  fprintf(output,"SCALARS front_description float 1\n");
  fprintf(output,"LOOKUP_TABLE default\n");
  for (i=0; i<sa_size(tri); ++i) {
    fprintf(output,"%i\n",tri[i].front_description);
  }
  fprintf(output,"SCALARS back_description float 1\n");
  fprintf(output,"LOOKUP_TABLE default\n");
  for (i=0; i<sa_size(tri); ++i) {
    fprintf(output,"%i\n",tri[i].back_description);
  }

exit:
  return res;
error:
  goto exit;
}
