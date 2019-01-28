/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef STARDIS_APP_H
#define STARDIS_APP_H

#include <star/sstl.h>
#include <rsys/rsys.h>
#include <rsys/float3.h>
#include <rsys/stretchy_array.h>
#include <rsys/hash_table.h>
#include <sdis.h>
#include <tinyexpr.h>
#include "args.h"

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

static INLINE void
print_trg_as_obj
  (FILE* stream,
   const struct vertex* vertices,
   const struct triangle* trg)
{
  assert(stream && vertices && trg);
  fprintf(stream, "v %.8e %.8e %.8e\nv %.8e %.8e %.8e\nv %.8e %.8e %.8e\nf 1 2 3\n",
    SPLIT3(vertices[trg->indices.data[0]].xyz),
    SPLIT3(vertices[trg->indices.data[1]].xyz),
    SPLIT3(vertices[trg->indices.data[2]].xyz));
}

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
  unsigned fluid_id;
  double rho;
  double cp;
  char* Tinit;
};

static void
print_fluid(FILE* stream, const struct mat_fluid* f)
{
  assert(stream && f);
  fprintf(stream,
    "Fluid %u: cp=%g rho=%g Tinit='%s'\n",
    f->fluid_id, f->cp, f->rho, f->Tinit);
}

static char
eq_fluid(const struct mat_fluid* a, const struct mat_fluid* b)
{
  if (a->fluid_id != b->fluid_id
    || a->rho != b->rho
    || a->cp != b->cp
    || strcmp(a->Tinit, b->Tinit))
    return 0;
  return 1;
}

static size_t
hash_fluid(const struct mat_fluid* key)
{
#ifdef ARCH_32BITS
  /* 32-bits Fowler/Noll/Vo hash function */
  const uint32_t FNV32_PRIME =
    (uint32_t) (((uint32_t) 1 << 24) + ((uint32_t) 1 << 8) + 0x93);
  const uint32_t OFFSET32_BASIS = 2166136261u;
  uint32_t hash = OFFSET32_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->fluid_id)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->fluid_id)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->rho)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->rho)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->cp)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->cp)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, strlen(key->Tinit) * sizeof(*key->Tinit)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)key->Tinit)[i]);
    hash = hash * FNV32_PRIME;
  }
  return hash;
#elif defined(ARCH_64BITS)
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
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
  FOR_EACH(i, 0, strlen(key->Tinit)*sizeof(*key->Tinit)) {
    hash = hash ^ (uint64_t)((unsigned char) ((const char*)key->Tinit)[i]);
    hash = hash * FNV64_PRIME;
  }
  return hash;
#else
#error "Unexpected architecture"
#endif
}

struct mat_solid {
  unsigned solid_id;
  double lambda;
  double rho;
  double cp;
  double delta;
  char* Tinit;
  char* power;
  int has_power;
};

static void
print_solid(FILE* stream, const struct mat_solid* s)
{
  assert(stream && s);
  fprintf(stream,
    "Solid %u: lambda=%g cp=%g rho=%g delta=%g Tinit='%s' Power='%s'\n",
    s->solid_id, s->lambda, s->cp, s->rho, s->delta,
    s->Tinit, (s->has_power ? s->power : "0"));
}

static char
eq_solid(const struct mat_solid* a, const struct mat_solid* b)
{
  if (a->solid_id != b->solid_id
    || a->lambda != b->lambda
    || a->rho != b->rho
    || a->cp != b->cp
    || a->delta != b->delta
    || strcmp(a->Tinit, b->Tinit)
    || a->has_power != b->has_power)
    return 0;
  if (a->has_power && a->power != b->power) return 0;
  return 1;
}

static size_t
hash_solid(const struct mat_solid* key)
{
#ifdef ARCH_32BITS
  /* 32-bits Fowler/Noll/Vo hash function */
  const uint32_t FNV32_PRIME =
    (uint32_t) (((uint32_t) 1 << 24) + ((uint32_t) 1 << 8) + 0x93);
  const uint32_t OFFSET32_BASIS = 2166136261u;
  uint32_t hash = OFFSET32_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->solid_id)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->solid_id)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->lambda)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->lambda)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->rho)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->rho)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->cp)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->cp)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->delta)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->delta)[i]);
    hash = hash * FNV32_PRIME;
}
  FOR_EACH(i, 0, strlen(key->Tinit) * sizeof(*key->Tinit)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)key->Tinit)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_power)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->has_power)[i]);
    hash = hash * FNV32_PRIME;
  }
  if (!key->has_power) return hash;
  FOR_EACH(i, 0, strlen(key->Tinit) * sizeof(*key->power)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) key->power)[i]);
    hash = hash * FNV32_PRIME;
  }
  return hash;
#elif defined(ARCH_64BITS)
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
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
  FOR_EACH(i, 0, strlen(key->Tinit) * sizeof(*key->Tinit)) {
    hash = hash ^ (uint64_t)((unsigned char)((const char*)key->Tinit)[i]);
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
#else
#error "Unexpected architecture"
#endif
}

struct h_boundary {
  unsigned mat_id;
  double emissivity;
  double specular_fraction;
  double hc;
  double hc_max;
  char* T;
  int has_emissivity, has_hc;
};

static void
print_h_boundary
  (FILE* stream,
   const struct h_boundary* b,
   const enum description_type type)
{
  assert(stream && b
    && (type == DESC_BOUND_H_FOR_SOLID || type == DESC_BOUND_H_FOR_FLUID));
  fprintf(stream,
    "H boundary for %s: emissivity=%g specular_fraction=%g hc=%g hc_max=%g T='%s'\n",
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
    || a->specular_fraction != b->specular_fraction
    || strcmp(a->T, b->T)
    || a->has_emissivity != b->has_emissivity
    || a->has_hc != b->has_hc)
    return 0;
  if (a->has_emissivity && a->emissivity != b->emissivity) return 0;
  if (a->has_hc && (a->hc != b->hc || a->hc_max != b->hc_max)) return 0;
  return 1;
}

static size_t
hash_h_boundary(const struct h_boundary* key)
{
#ifdef ARCH_32BITS
  /* 32-bits Fowler/Noll/Vo hash function */
  const uint32_t FNV32_PRIME =
    (uint32_t) (((uint32_t) 1 << 24) + ((uint32_t) 1 << 8) + 0x93);
  const uint32_t OFFSET32_BASIS = 2166136261u;
  uint32_t hash = OFFSET32_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->mat_id)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->mat_id)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->emissivity)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->emissivity)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->specular_fraction)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->specular_fraction)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, strlen(key->T) * sizeof(*key->T)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)key->T)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_emissivity)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->has_emissivity)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_hc)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) &key->has_hc)[i]);
    hash = hash * FNV32_PRIME;
  }
  if (!key->has_hc) return hash;
  FOR_EACH(i, 0, sizeof(key->hc)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) &key->hc)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->hc_max)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) &key->hc_max)[i]);
    hash = hash * FNV32_PRIME;
  }
  return hash;
#elif defined(ARCH_64BITS)
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
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
#else
#error "Unexpected architecture"
#endif
}

struct t_boundary {
  unsigned mat_id;
  char* T;
  struct te_expr* te_temperature;
  double hc;
  double hc_max;
  int has_hc;
};

static void
print_t_boundary
  (FILE* stream,
   const struct t_boundary* b,
   const enum description_type type)
{
  assert(stream && b
    && (type == DESC_BOUND_T_FOR_SOLID || type == DESC_BOUND_T_FOR_FLUID));
  fprintf(stream,
    "T boundary for %s: hc=%g hc_max=%g T='%s'\n",
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

static size_t
hash_t_boundary
  (const struct t_boundary* key,
   const enum description_type type)
{
  (void)type;
  ASSERT(type == DESC_BOUND_T_FOR_SOLID || type == DESC_BOUND_T_FOR_FLUID);
#ifdef ARCH_32BITS
  /* 32-bits Fowler/Noll/Vo hash function */
  const uint32_t FNV32_PRIME =
    (uint32_t) (((uint32_t) 1 << 24) + ((uint32_t) 1 << 8) + 0x93);
  const uint32_t OFFSET32_BASIS = 2166136261u;
  uint32_t hash = OFFSET32_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->mat_id)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->mat_id)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_hc)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) &key->has_hc)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, strlen(key->T) * sizeof(*key->T)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) key->T)[i]);
    hash = hash * FNV32_PRIME;
  }
  if (!key->has_hc) return hash;
  FOR_EACH(i, 0, sizeof(key->hc)) {
    hash = hash ^ (uint32_t) ((unsigned char) ((const char*) &key->hc)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->hc_max)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->hc_max)[i]);
    hash = hash * FNV32_PRIME;
  }
  return hash;
#elif defined(ARCH_64BITS)
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
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
#else
#error "Unexpected architecture"
#endif
}

struct f_boundary {
  unsigned mat_id;
  char* flux;
  struct te_expr* te_flux;
  double hc;
  double hc_max;
  int has_hc;
};

static void
print_f_boundary
  (FILE* stream,
   const struct f_boundary* b,
   const enum description_type type)
{
  assert(stream && b && type == DESC_BOUND_F_FOR_SOLID);
  fprintf(stream,
    "F boundary for SOLID: hc=%g hc_max=%g flux='%s'\n",
    (b->has_hc ? b->hc : 0),
    (b->has_hc ? b->hc_max : 0),
    (b->flux ? b->flux : "0"));
}

static char
eq_f_boundary(const struct f_boundary* a, const struct f_boundary* b)
{
  if (a->mat_id != b->mat_id
    || strcmp(a->flux, b->flux)
    || a->has_hc != b->has_hc)
    return 0;
  if (a->has_hc && (a->hc != b->hc || a->hc_max != b->hc_max)) return 0;
  return 1;
}

static size_t
hash_f_boundary(const struct f_boundary* key)
{
#ifdef ARCH_32BITS
  /* 32-bits Fowler/Noll/Vo hash function */
  const uint32_t FNV32_PRIME =
    (uint32_t) (((uint32_t) 1 << 24) + ((uint32_t) 1 << 8) + 0x93);
  const uint32_t OFFSET32_BASIS = 2166136261u;
  uint32_t hash = OFFSET32_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->mat_id)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->mat_id)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->hc)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->hc)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->hc_max)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->hc_max)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, strlen(key->flux) * sizeof(*key->flux)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)key->flux)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_hc)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->has_hc)[i]);
    hash = hash * FNV32_PRIME;
  }
  return hash;
#elif defined(ARCH_64BITS)
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->mat_id)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->mat_id)[i]);
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
  FOR_EACH(i, 0, strlen(key->flux) * sizeof(*key->flux)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)key->flux)[i]);
    hash = hash * FNV64_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_hc)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->has_hc)[i]);
    hash = hash * FNV64_PRIME;
  }
  return hash;
#else
#error "Unexpected architecture"
#endif
}

struct solid_fluid_connect {
  double emissivity;
  double specular_fraction;
  double hc;
  double hc_max;
  int has_emissivity, has_hc;
};

static char
eq_sf_connect(const struct solid_fluid_connect* a, const struct solid_fluid_connect* b)
{
  return (char) (a->emissivity == b->emissivity
    && a->specular_fraction == b->specular_fraction
    && a->hc == b->hc
    && a->hc_max == b->hc_max
    && a->has_emissivity == b->has_emissivity
    && a->has_hc == b->has_hc);
}

static size_t
hash_sf_connect(const struct solid_fluid_connect* key)
{
#ifdef ARCH_32BITS
  /* 32-bits Fowler/Noll/Vo hash function */
  const uint32_t FNV32_PRIME =
    (uint32_t) (((uint32_t) 1 << 24) + ((uint32_t) 1 << 8) + 0x93);
  const uint32_t OFFSET32_BASIS = 2166136261u;
  uint32_t hash = OFFSET32_BASIS;
  size_t i;
  ASSERT(key);
  FOR_EACH(i, 0, sizeof(key->emissivity)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->emissivity)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->specular_fraction)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->specular_fraction)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->hc)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->hc)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->hc_max)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->hc_max)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_emissivity)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->has_emissivity)[i]);
    hash = hash * FNV32_PRIME;
  }
  FOR_EACH(i, 0, sizeof(key->has_hc)) {
    hash = hash ^ (uint32_t)((unsigned char)((const char*)&key->has_hc)[i]);
    hash = hash * FNV32_PRIME;
  }
  return hash;
#elif defined(ARCH_64BITS)
  /* 64-bits Fowler/Noll/Vo hash function */
  const uint64_t FNV64_PRIME =
    (uint64_t) (((uint64_t) 1 << 40) + ((uint64_t) 1 << 8) + 0xB3);
  const uint64_t OFFSET64_BASIS = (uint64_t) 14695981039346656037u;
  uint64_t hash = OFFSET64_BASIS;
  size_t i;
  ASSERT(key);
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
#else
#error "Unexpected architecture"
#endif
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
   struct description* desc)
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
  default: FATAL("Invalid type.\n");
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
  default: FATAL("Invalid type.\n");
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
  default: FATAL("Invalid type.\n");
  }
}

struct camera {
  double pos[3];
  double tgt[3];
  double up[3];
  double fov;
  unsigned spp;
  unsigned img[2];
};
#define NULL_CAMERA__ {{1,1,1},{0,0,0},{0,0,1},30,4,{640,480}} 
static const struct camera NULL_CAMERA = NULL_CAMERA__;

struct stardis {
  struct geometry geometry;
  struct description* descriptions;  /*array of materials and boundaries */

  size_t N; /*number of MC realizations*/
  unsigned nthreads;
  double probe[4]; /*x,y,z,t of probe*/
  double scale_factor;
  double radiative_temp[2];
  struct camera camera;
  struct mem_allocator allocator;
  int allocator_initialized;
};
#define NULL_ALLOCATOR__ {NULL}
#define NULL_STARDIS__ {NULL_GEOMETRY__, NULL, 0, 0, {0,0,0,0}, 0, {300,300}, NULL_CAMERA__, NULL_ALLOCATOR__, 0}
static const struct stardis NULL_STARDIS = NULL_STARDIS__;

extern res_T 
stardis_init
  (const struct args* args,
   struct stardis* stardis);

extern res_T
stardis_compute(struct stardis* stardis, enum stardis_mode mode);

void
stardis_release(struct stardis* stardis);

extern res_T
dump_vtk(FILE* output, const struct geometry* geometry);

#endif /*STARDIS-APP_H*/
