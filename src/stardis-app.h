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

struct vertex{
  float xyz[3];
};
#define NULL_VERTEX__ {{0.0 ,0.0 ,0.0}}
static const struct vertex NULL_VERTEX = NULL_VERTEX__;

static inline char
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

struct triangle{
  unsigned indices[3];
  unsigned sids[3];
  int medium_front;
  int medium_back;
  int bound_id;
};
#define NULL_TRIANGLE__ {{0, 0, 0}, {0, 0, 0}, -1, -1, -1}
static const struct triangle NULL_TRIANGLE = NULL_TRIANGLE__;

static char
eq_triangle(const struct triangle* a, const struct triangle* b)
{  
  return a->sids[0] == b->sids[0]
      && a->sids[1] == b->sids[1]
      && a->sids[2] == b->sids[2];
}

static size_t
hash_triangle(const struct triangle* tri)
{
  return hash_fnv64(tri->sids, sizeof(unsigned[3]));
}

/* Declare the hash table that map a triangle to its index */
#define HTABLE_NAME triangle 
#define HTABLE_DATA unsigned
#define HTABLE_KEY struct triangle
#define HTABLE_KEY_FUNCTOR_EQ eq_triangle
#define HTABLE_KEY_FUNCTOR_HASH hash_triangle
#include <rsys/hash_table.h>

struct geometry{
  struct vertex* vertex;
  struct triangle* triangle;
  unsigned medium_count;
  unsigned boundary_count;
  struct sdis_interface** interfaces;
};
#define NULL_GEOMETRY__ {NULL, NULL, 0, 0, NULL}
static const struct geometry NULL_GEOMETRY = NULL_GEOMETRY__;

static res_T
release_geometry
(struct geometry* geom)
{
  res_T res = RES_OK;

  sa_release(geom->vertex);
  sa_release(geom->triangle);
  sa_release(geom->interfaces);

exit:
  return res;
error:
  goto exit;
}

struct material{
  double lambda;
  double rho;
  double cp;
  double delta;
  double emissivity;
  double specular_fraction;
  char* Tinit;
  char* power;
};
#define NULL_MATERIAL__ {0.0 ,0.0 ,0.0, 0.0, 0.0, 0.0, NULL, NULL}
static const struct material NULL_MATERIAL = NULL_MATERIAL__;

struct boundary{
  double hc;
  char* T;
  char* flux;
};
#define NULL_BOUNDARY__  {-1, NULL, NULL}
static const struct boundary NULL_BOUNDARY = NULL_BOUNDARY__;

struct camera{
  double pos[3];
  double tgt[3];
  double up[3];
  double fov;
  unsigned spp;
  unsigned img[2];
};
#define NULL_CAMERA__ {{1,1,1},{0,0,0},{0,0,1},70,4,{640,480}} 
static const struct camera NULL_CAMERA = NULL_CAMERA__;

struct stardis{
  struct geometry geometry;
  struct material* material; /*array of materials*/
  struct boundary* boundary; /*array of boundaries*/

  size_t N; /*number of MC realizations*/
  unsigned nthreads;
  double probe[4]; /*x,y,z,t of probe*/
  double scale_factor;
  double radiative_temp[2];
  struct camera camera;
};
#define NULL_STARDIS__ {NULL_GEOMETRY__, NULL, NULL, 0, 0, {0,0,0,0}, 0, {300,300}, NULL_CAMERA__}
static const struct stardis NULL_STARDIS = NULL_STARDIS__;

extern res_T 
stardis_init
  (const struct args* args,
   struct stardis* stardis);

extern res_T
stardis_compute(struct stardis* stardis, enum stardis_mode mode);

extern res_T
stardis_release(struct stardis* stardis);

extern res_T
dump_vtk(FILE* output, const struct geometry* geometry, int nbound);

#endif /*STARDIS-APP_H*/
