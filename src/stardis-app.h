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

/* Forward declarations */
struct logger;
struct mem_allocator;
struct sdis_medium;

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

struct dummies {
  struct sdis_medium* dummy_fluid;
  unsigned dummy_fluid_id;
  struct sdis_medium* dummy_solid;
  unsigned dummy_solid_id;
};
#define DUMMIES_NULL__ {\
  NULL, UINT_MAX, NULL, UINT_MAX\
}

#define DARRAY_NAME media_ptr
#define DARRAY_DATA struct sdis_medium*
#include <rsys/dynamic_array.h>

struct geometry {
  struct sg3d_geometry* sg3d;
  struct darray_interface_ptrs interf_bytrg;
  struct darray_interface_ptrs interfaces;
};

static INLINE void
release_geometry(struct geometry* geom)
{
  size_t i;
  struct sdis_interface
    ** intf = darray_interface_ptrs_data_get(&geom->interfaces);
  if(geom->sg3d) SG3D(geometry_ref_put(geom->sg3d));
  for(i = 0; i < darray_interface_ptrs_size_get(&geom->interfaces); ++i)
    SDIS(interface_ref_put(intf[i]));
  darray_interface_ptrs_release(&geom->interfaces);
  darray_interface_ptrs_release(&geom->interf_bytrg);
}

static INLINE res_T
init_geometry
  (struct logger* logger,
   struct mem_allocator* allocator,
   const int verbose,
   struct geometry* geom)
{
  res_T res = RES_OK;
  struct sg3d_device* sg3d_dev = NULL;

  ASSERT(allocator && geom);
  
  geom->sg3d = NULL;
  darray_interface_ptrs_init(allocator, &geom->interfaces);
  darray_interface_ptrs_init(allocator, &geom->interf_bytrg);
  ERR(sg3d_device_create(logger, allocator, verbose, &sg3d_dev));
  ERR(sg3d_geometry_create(sg3d_dev, &geom->sg3d));

exit:
  if(sg3d_dev) SG3D(device_ref_put(sg3d_dev));
  return res;
error:
  release_geometry(geom);
  goto exit;
}

/*******************************************************************************
 * rsys/str additions
 ******************************************************************************/
static res_T
ensure_allocated(struct str* str, const size_t len, const char keep_old)
{
  char* buf = NULL;
  const size_t alloc_granularity = 32;
  size_t mod = 0;
  size_t new_len = 0;
  size_t new_size = 0;
  ASSERT(str);

 if(len * sizeof(char) <= str->allocated)
    return RES_OK;

  mod = len % alloc_granularity;
  new_len = !mod ? len : len - mod + alloc_granularity;
  new_size = new_len * sizeof(char);
  buf = MEM_ALLOC(str->allocator, new_size);
 if(!buf)
    return RES_MEM_ERR;

 if(keep_old) {
    strncpy(buf, str->cstr, new_len - 1);
    buf[new_len - 1] = '\0';
  }

  str->allocated = new_len * sizeof(char);
 if(str->cstr != str->buffer)
    MEM_RM(str->allocator, str->cstr);

  str->cstr = buf;
  return RES_OK;
}

static res_T
str_printf
(struct str* str,
  const char* fmt, ...)
#ifdef COMPILER_GCC
  __attribute((format(printf, 3, 4)))
#endif
{
  res_T res = RES_OK;
  size_t initial_len, left;
  va_list args_cp;
  int needed = 0;
  va_list args;
  ASSERT(str && fmt);
  va_start(args, fmt);

  initial_len = str_len(str);
  left = str->allocated - initial_len;
  ASSERT(left >= 0);
 if(left) {
    VA_COPY(args_cp, args);
    needed = vsnprintf(str->cstr+ initial_len, left, fmt, args_cp);
  }
 if(needed >= left) {
    ERR(ensure_allocated(str, initial_len + needed + 1, 1));
    left = str->allocated - initial_len;
    needed = vsnprintf(str->cstr+ initial_len, left, fmt, args);
    CHK(needed < left);
  }
  va_end(args);
end:
  return res;
error:
  goto end;
}
/******************************************************************************/

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

static res_T
print_fluid
  (struct str* str,
   const struct mat_fluid* f)
{
  res_T res = RES_OK;
  ASSERT(str && f);
  ERR(str_printf(str, "Fluid '%s': rho=%g cp=%g",
    str_cget(&f->name), f->rho, f->cp));
  if(f->tinit >= 0)
    ERR(str_printf(str, " Tinit=%g", f->tinit));
  if(f->imposed_temperature >= 0)
    ERR(str_printf(str, " Temp=%g", f->imposed_temperature));
end:
  return res;
error:
  goto end;
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

static res_T
print_solid
  (struct str* str,
   const struct mat_solid* s)
{
  res_T res = RES_OK;
  ASSERT(str && s);
  ERR(str_printf(str, "Solid '%s': lambda=%g rho=%g cp=%g delta=%g",
    str_cget(&s->name), s->lambda, s->rho, s->cp, s->delta));
  if(s->vpower != 0)
    ERR(str_printf(str, " VPower=%g", s->vpower));
  if(s->tinit >= 0)
    ERR(str_printf(str, " Tinit=%g", s->tinit));
  if(s->imposed_temperature >= 0)
    ERR(str_printf(str, " Temp=%g", s->imposed_temperature));
end:
  return res;
error:
  goto end;
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

static res_T
print_h_boundary
  (struct str* str,
   const struct h_boundary* b,
   const enum description_type type)
{
  res_T res = RES_OK;
  ASSERT(str && b
    && (type == DESC_BOUND_H_FOR_SOLID || type == DESC_BOUND_H_FOR_FLUID));
  ERR(str_printf(str,
    "H boundary for %s '%s': emissivity=%g specular_fraction=%g hc=%g T=%g "
    "(using medium %u as external medium)",
    (type == DESC_BOUND_H_FOR_SOLID ? "solid" : "fluid"), str_cget(&b->name),
    b->emissivity, b->specular_fraction, b->hc, b->imposed_temperature, b->mat_id));
end:
  return res;
error:
  goto end;
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
  double emissivity;
  double specular_fraction;
  double hc;
  double imposed_temperature;
};

static FINLINE res_T
init_t(struct mem_allocator* allocator, struct t_boundary* dst)
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
release_t_boundary(struct t_boundary* bound)
{
  str_release(&bound->name);
}

static res_T
print_t_boundary
  (struct str* str,
   const struct t_boundary* b,
   const enum description_type type)
{
  res_T res = RES_OK;
  ASSERT(str && b
    && (type == DESC_BOUND_T_FOR_SOLID || type == DESC_BOUND_T_FOR_FLUID));
  ERR(str_printf(str, "T boundary for %s' %s': T=%g ",
    (type == DESC_BOUND_T_FOR_SOLID ? "solid" : "fluid"),
    str_cget(&b->name), b->imposed_temperature));

  if(type == DESC_BOUND_T_FOR_FLUID)
    ERR(str_printf(str, "emissivity=%g, specular_fraction=%g hc=%g ",
      b->emissivity, b->specular_fraction, b->hc));

  ERR(str_printf(str, "(using medium %u as external medium)", b->mat_id));
end:
  return res;
error:
  goto end;
}

static FINLINE res_T
cp_t_boundary(struct t_boundary* dst, const struct t_boundary* src)
{
  str_copy(&dst->name, &src->name);
  dst->mat_id = src->mat_id;
  dst->emissivity = src->emissivity;
  dst->specular_fraction = src->specular_fraction;
  dst->hc = src->hc;
  dst->imposed_temperature = src->imposed_temperature;
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

static res_T
print_f_boundary
  (struct str* str,
   const struct f_boundary* b)
{
  res_T res = RES_OK;
  ASSERT(str && b);
  ERR(str_printf(str,
    "F boundary for SOLID '%s': flux=%g (using medium %u as external medium)",
    str_cget(&b->name), b->imposed_flux, b->mat_id));
end:
  return res;
error:
  goto end;
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

static res_T
print_sf_connect
  (struct str* str,
   const struct solid_fluid_connect* c)
{
  res_T res = RES_OK;
  ASSERT(str && c);
  ERR(str_printf(str, "Solid-Fluid connection '%s':", str_cget(&c->name)));
  ERR(str_printf(str, " emissivity=%g, specular_fraction=%g hc=%g",
      c->emissivity, c->specular_fraction, c->hc));
end:
  return res;
error:
  goto end;
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

static INLINE res_T
print_description
  (struct str* str,
   const struct description* desc)
{
  res_T res = RES_OK;
  ASSERT(str && desc);
  switch (desc->type) {
  case DESC_MAT_SOLID:
    ERR(print_solid(str, &desc->d.solid));
    break;
  case DESC_MAT_FLUID:
    ERR(print_fluid(str, &desc->d.fluid));
    break;
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_T_FOR_FLUID:
    ERR(print_t_boundary(str, &desc->d.t_boundary, desc->type));
    break;
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    ERR(print_h_boundary(str, &desc->d.h_boundary, desc->type));
    break;
  case DESC_BOUND_F_FOR_SOLID:
    ERR(print_f_boundary(str, &desc->d.f_boundary));
    break;
  case DESC_SOLID_FLUID_CONNECT:
    ERR(print_sf_connect(str, &desc->d.sf_connect));
    break;
  default:
    FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
  }
end:
  return res;
error:
  goto end;
}

static FINLINE res_T
cp_description
  (struct description* dst,
   const struct description* src)
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

static void
log_err(const char* msg, void* ctx)
{
  ASSERT(msg);
  (void)ctx;
#ifdef OS_WINDOWS
  fprintf(stderr, "error: %s", msg);
#else
  fprintf(stderr, "\x1b[31merror:\x1b[0m %s", msg);
#endif
}

static void
log_warn(const char* msg, void* ctx)
{
  ASSERT(msg);
  (void)ctx;
#ifdef OS_WINDOWS
  fprintf(stderr, "warning: %s", msg);
#else
  fprintf(stderr, "\x1b[33mwarning:\x1b[0m %s", msg);
#endif
}

static void
log_prt(const char* msg, void* ctx)
{
  ASSERT(msg);
  (void)ctx;
#ifdef OS_WINDOWS
  fprintf(stderr, "message: %s", msg);
#else
  fprintf(stderr, "\x1b[32moutput:\x1b[0m %s", msg);
#endif
}

struct counts {
  size_t smed_count, fmed_count, tbound_count, hbound_count,
    fbound_count, sfconnect_count;
};
#define COUNTS_NULL__ {\
  0, 0, 0, 0, 0, 0\
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

struct compute_surface {
  struct darray_size_t primitives;
  struct darray_sides sides;
  struct darray_uint err_triangles;
};

struct stardis {
  struct geometry geometry;
  struct sdis_scene* sdis_scn; /* The solver scene */
  struct darray_descriptions descriptions;  /* Materials and boundaries */
  struct darray_media_ptr media;
  struct counts counts;

  double probe[4]; /* x,y,z,t of probe when mode is PROBE_COMPUTE */
  struct camera camera; /* camera when mode is IR_COMPUTE */
  struct str solve_name; /* medium name when mode is MEDIUM_COMPUTE,
                            boundary file name when [FLUX_]BOUNDARY_COMPUTE
                            map file name when MAP_COMPUTE */
  struct compute_surface compute_surface; /* 2D compute region when mode is
                                             [FLUX_]BOUNDARY_COMPUTE
                                             or MAP_COMPUTE */
  int mode;
  size_t samples;
  unsigned nthreads;
  double scale_factor;
  double radiative_temp[2];
  struct mem_allocator* allocator;
  struct logger* logger;
  struct sdis_device* dev;
  enum sdis_heat_path_flag dump_paths;
  int verbose;
};

extern LOCAL_SYM res_T
stardis_init
  (const struct args* args,
   struct logger* logger,
   struct mem_allocator* allocator,
   struct stardis* stardis);

extern LOCAL_SYM void
stardis_release
  (struct stardis* stardis);

#endif /*STARDIS-APP_H*/
