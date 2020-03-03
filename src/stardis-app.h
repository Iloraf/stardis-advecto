/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#ifndef STARDIS_APP_H
#define STARDIS_APP_H

#include "stardis-parsing.h"

#include <star/sstl.h>
#include <star/sg3d.h>

#include <rsys/rsys.h>
#include <rsys/float3.h>
#include <rsys/double3.h>
#include <rsys/dynamic_array_size_t.h>
#include <rsys/dynamic_array.h>
#include <rsys/str.h>

#include <sdis.h>

#include <limits.h>

/* Utility macros */
#define ERR(Expr) if((res = (Expr)) != RES_OK) goto error; else (void)0

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

#define DARRAY_NAME interface_ptrs
#define DARRAY_DATA struct sdis_interface*
#include <rsys/dynamic_array.h>

struct geometry {
  struct sg3d_geometry* sg3d;
  struct sdis_scene* sdis_scn;
  struct darray_interface_ptrs interf_bytrg;
  struct darray_interface_ptrs interfaces;
};

static INLINE res_T
init_geometry
  (struct mem_allocator* allocator,
   const int verbose,
   struct geometry* geom)
{
  res_T res = RES_OK;
  struct sg3d_device* sg3d_dev = NULL;
  struct sg3d_geometry* sg3d = NULL;
  ASSERT(allocator && geom);

  ERR(sg3d_device_create(NULL, allocator, verbose, &sg3d_dev));
  ERR(sg3d_geometry_create(sg3d_dev, &sg3d));
  darray_interface_ptrs_init(allocator, &geom->interfaces);
  darray_interface_ptrs_init(allocator, &geom->interf_bytrg);

exit:
  geom->sg3d = sg3d;
  geom->sdis_scn = NULL;
  if(sg3d_dev) SG3D(device_ref_put(sg3d_dev));
  return res;
error:
  if(sg3d) SG3D(geometry_ref_put(sg3d));
  sg3d = NULL;
  goto exit;
}

static INLINE void
release_geometry(struct geometry* geom)
{
  size_t i;
  struct sdis_interface
    **intf = darray_interface_ptrs_data_get(&geom->interfaces);
  if(geom->sg3d) SG3D(geometry_ref_put(geom->sg3d));
  if(geom->sdis_scn) SDIS(scene_ref_put(geom->sdis_scn));
  for(i = 0; i < darray_interface_ptrs_size_get(&geom->interfaces); ++i)
    SDIS(interface_ref_put(intf[i]));
  darray_interface_ptrs_release(&geom->interfaces);
  darray_interface_ptrs_release(&geom->interf_bytrg);
}

struct mat_fluid {
  struct str name;
  unsigned fluid_id;
  double rho;
  double cp;
  double tinit;
  double imposed_temperature;
};

static FINLINE res_T
init_fluid(struct mem_allocator* allocator, struct mat_fluid* dst)
{
  str_init(allocator, &dst->name);
  dst->fluid_id = UINT_MAX;
  dst->rho = 0;
  dst->cp = 0;
  dst->tinit = 0;
  dst->imposed_temperature = -1;
  return RES_OK;
}

static FINLINE void
release_fluid(struct mat_fluid* fluid)
{
  str_release(&fluid->name);
}

static void
print_fluid(FILE* stream, const struct mat_fluid* f)
{
  ASSERT(stream && f);
  fprintf(stream, "Fluid '%s': rho=%g cp=%g",
    str_cget(&f->name), f->rho, f->cp);
  if(f->tinit >= 0) fprintf(stream, " Tinit=%g", f->tinit);
  if(f->imposed_temperature >= 0)
    fprintf(stream, " Temp=%g", f->imposed_temperature);
  fprintf(stream, "\n");
}

static FINLINE res_T
cp_fluid(struct mat_fluid* dst, const struct mat_fluid* src)
{
  str_copy(&dst->name, &src->name);
  dst->fluid_id = src->fluid_id;
  dst->rho = src->rho;
  dst->cp = src->cp;
  dst->tinit = src->tinit;
  dst->imposed_temperature = src->imposed_temperature;
  return RES_OK;
}

struct mat_solid {
  struct str name;
  unsigned solid_id;
  double lambda;
  double rho;
  double cp;
  double delta;
  double tinit;
  double imposed_temperature;
  double vpower;
};

static FINLINE res_T
init_solid(struct mem_allocator* allocator, struct mat_solid* dst)
{
  str_init(allocator, &dst->name);
  dst->solid_id = UINT_MAX;
  dst->lambda = 0;
  dst->rho = 0;
  dst->cp = 0;
  dst->delta = 0;
  dst->tinit = -1;
  dst->imposed_temperature = -1;
  dst->vpower = 0;
  return RES_OK;
}

static FINLINE void
release_solid(struct mat_solid* solid)
{
  str_release(&solid->name);
}

static void
print_solid(FILE* stream, const struct mat_solid* s)
{
  ASSERT(stream && s);
  fprintf(stream,
    "Solid '%s': lambda=%g rho=%g cp=%g delta=%g",
    str_cget(&s->name), s->lambda, s->rho, s->cp, s->delta);
  if(s->vpower != 0) fprintf(stream, " VPower=%g", s->vpower);
  if(s->tinit >= 0) fprintf(stream, " Tinit=%g", s->tinit);
  if(s->imposed_temperature >= 0)
    fprintf(stream, " Temp=%g", s->imposed_temperature);
  fprintf(stream, "\n");
}

static FINLINE res_T
cp_solid(struct mat_solid* dst, const struct mat_solid* src)
{
  str_copy(&dst->name, &src->name);
  dst->solid_id = src->solid_id;
  dst->lambda = src->lambda;
  dst->rho = src->rho;
  dst->cp = src->cp;
  dst->delta = src->delta;
  dst->tinit = src->tinit;
  dst->imposed_temperature = src->imposed_temperature;
  dst->vpower = src->vpower;
  return RES_OK;
}

struct h_boundary {
  struct str name;
  unsigned mat_id;
  double emissivity;
  double specular_fraction;
  double hc;
  double imposed_temperature;
};

static FINLINE res_T
init_h(struct mem_allocator* allocator, struct h_boundary* dst)
{
  str_init(allocator, &dst->name);
  dst->mat_id = UINT_MAX;
  dst->emissivity = 0;
  dst->specular_fraction = 0;
  dst->hc = 0;
  dst->imposed_temperature = -1;
  return RES_OK;
}

static FINLINE void
release_h_boundary(struct h_boundary* bound)
{
  str_release(&bound->name);
}

static void
print_h_boundary
  (FILE* stream,
   const struct h_boundary* b,
   const enum description_type type)
{
  ASSERT(stream && b
    && (type == DESC_BOUND_H_FOR_SOLID || type == DESC_BOUND_H_FOR_FLUID));
  fprintf(stream,
    "H boundary for %s '%s': emissivity=%g specular_fraction=%g hc=%g T=%g\n",
    (type == DESC_BOUND_H_FOR_SOLID ? "solid" : "fluid"),
    str_cget(&b->name),
    b->emissivity, b->specular_fraction, b->hc,
    b->imposed_temperature);
}

static FINLINE res_T
cp_h_boundary(struct h_boundary* dst, const struct h_boundary* src)
{
  str_copy(&dst->name, &src->name);
  dst->mat_id = src->mat_id;
  dst->specular_fraction = src->specular_fraction;
  dst->imposed_temperature = src->imposed_temperature;
  dst->emissivity = src->emissivity;
  dst->hc = src->hc;
  return RES_OK;
}

struct t_boundary {
  struct str name;
  unsigned mat_id;
  double hc;
  double imposed_temperature;
};

static FINLINE res_T
init_t(struct mem_allocator* allocator, struct t_boundary* dst)
{
  str_init(allocator, &dst->name);
  dst->mat_id = UINT_MAX;
  dst->hc = 0;
  dst->imposed_temperature = -1;
  return RES_OK;
}

static FINLINE void
release_t_boundary(struct t_boundary* bound)
{
  str_release(&bound->name);
}

static void
print_t_boundary
  (FILE* stream,
   const struct t_boundary* b,
   const enum description_type type)
{
  ASSERT(stream && b
    && (type == DESC_BOUND_T_FOR_SOLID || type == DESC_BOUND_T_FOR_FLUID));
  fprintf(stream,
    "T boundary for %s' %s': hc=%g T=%f\n",
    (type == DESC_BOUND_T_FOR_SOLID ? "solid" : "fluid"),
    str_cget(&b->name),
    b->hc, b->imposed_temperature);
}

static FINLINE res_T
cp_t_boundary(struct t_boundary* dst, const struct t_boundary* src)
{
  str_copy(&dst->name, &src->name);
  dst->mat_id = src->mat_id;
  dst->imposed_temperature = src->imposed_temperature;
  dst->hc = src->hc;
  return RES_OK;
}

struct f_boundary {
  struct str name;
  unsigned mat_id;
  double imposed_flux;
};

static FINLINE res_T
init_f(struct mem_allocator* allocator, struct f_boundary* dst)
{
  str_init(allocator, &dst->name);
  dst->mat_id = UINT_MAX;
  dst->imposed_flux = -1;
  return RES_OK;
}

static FINLINE void
release_f_boundary(struct f_boundary* bound)
{
  str_release(&bound->name);
}

static void
print_f_boundary
  (FILE* stream,
   const struct f_boundary* b)
{
  ASSERT(stream && b);
  fprintf(stream,
    "F boundary for SOLID '%s': flux=%f\n",
    str_cget(&b->name),
    b->imposed_flux);
}

static FINLINE res_T
cp_f_boundary(struct f_boundary* dst, const struct f_boundary* src)
{
  str_copy(&dst->name, &src->name);
  dst->mat_id = src->mat_id;
  dst->imposed_flux = src->imposed_flux;
  return RES_OK;
}

struct solid_fluid_connect {
  struct str name;
  double emissivity;
  double specular_fraction;
  double hc;
};

static FINLINE void
release_sf_connect(struct solid_fluid_connect* connect)
{
  str_release(&connect->name);
}

static FINLINE res_T
init_sf(struct mem_allocator* allocator, struct solid_fluid_connect* dst)
{
  str_init(allocator, &dst->name);
  dst->emissivity = 0;
  dst->specular_fraction = 0;
  dst->hc = 0;
  return RES_OK;
}

static void
print_sf_connect
  (FILE* stream,
   const struct solid_fluid_connect* c)
{
  ASSERT(stream && c);
  fprintf(stream, "Solid-Fluid connection '%s':", str_cget(&c->name));
  fprintf(stream, " emissivity=%f, specular_fraction=%f",
      c->emissivity, c->specular_fraction);
  fprintf(stream, " hc=%f", c->hc);
  fprintf(stream, "\n");
}

static FINLINE res_T
cp_sf_connect(struct solid_fluid_connect* dst, const struct solid_fluid_connect* src)
{
  str_copy(&dst->name, &src->name);
  dst->specular_fraction = src->specular_fraction;
  dst->emissivity = src->emissivity;
  dst->hc = src->hc;
  return RES_OK;
}

static FINLINE res_T
cp_release_sf_connect(struct solid_fluid_connect* dst, struct solid_fluid_connect* src)
{
  return cp_sf_connect(dst, src);
}

struct description {
  enum description_type type;
  union {
    struct mat_fluid fluid;
    struct mat_solid solid;
    struct t_boundary t_boundary;
    struct f_boundary f_boundary;
    struct h_boundary h_boundary;
    struct solid_fluid_connect sf_connect;
  } d;
};

static FINLINE res_T
init_description(struct mem_allocator* alloc, struct description* desc)
{
  ASSERT(desc);
  (void)alloc;
  desc->type = DESCRIPTION_TYPE_COUNT__;
  return RES_OK;
}

static FINLINE void
release_description(struct description* desc)
{
  switch (desc->type) {
  case DESC_MAT_SOLID:
    release_solid(&desc->d.solid);
    break;
  case DESC_MAT_FLUID:
    release_fluid(&desc->d.fluid);
    break;
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    release_h_boundary(&desc->d.h_boundary);
    break;
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_T_FOR_FLUID:
    release_t_boundary(&desc->d.t_boundary);
    break;
  case DESC_BOUND_F_FOR_SOLID:
    release_f_boundary(&desc->d.f_boundary);
    break;
  case DESC_SOLID_FLUID_CONNECT:
    release_sf_connect(&desc->d.sf_connect);
    break;
  default:
    FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
  }
}

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
    print_f_boundary(stream, &desc->d.f_boundary);
    break;
  case DESC_SOLID_FLUID_CONNECT:
    print_sf_connect(stream, &desc->d.sf_connect);
    break;
  default:
    FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
  }
}

static FINLINE res_T
cp_description(struct description* dst, const struct description* src)
{
  res_T res = RES_OK;
  ASSERT(src && dst);
  switch (src->type) {
  case DESC_MAT_SOLID:
    if(dst->type == DESCRIPTION_TYPE_COUNT__) {
      dst->type = src->type;
      ERR(init_solid(src->d.solid.name.allocator, &dst->d.solid));
    }
    ERR(cp_solid(&dst->d.solid, &src->d.solid));
    break;
  case DESC_MAT_FLUID:
    if(dst->type == DESCRIPTION_TYPE_COUNT__) {
      dst->type = src->type;
      ERR(init_fluid(src->d.fluid.name.allocator, &dst->d.fluid));
    }
    ERR(cp_fluid(&dst->d.fluid, &src->d.fluid));
    break;
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    if(dst->type == DESCRIPTION_TYPE_COUNT__) {
      dst->type = src->type;
      ERR(init_h(src->d.h_boundary.name.allocator, &dst->d.h_boundary));
    }
    ERR(cp_h_boundary(&dst->d.h_boundary, &src->d.h_boundary));
    break;
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_T_FOR_FLUID:
    if(dst->type == DESCRIPTION_TYPE_COUNT__) {
      dst->type = src->type;
      ERR(init_t(src->d.t_boundary.name.allocator, &dst->d.t_boundary));
    }
    ERR(cp_t_boundary(&dst->d.t_boundary, &src->d.t_boundary));
    break;
  case DESC_BOUND_F_FOR_SOLID:
    if(dst->type == DESCRIPTION_TYPE_COUNT__) {
      dst->type = src->type;
      ERR(init_f(src->d.f_boundary.name.allocator, &dst->d.f_boundary));
    }
    ERR(cp_f_boundary(&dst->d.f_boundary, &src->d.f_boundary));
    break;
  case DESC_SOLID_FLUID_CONNECT:
    if(dst->type == DESCRIPTION_TYPE_COUNT__) {
      dst->type = src->type;
      ERR(init_sf(src->d.sf_connect.name.allocator, &dst->d.sf_connect));
    }
    ERR(cp_sf_connect(&dst->d.sf_connect, &src->d.sf_connect));
    break;
  default:
    FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
  }
end:
  return res;
error:
  goto end;
}

struct camera {
  double pos[3];
  double tgt[3];
  double up[3];
  double fov;
  unsigned spp;
  unsigned img[2];
  double time;
};

static void
init_camera(struct camera* cam) {
  d3_splat(cam->pos, 0);
  d3_splat(cam->tgt, 0);
  d3_splat(cam->up, 0);
  cam->fov = 0;
  cam->spp = 0;
  cam->img[0] = cam->img[1] = 0;
  cam->time = INF;
}

/* Type to store the primitives of a compute region */
#define DARRAY_NAME sides
#define DARRAY_DATA enum sdis_side
#include <rsys/dynamic_array.h>

#define DARRAY_NAME descriptions
#define DARRAY_DATA struct description
#define DARRAY_FUNCTOR_INIT init_description
#define DARRAY_FUNCTOR_COPY cp_description
#define DARRAY_FUNCTOR_RELEASE release_description
#include <rsys/dynamic_array.h>

struct compute_region {
  struct darray_size_t primitives;
  struct darray_sides sides;
  struct darray_uint err_triangles;
};

struct stardis {
  struct geometry geometry;
  struct darray_descriptions descriptions;  /* Materials and boundaries */

  double probe[4]; /* x,y,z,t of probe when mode is PROBE_COMPUTE */
  struct camera camera; /* camera when mode is IR_COMPUTE */
  struct str solve_name; /* medium name when mode is MEDIUM_COMPUTE,
                            boundary file name when [FLUX_]BOUNDARY_COMPUTE
                            map file name when MAP_COMPUTE */
  struct compute_region compute_region; /* Compute region when mode is
                                           BOUNDARY_COMPUTE or BOUNDARY_COMPUTE */
  size_t samples;
  unsigned nthreads;
  double scale_factor;
  double radiative_temp[2];
  struct mem_allocator* allocator;
  enum sdis_heat_path_flag dump_paths;
  int verbose;
};

extern LOCAL_SYM res_T
stardis_init
  (const struct args* args,
   struct stardis* stardis);

extern LOCAL_SYM res_T
stardis_compute
  (struct stardis* stardis,
   const int mode);

extern LOCAL_SYM void
stardis_release
  (struct stardis* stardis);

#endif /*STARDIS-APP_H*/
