/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef STARDIS_APP_H
#define STARDIS_APP_H

#include "stardis-parsing.h"

#include <star/sstl.h>
#include <rsys/rsys.h>
#include <rsys/float3.h>
#include <rsys/stretchy_array.h>
#include <rsys/hash_table.h>
#include <sdis.h>
#include <tinyexpr.h>

#include <limits.h>

#define FREE_AARRAY(ARRAY) if (ARRAY) {\
  int i = 0; \
  for (i=0; *(ARRAY+i);i++){\
    free(ARRAY[i]);\
  }\
  free(ARRAY);\
}

/* Different types of descriptions */
enum description_type {
  DESC_MAT_SOLID,
  DESC_MAT_FLUID,
  DESC_BOUND_H_FOR_FLUID,
  DESC_BOUND_H_FOR_SOLID,
  DESC_BOUND_T_FOR_FLUID,
  DESC_BOUND_T_FOR_SOLID,
  DESC_BOUND_F_FOR_SOLID,
  DESC_SOLID_FLUID_CONNECT,
  DESCRIPTION_TYPE_COUNT__,
  DESC_OUTSIDE
};

struct vertex{
  float xyz[3];
};
#define NULL_VERTEX__ {{0.0 ,0.0 ,0.0}}
static const struct vertex NULL_VERTEX = NULL_VERTEX__;

static INLINE char
eq_vertex(const struct vertex* a, const struct vertex* b)
{
  return (char)f3_eq(a->xyz, b->xyz);
}

/* Declare the hash table that map a vertex to its index */
#define HTABLE_NAME vertex
#define HTABLE_DATA unsigned
#define HTABLE_KEY struct vertex
#define HTABLE_KEY_FUNCTOR_EQ eq_vertex
#include <rsys/hash_table.h>

struct unsigned3 { unsigned data[3]; };
struct triangle {
  struct unsigned3 indices;
  unsigned front_description;
  unsigned back_description;
  unsigned connection_description;
};
#define NULL_TRIANGLE__ {{{0, 0, 0}}, UINT_MAX, UINT_MAX, UINT_MAX }
static const struct triangle NULL_TRIANGLE = NULL_TRIANGLE__;

static INLINE char
eq_indices(const struct unsigned3* a, const struct unsigned3* b)
{  
  return a->data[0] == b->data[0]
    && a->data[1] == b->data[1]
    && a->data[2] == b->data[2];
}

static INLINE res_T
cp_indices(struct unsigned3* dst, const struct unsigned3* src)
{
  int i;
  FOR_EACH(i, 0, 3) dst->data[i] = src->data[i];
  return RES_OK;
}

/* Declare the hash table that maps a triangle id to its index */
#define HTABLE_NAME triangle 
#define HTABLE_DATA unsigned
#define HTABLE_KEY struct unsigned3
#define HTABLE_KEY_FUNCTOR_EQ eq_indices
#define HTABLE_KEY_FUNCTOR_COPY cp_indices
#include <rsys/hash_table.h>

struct geometry {
  struct vertex* vertex;
  struct triangle* triangle;
  struct sdis_interface** interf_bytrg;
  struct sdis_interface** interfaces;
};
#define NULL_GEOMETRY__ {NULL, NULL, NULL, NULL }
static const struct geometry NULL_GEOMETRY = NULL_GEOMETRY__;

static INLINE void
release_geometry(struct geometry* geom)
{
  size_t i;
  for (i = 0; i < sa_size(geom->interfaces); ++i)
    SDIS(interface_ref_put(geom->interfaces[i]));
  sa_release(geom->vertex); geom->vertex = NULL;
  sa_release(geom->triangle); geom->triangle = NULL;
  sa_release(geom->interfaces); geom->interfaces = NULL;
  sa_release(geom->interf_bytrg); geom->interf_bytrg = NULL;
}

struct mat_fluid {
  char name[32];
  unsigned fluid_id;
  double rho;
  double cp;
  char* Tinit;
  char* Temp;
};
#define NULL_FLUID__ { "", UINT_MAX, 0, 0, NULL, NULL }
static const struct mat_fluid NULL_FLUID = NULL_FLUID__;

static void
print_fluid(FILE* stream, const struct mat_fluid* f)
{
  ASSERT(stream && f);
  fprintf(stream, "Fluid '%s': cp=%g rho=%g",
    f->name, f->cp, f->rho);
  if (f->Tinit) fprintf(stream, " Tinit='%s'", f->Tinit);
  if (f->Temp) fprintf(stream, " Temp='%s'", f->Temp);
  fprintf(stream, "\n");
}

static char
eq_fluid(const struct mat_fluid* a, const struct mat_fluid* b)
{
  if (strcmp(a->name, b->name)
    || a->fluid_id != b->fluid_id
    || a->rho != b->rho
    || a->cp != b->cp
    || strcmp(a->Tinit, b->Tinit)
    || strcmp(a->Temp, b->Temp))
    return 0;
  return 1;
}

static FINLINE res_T
cp_fluid(struct mat_fluid* dst, const struct mat_fluid* src)
{
  strcpy(dst->name, src->name);
  dst->fluid_id = src->fluid_id;
  dst->rho = src->rho;
  dst->cp = src->cp;
  if (!src->Tinit) dst->Tinit = NULL;
  else {
    dst->Tinit = malloc(sizeof(src->Tinit) + 1);
    if (!dst->Tinit) return RES_MEM_ERR;
    strcpy(dst->Tinit, src->Tinit);
  }
  if (!src->Temp) dst->Temp = NULL;
  else {
    dst->Temp = malloc(sizeof(src->Temp) + 1);
    if (!dst->Temp) return RES_MEM_ERR;
    strcpy(dst->Temp, src->Temp);
  }
  return RES_OK;
}

static FINLINE res_T
cp_release_fluid(struct mat_fluid* dst, struct mat_fluid* src)
{
  strcpy(dst->name, src->name);
  dst->fluid_id = src->fluid_id;
  dst->rho = src->rho;
  dst->cp = src->cp;
  dst->Tinit = src->Tinit; src->Tinit = NULL;
  dst->Temp = src->Temp; src->Temp = NULL;
  return RES_OK;
}

static FINLINE res_T
release_fluid(struct mat_fluid* a)
{
  free(a->Tinit);
  free(a->Temp);
  return RES_OK;
}

static size_t
hash_fluid(const struct mat_fluid* key)
{
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->name)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->name)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->fluid_id)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->fluid_id)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->rho)) {
    hash = hash ^ (uint64_t) ((unsigned char)((const char*)&key->rho)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->cp)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->cp)[i]);
    hash = hash * FNV64_PRIME;
  }
  hash = hash ^ (uint64_t)((unsigned char)(key->Tinit == 0));
  hash = hash * FNV64_PRIME;
  FOR_EACH(i, 0, key->Tinit ? strlen(key->Tinit)*sizeof(*key->Tinit) : 0) {
    hash = hash ^ (uint64_t)((unsigned char) ((const char*)key->Tinit)[i]);
    hash = hash * FNV64_PRIME;
  }
  hash = hash ^ (uint64_t)((unsigned char)(key->Temp == 0));
  hash = hash * FNV64_PRIME;
  FOR_EACH(i, 0, key->Temp ? strlen(key->Temp) * sizeof(*key->Temp) : 0) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)key->Temp)[i]);
    hash = hash * FNV64_PRIME;
  }
  return hash;
}

struct mat_solid {
  char name[32];
  unsigned solid_id;
  double lambda;
  double rho;
  double cp;
  double delta;
  char* Tinit;
  char* Temp;
  char* power;
  int has_power;
};
#define NULL_SOLID__ { "", UINT_MAX, 0, 0, 0, 0, NULL, NULL, NULL, 0}
static const struct mat_solid NULL_SOLID = NULL_SOLID__;

static void
print_solid(FILE* stream, const struct mat_solid* s)
{
  ASSERT(stream && s);
  fprintf(stream,
    "Solid '%s': lambda=%g cp=%g rho=%g delta=%g Power='%s'",
    s->name, s->lambda, s->cp, s->rho, s->delta,
    (s->has_power ? s->power : "0"));
  if (s->Tinit) fprintf(stream, " Tinit='%s'", s->Tinit);
  if (s->Temp) fprintf(stream, " Temp='%s'", s->Temp);
  fprintf(stream, "\n");
}

static char
eq_solid(const struct mat_solid* a, const struct mat_solid* b)
{
  if (strcmp(a->name, b->name)
    || a->solid_id != b->solid_id
    || a->lambda != b->lambda
    || a->rho != b->rho
    || a->cp != b->cp
    || a->delta != b->delta
    || strcmp(a->Tinit, b->Tinit)
    || strcmp(a->Temp, b->Temp)
    || a->has_power != b->has_power)
    return 0;
  if (a->has_power && a->power != b->power) return 0;
  return 1;
}

static FINLINE res_T
cp_solid(struct mat_solid* dst, const struct mat_solid* src)
{
  strcpy(dst->name, src->name);
  dst->solid_id = src->solid_id;
  dst->lambda = src->lambda;
  dst->rho = src->rho;
  dst->cp = src->cp;
  dst->delta = src->delta;
  if (!src->Tinit) dst->Tinit = NULL;
  else {
    dst->Tinit = malloc(sizeof(src->Tinit) + 1);
    if (!dst->Tinit) return RES_MEM_ERR;
    strcpy(dst->Tinit, src->Tinit);
  }
  if (!src->Temp) dst->Temp = NULL;
  else {
    dst->Temp = malloc(sizeof(src->Temp) + 1);
    if (!dst->Temp) return RES_MEM_ERR;
    strcpy(dst->Temp, src->Temp);
  }
  dst->has_power = src->has_power;
  if (!src->power) dst->power = NULL;
  else {
    dst->power = malloc(sizeof(src->power) + 1);
    if (!dst->power) return RES_MEM_ERR;
    strcpy(dst->power, src->power);
  }
  return RES_OK;
}

static FINLINE res_T
cp_release_solid(struct mat_solid* dst, struct mat_solid* src)
{
  strcpy(dst->name, src->name);
  dst->solid_id = src->solid_id;
  dst->lambda = src->lambda;
  dst->rho = src->rho;
  dst->cp = src->cp;
  dst->delta = src->delta;
  dst->Tinit = src->Tinit; src->Tinit = NULL;
  dst->Temp = src->Temp; src->Temp = NULL;
  dst->has_power = src->has_power;
  dst->power = src->power; src->power = NULL;
  return RES_OK;
}

static FINLINE res_T
release_solid(struct mat_solid* a)
{
  free(a->power);
  free(a->Tinit);
  free(a->Temp);
  return RES_OK;
}

static size_t
hash_solid(const struct mat_solid* key)
{
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->name)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->name)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->solid_id)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->solid_id)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->lambda)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->lambda)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->rho)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->rho)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->cp)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->cp)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->delta)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->delta)[i]);
    hash = hash * FNV64_PRIME;
  }
  hash = hash ^ (uint64_t)((unsigned char)(key->Tinit == 0));
  hash = hash * FNV64_PRIME;
  FOR_EACH(i, 0, key->Tinit ? strlen(key->Tinit) * sizeof(*key->Tinit) : 0) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)key->Tinit)[i]);
    hash = hash * FNV64_PRIME;
  }
  hash = hash ^ (uint64_t)((unsigned char)(key->Temp == 0));
  hash = hash * FNV64_PRIME;
  FOR_EACH(i, 0, key->Temp ? strlen(key->Temp) * sizeof(*key->Temp) : 0) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)key->Temp)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_power)) {
    hash = hash ^ (uint64_t) ((unsigned char) ((const char*) &key->has_power)[i]);
    hash = hash * FNV64_PRIME;
  }
  if (!key->has_power) return hash;
  FOR_EACH(i, 0, strlen(key->power) * sizeof(*key->power)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)key->power)[i]);
    hash = hash * FNV64_PRIME;
  }
  return hash;
}

struct h_boundary {
  char name[32];
  unsigned mat_id;
  double emissivity;
  double specular_fraction;
  double hc;
  double hc_max;
  char* T;
  int has_emissivity, has_hc;
};
#define NULL_HBOUND__ { "", UINT_MAX, 0, 0, 0, 0, NULL, 0, 0}
static const struct h_boundary NULL_HBOUND = NULL_HBOUND__;

static void
print_h_boundary
  (FILE* stream,
   const struct h_boundary* b,
   const enum description_type type)
{
  ASSERT(stream && b
    && (type == DESC_BOUND_H_FOR_SOLID || type == DESC_BOUND_H_FOR_FLUID));
  fprintf(stream,
    "H boundary %s for %s: emissivity=%g specular_fraction=%g hc=%g hc_max=%g T='%s'\n",
    b->name,
    (type == DESC_BOUND_H_FOR_SOLID ? "solid" : "fluid"),
    (b->has_emissivity ? b->emissivity : 0),
    (b->has_emissivity ? b->specular_fraction : 0),
    (b->has_hc ? b->hc : 0),
    (b->has_hc ? b->hc_max : 0),
    (b->T ? b->T : "0"));
}

static char
eq_h_boundary
  (const struct h_boundary* a,
   const struct h_boundary* b)
{
  if (a->mat_id != b->mat_id
    || strcmp(a->name, b->name)
    || a->specular_fraction != b->specular_fraction
    || strcmp(a->T, b->T)
    || a->has_emissivity != b->has_emissivity
    || a->has_hc != b->has_hc)
    return 0;
  if (a->has_emissivity && a->emissivity != b->emissivity) return 0;
  if (a->has_hc && (a->hc != b->hc || a->hc_max != b->hc_max)) return 0;
  return 1;
}

static FINLINE res_T
cp_h_boundary(struct h_boundary* dst, const struct h_boundary* src)
{
  strcpy(dst->name, src->name);
  dst->mat_id = src->mat_id;
  dst->specular_fraction = src->specular_fraction;
  if (!src->T) dst->T = NULL;
  else {
    dst->T = malloc(sizeof(src->T) + 1);
    if (!dst->T) return RES_MEM_ERR;
    strcpy(dst->T, src->T);
  }
  dst->has_emissivity = src->has_emissivity;
  dst->has_hc = src->has_hc;
  dst->emissivity = src->emissivity;
  dst->hc_max = src->hc_max;
  dst->hc = src->hc;
  return RES_OK;
}

static FINLINE res_T
cp_release_h_boundary(struct h_boundary* dst, struct h_boundary* src)
{
  strcpy(dst->name, src->name);
  dst->mat_id = src->mat_id;
  dst->specular_fraction = src->specular_fraction;
  dst->T = src->T; src->T = NULL;
  dst->has_emissivity = src->has_emissivity;
  dst->has_hc = src->has_hc;
  dst->emissivity = src->emissivity;
  dst->hc_max = src->hc_max;
  dst->hc = src->hc;
  return RES_OK;
}

static FINLINE res_T
release_h_boundary(struct h_boundary* a)
{
  free(a->T);
  return RES_OK;
}

static size_t
hash_h_boundary(const struct h_boundary* key)
{
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->name)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->name)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->mat_id)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->mat_id)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->emissivity)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->emissivity)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->specular_fraction)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->specular_fraction)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, strlen(key->T) * sizeof(*key->T)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)key->T)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_emissivity)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->has_emissivity)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_hc)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) &key->has_hc)[i]);
    hash = hash * FNV64_PRIME;
  }
  if (!key->has_hc) return hash;
  FOR_EACH(i, 0, sizeof(key->hc)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) &key->hc)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->hc_max)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) &key->hc_max)[i]);
    hash = hash * FNV64_PRIME;
  }
  return hash;
}

struct t_boundary {
  char name[32];
  unsigned mat_id;
  char* T;
  struct te_expr* te_temperature;
  double hc;
  double hc_max;
  int has_hc;
};
#define NULL_TBOUND__ { "", UINT_MAX, NULL, NULL, 0, 0, 0}
static const struct t_boundary NULL_TBOUND = NULL_TBOUND__;

static void
print_t_boundary
  (FILE* stream,
   const struct t_boundary* b,
   const enum description_type type)
{
  ASSERT(stream && b
    && (type == DESC_BOUND_T_FOR_SOLID || type == DESC_BOUND_T_FOR_FLUID));
  fprintf(stream,
    "T boundary %s  for %s: hc=%g hc_max=%g T='%s'\n",
    b->name,
    (type == DESC_BOUND_T_FOR_SOLID ? "solid" : "fluid"),
    (b->has_hc ? b->hc : 0),
    (b->has_hc ? b->hc_max : 0),
    (b->T ? b->T : "0"));
}

static char
eq_t_boundary
  (const struct t_boundary* a,
   const struct t_boundary* b,
   const enum description_type type)
{
  ASSERT(type == DESC_BOUND_T_FOR_SOLID || type == DESC_BOUND_T_FOR_FLUID);
  /* These fields are meaningful  by both types */
  if (strcmp(a->name, b->name))
    return 0;
  if (a->mat_id != b->mat_id || strcmp(a->T, b->T))
    return 0;
  if (type != DESC_BOUND_T_FOR_FLUID)
    return 1;
  /* These ones are only relevant for fluids */
  if (a->has_hc != b->has_hc)
    return 0;
  if (a->has_hc && (a->hc != b->hc || a->hc_max != b->hc_max))
    return 0;
  return 1;
}

static FINLINE res_T
cp_t_boundary(struct t_boundary* dst, const struct t_boundary* src)
{
  strcpy(dst->name, src->name);
  dst->mat_id = src->mat_id;
  if (!src->T) dst->T = NULL;
  else {
    dst->T = malloc(sizeof(src->T) + 1);
    if (!dst->T) return RES_MEM_ERR;
    strcpy(dst->T, src->T);
  }
  dst->has_hc = src->has_hc;
  dst->hc_max = src->hc_max;
  dst->hc = src->hc;
  if (!src->te_temperature) dst->te_temperature = NULL;
  else {
    dst->te_temperature = te_duplicate(src->te_temperature);
    if (!dst->te_temperature) return RES_MEM_ERR;
  }
  return RES_OK;
}

static FINLINE res_T
cp_release_t_boundary(struct t_boundary* dst, struct t_boundary* src)
{
  strcpy(dst->name, src->name);
  dst->mat_id = src->mat_id;
  dst->T = src->T; src->T = NULL;
  dst->has_hc = src->has_hc;
  dst->hc_max = src->hc_max;
  dst->hc = src->hc;
  dst->te_temperature = src->te_temperature; src->te_temperature = NULL;
  return RES_OK;
}

static FINLINE res_T
release_t_boundary(struct t_boundary* a)
{
  free(a->T);
  te_free(a->te_temperature);
  return RES_OK;
}

static size_t
hash_t_boundary
  (const struct t_boundary* key,
   const enum description_type type)
{
  (void)type;
  ASSERT(type == DESC_BOUND_T_FOR_SOLID || type == DESC_BOUND_T_FOR_FLUID);
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->name)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->name)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->mat_id)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->mat_id)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_hc)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) &key->has_hc)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, strlen(key->T) * sizeof(*key->T)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) key->T)[i]);
    hash = hash * FNV64_PRIME;
  }
  if (!key->has_hc) return hash;
  FOR_EACH(i, 0, sizeof(key->hc)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->hc)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->hc_max)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->hc_max)[i]);
    hash = hash * FNV64_PRIME;
  }
  return hash;
}

struct f_boundary {
  char name[32];
  unsigned mat_id;
  char* flux;
  struct te_expr* te_flux;
};
#define NULL_FBOUND__ { "", UINT_MAX, NULL, NULL}
static const struct f_boundary NULL_FBOUND = NULL_FBOUND__;

static void
print_f_boundary
  (FILE* stream,
   const struct f_boundary* b,
   const enum description_type type)
{
  ASSERT(stream && b && type == DESC_BOUND_F_FOR_SOLID); (void)type;
  fprintf(stream,
    "F boundary %s for SOLID: flux='%s'\n",
    b->name,
    (b->flux ? b->flux : "0"));
}

static char
eq_f_boundary(const struct f_boundary* a, const struct f_boundary* b)
{
  return (a->mat_id == b->mat_id
    && 0 == strcmp(a->name, b->name)
    && 0 == strcmp(a->flux, b->flux));
}

static FINLINE res_T
cp_f_boundary(struct f_boundary* dst, const struct f_boundary* src)
{
  strcpy(dst->name, src->name);
  dst->mat_id = src->mat_id;
  if (!src->flux) dst->flux = NULL;
  else {
    dst->flux = malloc(sizeof(src->flux) + 1);
    if (!dst->flux) return RES_MEM_ERR;
    strcpy(dst->flux, src->flux);
  }
  if (!src->te_flux) dst->te_flux = NULL;
  else {
    dst->te_flux = te_duplicate(src->te_flux);
    if (!dst->te_flux) return RES_MEM_ERR;
  }
  return RES_OK;
}

static FINLINE res_T
cp_release_f_boundary(struct f_boundary* dst, struct f_boundary* src)
{
  strcpy(dst->name, src->name);
  dst->mat_id = src->mat_id;
  dst->flux = malloc(sizeof(src->flux) + 1);
  dst->flux = src->flux; src->flux = NULL;
  dst->te_flux = src->te_flux; src->te_flux = NULL;
  return RES_OK;
}

static FINLINE res_T
release_f_boundary(struct f_boundary* a)
{
  free(a->flux);
  te_free(a->te_flux);
  return RES_OK;
}

static size_t
hash_f_boundary(const struct f_boundary* key)
{
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->name)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->name)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->mat_id)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->mat_id)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, strlen(key->flux) * sizeof(*key->flux)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)key->flux)[i]);
    hash = hash * FNV64_PRIME;
  }
  return hash;
}

struct solid_fluid_connect {
  char name[32];
  double emissivity;
  double specular_fraction;
  double hc;
  double hc_max;
  int has_emissivity, has_hc;
};
#define NULL_SFCONNECT__ { "", 0, 0, 0, 0, 0, 0}
static const struct solid_fluid_connect NULL_SFCONNECT = NULL_SFCONNECT__;

static char
eq_sf_connect(const struct solid_fluid_connect* a, const struct solid_fluid_connect* b)
{
  return (char)(!strcmp(a->name, b->name)
    && a->emissivity == b->emissivity
    && a->specular_fraction == b->specular_fraction
    && a->hc == b->hc
    && a->hc_max == b->hc_max
    && a->has_emissivity == b->has_emissivity
    && a->has_hc == b->has_hc);
}

static FINLINE res_T
cp_sf_connect(struct solid_fluid_connect* dst, const struct solid_fluid_connect* src)
{
  strcpy(dst->name, src->name);
  dst->specular_fraction = src->specular_fraction;
  dst->has_emissivity = src->has_emissivity;
  dst->has_hc = src->has_hc;
  dst->emissivity = src->emissivity;
  dst->hc_max = src->hc_max;
  dst->hc = src->hc;
  return RES_OK;
}

static FINLINE res_T
cp_release_sf_connect(struct solid_fluid_connect* dst, struct solid_fluid_connect* src)
{
  return cp_sf_connect(dst, src);
}

static FINLINE res_T
release_sf_connect(struct solid_fluid_connect* a)
{
  (void)a;
  return RES_OK;
}

static size_t
hash_sf_connect(const struct solid_fluid_connect* key)
{
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->name)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)&key->name)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->emissivity)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->emissivity)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->specular_fraction)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->specular_fraction)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->hc)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->hc)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->hc_max)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->hc_max)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_emissivity)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->has_emissivity)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_hc)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->has_hc)[i]);
    hash = hash * FNV64_PRIME;
  }
  return hash;
}

struct description {
  enum description_type type;
  union d {
    struct mat_fluid fluid;
    struct mat_solid solid;
    struct t_boundary t_boundary;
    struct f_boundary f_boundary;
    struct h_boundary h_boundary;
    struct solid_fluid_connect sf_connect;
  } d;
};

static INLINE void
print_description
  (FILE* stream,
   const struct description* desc)
{
  ASSERT(stream && desc);
  switch (desc->type) {
  case DESC_MAT_SOLID:
    print_solid(stream, &desc->d.solid);
    break;
  case DESC_MAT_FLUID:
    print_fluid(stream, &desc->d.fluid);
    break;
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_T_FOR_FLUID:
    print_t_boundary(stream, &desc->d.t_boundary, desc->type);
    break;
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    print_h_boundary(stream, &desc->d.h_boundary, desc->type);
    break;
  case DESC_BOUND_F_FOR_SOLID:
    print_f_boundary(stream, &desc->d.f_boundary, desc->type);
    break;
  default: {
    char msg[1024];
    snprintf(msg, sizeof(msg), "%s (%s: %d): Invalid type.\n",
      __FUNCTION__, __FILE__, __LINE__);
    FATAL(msg);
  }
  }
}

static INLINE char
eq_description(const struct description* a, const struct description* b)
{
  if (a->type != b->type) return 0;
  switch (a->type) {
  case DESC_MAT_SOLID:
    return eq_solid(&a->d.solid, &b->d.solid);
  case DESC_MAT_FLUID:
    return eq_fluid(&a->d.fluid, &b->d.fluid);
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    return eq_h_boundary(&a->d.h_boundary, &b->d.h_boundary);
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_T_FOR_FLUID:
    return eq_t_boundary(&a->d.t_boundary, &b->d.t_boundary, a->type);
  case DESC_BOUND_F_FOR_SOLID:
    return eq_f_boundary(&a->d.f_boundary, &b->d.f_boundary);
  case DESC_SOLID_FLUID_CONNECT:
    return eq_sf_connect(&a->d.sf_connect, &b->d.sf_connect);
  default: {
    char msg[1024];
    snprintf(msg, sizeof(msg), "%s (%s: %d): Invalid type.\n",
      __FUNCTION__, __FILE__, __LINE__);
    FATAL(msg);
  }
  }
}

static FINLINE res_T
init_description(struct mem_allocator* alloc, struct description* key)
{
  (void)alloc;
  key->type = DESCRIPTION_TYPE_COUNT__;
  return RES_OK;
}

static FINLINE res_T
cp_description(struct description* dst, const struct description* src)
{
  ASSERT(src && dst);
  dst->type = src->type;
  switch (dst->type) {
  case DESC_MAT_SOLID:
    return cp_solid(&dst->d.solid, &src->d.solid);
  case DESC_MAT_FLUID:
    return cp_fluid(&dst->d.fluid, &src->d.fluid);
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    return cp_h_boundary(&dst->d.h_boundary, &src->d.h_boundary);
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_T_FOR_FLUID:
    return cp_t_boundary(&dst->d.t_boundary, &src->d.t_boundary);
  case DESC_BOUND_F_FOR_SOLID:
    return cp_f_boundary(&dst->d.f_boundary, &src->d.f_boundary);
  case DESC_SOLID_FLUID_CONNECT:
    return cp_sf_connect(&dst->d.sf_connect, &src->d.sf_connect);
  default: {
    char msg[1024];
    snprintf(msg, sizeof(msg), "%s (%s: %d): Invalid type.\n",
      __FUNCTION__, __FILE__, __LINE__);
    FATAL(msg);
  }
  }
}

static FINLINE res_T
cp_release_description(struct description* dst, struct description* src)
{
  ASSERT(src && dst);
  dst->type = src->type;
  switch (dst->type) {
  case DESC_MAT_SOLID:
    return cp_release_solid(&dst->d.solid, &src->d.solid);
  case DESC_MAT_FLUID:
    return cp_release_fluid(&dst->d.fluid, &src->d.fluid);
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    return cp_release_h_boundary(&dst->d.h_boundary, &src->d.h_boundary);
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_T_FOR_FLUID:
    return cp_release_t_boundary(&dst->d.t_boundary, &src->d.t_boundary);
  case DESC_BOUND_F_FOR_SOLID:
    return cp_release_f_boundary(&dst->d.f_boundary, &src->d.f_boundary);
  case DESC_SOLID_FLUID_CONNECT:
    return cp_release_sf_connect(&dst->d.sf_connect, &src->d.sf_connect);
  case DESCRIPTION_TYPE_COUNT__:
    return RES_OK;
  default: {
    char msg[1024];
    snprintf(msg, sizeof(msg), "%s (%s: %d): Invalid type.\n",
      __FUNCTION__, __FILE__, __LINE__);
    FATAL(msg);
  }
  }
}

static FINLINE res_T
release_description(struct description* a)
{
  ASSERT(a);
  switch (a->type) {
  case DESC_MAT_SOLID:
    return release_solid(&a->d.solid);
  case DESC_MAT_FLUID:
    return release_fluid(&a->d.fluid);
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    return release_h_boundary(&a->d.h_boundary);
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_T_FOR_FLUID:
    return release_t_boundary(&a->d.t_boundary);
  case DESC_BOUND_F_FOR_SOLID:
    return release_f_boundary(&a->d.f_boundary);
  case DESC_SOLID_FLUID_CONNECT:
    return release_sf_connect(&a->d.sf_connect);
  case DESCRIPTION_TYPE_COUNT__:
    return RES_OK;
  default: {
    char msg[1024];
    snprintf(msg, sizeof(msg), "%s (%s: %d): Invalid type.\n",
      __FUNCTION__, __FILE__, __LINE__);
    FATAL(msg);
  }
  }
}

static INLINE size_t
hash_description(const struct description* key)
{
  switch (key->type) {
  case DESC_MAT_SOLID:
    return hash_solid(&key->d.solid);
  case DESC_MAT_FLUID:
    return hash_fluid(&key->d.fluid);
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    return hash_h_boundary(&key->d.h_boundary);
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_T_FOR_FLUID:
    return hash_t_boundary(&key->d.t_boundary, key->type);
  case DESC_BOUND_F_FOR_SOLID:
    return hash_f_boundary(&key->d.f_boundary);
  case DESC_SOLID_FLUID_CONNECT:
    return hash_sf_connect(&key->d.sf_connect);
  default: {
    char msg[1024];
    snprintf(msg, sizeof(msg), "%s (%s: %d): Invalid type.\n",
      __FUNCTION__, __FILE__, __LINE__);
    FATAL(msg);
  }
  }
}

struct camera {
  double pos[3];
  double tgt[3];
  double up[3];
  double fov;
  unsigned spp;
  unsigned img[2];
  union { /* Trick to allow static initialization with INF */
    uint64_t t;
    double time;
  } u;
};
#define NULL_CAMERA__ {\
  {1,1,1},{0,0,0},{0,0,1},30,4,{640,480},\
  { 0x7FF0000000000000 } /* time=INF */\
} 
static const struct camera NULL_CAMERA = NULL_CAMERA__;

struct solve_boundary {
  size_t* primitives;
  enum sdis_side* sides;
};

struct stardis {
  struct geometry geometry;
  struct description* descriptions;  /*array of materials and boundaries */

  double probe[4]; /* x,y,z,t of probe when mode is PROBE_COMPUTE */
  struct camera camera; /* camera when mode is IR_COMPUTE */
  char solve_name[64]; /* medium name when mode is MEDIUM_COMPUTE,
                       boundary file name when BOUNDARY_COMPUTE*/
  struct solve_boundary boundary; /* Boundary to solve when mode
                                  is BOUNDARY_COMPUTE */

  size_t N; /*number of MC realizations*/
  unsigned nthreads;
  double scale_factor;
  double radiative_temp[2];
  struct mem_allocator allocator;
  int allocator_initialized;
  enum sdis_heat_path_flag dump_paths;
};
#define NULL_ALLOCATOR__ {NULL}
#define NULL_STARDIS__ {\
  NULL_GEOMETRY__, NULL,\
  {0,0,0,0}, NULL_CAMERA__, "", {NULL,NULL},\
  0, 0, 0, {300,300}, NULL_ALLOCATOR__, 0, SDIS_HEAT_PATH_NONE}
static const struct stardis NULL_STARDIS = NULL_STARDIS__;

enum content_type {
  CONTENT_GEOMETRY,
  CONTENT_CONNECTION,
  CONTENT_BOUNDARY,
  CONTENT_TYPES_COUNT
};

extern res_T 
stardis_init
  (const struct args* args,
   struct stardis* stardis);

extern res_T
stardis_compute
  (struct stardis* stardis,
   const int mode);

extern void
stardis_release
  (struct stardis* stardis);

#endif /*STARDIS-APP_H*/
