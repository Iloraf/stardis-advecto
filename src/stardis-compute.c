/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#include "stardis-app.h"
#include <sdis.h>
#include <sdis_version.h>
#include <rsys/double3.h>
#include <rsys/double2.h>
#include <rsys/dynamic_array_uint.h>

#include <rsys/image.h>
#include<star/senc.h>

#include <rsys/hash_table.h>
#define HTABLE_NAME vrtx_rank
#define HTABLE_DATA unsigned
#define HTABLE_KEY unsigned
#include <rsys/hash_table.h>

#define HTABLE_NAME weigth
#define HTABLE_DATA double
#define HTABLE_KEY unsigned
#include <rsys/hash_table.h>

struct w_ctx {
  struct description* desc;
  struct htable_weigth weigths;
};

/* A type to limit variable use in tinyexpr expressions */
enum var_prohibited_t {
  ALL_VARS_ALLOWED = 0,
  T_PROHIBITED = BIT(4),
  XYZ_PROHIBITED = BIT(5),
  NO_VAR_ALLOWED = T_PROHIBITED | XYZ_PROHIBITED
};

static void
geometry_get_position
  (const size_t ivert, 
   double pos[3],
   void* context)
{
  struct geometry* geom = context;
  pos[0] = geom->vertex[ivert].xyz[0];
  pos[1] = geom->vertex[ivert].xyz[1];
  pos[2] = geom->vertex[ivert].xyz[2];
}

static void
geometry_get_indices
  (const size_t itri,
   size_t ids[3],
   void* context)
{
  struct geometry* geom = context;
  ids[0] = geom->triangle[itri].indices.data[0];
  ids[1] = geom->triangle[itri].indices.data[1];
  ids[2] = geom->triangle[itri].indices.data[2];
}

static void
geometry_get_interface
  (const size_t itri, 
   struct sdis_interface** interf, 
   void* context)
{
  struct geometry* geom = context;
  *interf = geom->interf_bytrg[itri];
}

static res_T
compile_expr_to_fn
  (struct te_expr** f,
   const char* math_expr,
   const int prohibited,
   int* is_zero)
{
  te_variable vars[4] = {
    TE_DEF_OFFSET("x", offsetof(struct sdis_rwalk_vertex, P[0])),
    TE_DEF_OFFSET("y", offsetof(struct sdis_rwalk_vertex, P[1])),
    TE_DEF_OFFSET("z", offsetof(struct sdis_rwalk_vertex, P[2])),
    TE_DEF_OFFSET("t", offsetof(struct sdis_rwalk_vertex, time))
  };
  int fst = 0, lst = 4;
  ASSERT(math_expr);
  ASSERT(prohibited == (prohibited & NO_VAR_ALLOWED)); /* Use only defined flags */
  if (prohibited & XYZ_PROHIBITED) fst = 3;
  if(prohibited & T_PROHIBITED) lst = 3;
  *f = te_compile(math_expr, vars+fst, lst-fst, NULL);
  if (!*f) {
    if ((prohibited != ALL_VARS_ALLOWED)
      && te_compile(math_expr, vars, 4, NULL))
      fprintf(stderr, "Expression %s use a probibited variable ", math_expr);
    if (prohibited == NO_VAR_ALLOWED)
      fprintf(stderr, "(no variable allowed)\n");
    else if (prohibited == XYZ_PROHIBITED)
      fprintf(stderr, "(xyz prohibited)\n");
    else {
      ASSERT(prohibited == T_PROHIBITED);
      fprintf(stderr, "(t prohibited)\n");
    }
    return RES_BAD_ARG;
  }
  if (is_zero) *is_zero = !((*f)->type == TE_CONSTANT && (*f)->v.value == 0);
  return RES_OK;
}

/*******************************************************************************
 * Fluid data
 ******************************************************************************/
struct fluid {
  char name[32];
  double cp; /* Calorific capacity */
  double rho; /* Volumic mass */
  /* Compute mode */
  int is_green, is_outside;
  double t0;
  /* TinyExpr stuff to compute temperature */
  struct te_expr *temp;
  struct te_expr *t_init;
  /* ID */
  unsigned id;
};

static double
fluid_get_calorific_capacity
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  (void)vtx;
  return fluid_props->cp;
}

static double
fluid_get_volumic_mass
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  (void)vtx;
  return fluid_props->rho;
}

static double
fluid_get_temperature
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  char msg[128];
  ASSERT(fluid_props->t_init || fluid_props->temp);
  if (vtx->time <= fluid_props->t0) {
    if (!fluid_props->t_init) {
      if (fluid_props->name[0])
        sprintf(msg,
          "fluid_get_temperature: getting undefined Tinit (fluid '%s')\n",
          fluid_props->name);
      else
        sprintf(msg, "fluid_get_temperature: getting undefined Tinit\n");
      FATAL(msg);
    }
    if (fluid_props->is_green) {
      if (fluid_props->name[0])
        sprintf(msg,
          "fluid_get_temperature: getting Tinit in green mode (fluid '%s')\n",
          fluid_props->name);
      else
        sprintf(msg, "fluid_get_temperature: getting Tinit in green mode\n");
      FATAL(msg);
    }
    return te_eval(fluid_props->t_init, vtx);
  }
  if (fluid_props->temp)
    return te_eval(fluid_props->temp, vtx);
  else return -1;
}

static void
release_fluid_data(void* f)
{
  struct fluid* fluid = (struct fluid*)f;
  te_free(fluid->t_init);
  te_free(fluid->temp);
}

/*******************************************************************************
 * Solid data
 ******************************************************************************/
struct solid {
  char name[32];
  double cp; /* Calorific capacity */
  double lambda; /* Conductivity */
  double rho; /* Volumic mass */
  double delta; /* Numerical parameter */
  /* Compute mode */
  int is_green, is_outside;
  double t0;
  /* TinyExpr stuff to compute temperature & power */
  struct te_expr *temp;
  struct te_expr *t_init;
  struct te_expr *power;
  /* ID */
  unsigned id;
};

static double
solid_get_calorific_capacity
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->cp;
}

static double
solid_get_thermal_conductivity
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->lambda;
}

static double
solid_get_volumic_mass
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->rho;
}

static double
solid_get_delta
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->delta;
}

#if Stardis_VERSION_MINOR == 3
static double
solid_get_delta_boundary
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return solid_props->delta
    /* Emperical scale factor that ensures that delta_boundary > delta withouht
     * being an exact multiple of delta. */
    /** 2.1 */;
}
#endif

static double
solid_get_temperature
(const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  char msg[128];
  ASSERT(solid_props->t_init || solid_props->temp);
  if (vtx->time <= solid_props->t0) {
    if (!solid_props->t_init) {
      if (solid_props->name[0])
        sprintf(msg,
          "solid_get_temperature: getting undefined Tinit (solid '%s')\n",
          solid_props->name);
      else
        sprintf(msg, "solid_get_temperature: getting undefined Tinit\n");
      FATAL(msg);
    }
    if (solid_props->is_green) {
      if (solid_props->name[0])
        sprintf(msg,
          "solid_get_temperature: getting Tinit in green mode (solid '%s')\n",
          solid_props->name);
      else
        sprintf(msg, "solid_get_temperature: getting Tinit in green mode\n");
      FATAL(msg);
    }
  }
  if (solid_props->temp)
    return te_eval(solid_props->t_init, vtx);
  else return -1;
}

static double
solid_get_power
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return te_eval(solid_props->power, vtx);
}

static void 
release_solid_data(void* s)
{
  struct solid* solid = (struct solid*)s;
  te_free(solid->t_init);
  te_free(solid->power);
}

/*******************************************************************************
 * Interface data
 ******************************************************************************/
struct intface {
  double hc; /* Convection coefficient */
  double emissivity;
  double alpha;
  /* TinyExpr stuff to compute temperature & flux */
  struct te_expr *temperature;
  struct te_expr *flux;
  /* IDs */
  unsigned front_boundary_id, back_boundary_id;
};

static double
interface_get_convection_coef
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->hc;
}

static double
interface_get_temperature
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);

  if (interface_props->temperature == NULL)
    return -1;
  
  return te_eval(interface_props->temperature, frag);
}

static double
interface_get_flux
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);

  if (interface_props->flux == NULL)
    return SDIS_FLUX_NONE;
  
  return te_eval(interface_props->flux, frag);
}

static void 
release_interface_data(void* s)
{
  struct intface* intface = (struct intface*)s;
  te_free(intface->temperature);
  te_free(intface->flux);
}

static double
interface_get_emissivity
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->emissivity;
}

static INLINE double
interface_get_alpha
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->alpha;
}

static res_T
select_probe_type
  (const struct sdis_scene* scn, 
   const struct triangle* triangle,
   const struct vertex* vertex,
   const struct description* descriptions,
   const int on_interface,
   double pos[3], 
   size_t* iprim, 
   double uv[2])
{
  res_T res = RES_OK;
  double min_d = DBL_MAX, min_delta = DBL_MAX, new_pos[3], tmp_uv[2], min_uv[2];
  const struct description *min_desc = NULL;
  size_t i = 0, found = SIZE_MAX;
  enum description_type min_type = DESCRIPTION_TYPE_COUNT__;

  for (i = 0; i < sa_size(triangle); ++i) {
    const struct triangle* tr = triangle + i;
    double dp[3], d, projected_pos[3], nn[3];
    float v0[3], v1[3], v2[3], e1[3], e2[3], n[3];
    const struct description *desc = NULL;
    enum sdis_side s;

    res = sdis_scene_boundary_project_position(scn, i, pos, tmp_uv);
    if (res != RES_OK) return res;
    res = sdis_scene_get_boundary_position(scn, i, tmp_uv, projected_pos);
    if (res != RES_OK) return res;
    d3_sub(dp, projected_pos, pos);
    d = d3_len(dp);

    if (d == 0) {
      /* Best possible match */
      found = i;
      fprintf(stderr,
        "The probe is on the primitive %llu.\n", (long long int)*iprim);
      break;
    }
    if (d >= min_d) {
      /* No improvement */
      continue;
    }
    min_d = d;
    d2_set(min_uv, tmp_uv);

    /* Find side */
    f3_set(v0, vertex[tr->indices.data[0]].xyz);
    f3_set(v1, vertex[tr->indices.data[1]].xyz);
    f3_set(v2, vertex[tr->indices.data[2]].xyz);
    f3_sub(e1, v1, v0);
    f3_sub(e2, v2, v0);
    f3_cross(n, e1, e2);

    /* Stardis solver convention is triangle normal is back side */
    d3_set_f3(nn, n);
    s = (d3_dot(nn, dp) < 0) ? SDIS_BACK : SDIS_FRONT;

    if (s == SDIS_FRONT) {
      if (tr->front_description != UINT_MAX) {
        desc = descriptions + tr->front_description;
      }
    } else {
      if (tr->back_description != UINT_MAX) {
        desc = descriptions + tr->back_description;
      }
    }

    min_desc = desc;
    if (!desc) continue;

    min_type = desc->type;
    if (desc->type == DESC_MAT_SOLID) {
      found = i;
      min_delta = desc->d.solid.delta;
      d3_set(new_pos, projected_pos);
    }
  }

  if (!min_desc) {
    /* Don't manage pos is outside the model
     * but very close and could be considered on the boundary */
    fprintf(stderr, "The probe is outside the model.\n");
    return RES_BAD_ARG;
  }

  if (on_interface) {
    *iprim = found;
    if (min_type == DESC_MAT_FLUID)
      fprintf(stderr, "Probe was on a fluid.\n");
    fprintf(stderr, "The probe is moved to %g,%g,%g (primitive %llu)\n",
      SPLIT3(new_pos), (long long int)*iprim);
    if (min_type == DESC_MAT_SOLID && min_d > min_delta)
      fprintf(stderr, "WARNING: The probe move is %.1f delta long.\n", min_d / min_delta);
    d3_set(pos, new_pos);
    d2_set(uv, min_uv);
  } else {
    if (min_type == DESC_MAT_FLUID)
      fprintf(stderr,
        "The probe is in a fluid: computing fluid temperature, not using a specific position.\n");
    if (min_type == DESC_MAT_SOLID && min_d < min_delta) {
      fprintf(stderr,
        "WARNING: The probe is very close to the primitive %llu (%.1f delta).\n",
        (long long int)found, min_d / min_delta);
      fprintf(stderr,
        "To have this probe moved on the closest interface use -P instead of -p\n");
    }
  }

  return res;
}

static void
dump_image(const struct sdis_estimator_buffer* buf)
{
  size_t definition[2];
  double* temps = NULL;
  size_t ix, iy;

  CHK(buf != NULL);
  CHK(sdis_estimator_buffer_get_definition(buf, definition) == RES_OK);

  temps = mem_alloc(definition[0] * definition[1] *sizeof(double));
  CHK(temps != NULL);

  /* Compute the per pixel temperature */
  fprintf(stdout,"# vtk DataFile Version 2.0\nvtk output\nASCII\nDATASET STRUCTURED_POINTS\n");
  fprintf(stdout,"DIMENSIONS %zu %zu 1\n", definition[0], definition[1]);
  fprintf(stdout,"ORIGIN 0 0 0\n");
  fprintf(stdout,"SPACING 1 1 1\n");
  fprintf(stdout,"POINT_DATA %zu\n", definition[0] * definition[1]);
  fprintf(stdout,"SCALARS temperature_estimate float 1\n");
  fprintf(stdout,"LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc T;
      CHK(sdis_estimator_buffer_at(buf, ix, iy, &estimator) == RES_OK);
      CHK(sdis_estimator_get_temperature(estimator, &T) == RES_OK);
      fprintf(stdout,"%f\n", T.E);
    }
  }
  fprintf(stdout, "SCALARS temperature_std_dev float 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc T;
      CHK(sdis_estimator_buffer_at(buf, ix, iy, &estimator) == RES_OK);
      CHK(sdis_estimator_get_temperature(estimator, &T) == RES_OK);
      fprintf(stdout, "%f\n", T.SE);
    }
  }
  fprintf(stdout, "SCALARS computation_time float 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc time;
      CHK(sdis_estimator_buffer_at(buf, ix, iy, &estimator) == RES_OK);
      CHK(sdis_estimator_get_realisation_time(estimator, &time) == RES_OK);
      fprintf(stdout, "%f\n", time.E);
    }
  }
  fprintf(stdout, "SCALARS computation_time_std_dev float 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc time;
      CHK(sdis_estimator_buffer_at(buf, ix, iy, &estimator) == RES_OK);
      CHK(sdis_estimator_get_realisation_time(estimator, &time) == RES_OK);
      fprintf(stdout, "%f\n", time.SE);
    }
  }
  fprintf(stdout, "SCALARS temperature_failures_count int 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      size_t nfails;
      CHK(sdis_estimator_buffer_at(buf, ix, iy, &estimator) == RES_OK);
      CHK(sdis_estimator_get_failure_count(estimator, &nfails) == RES_OK);
      fprintf(stdout, "%zu\n", nfails);
    }
  }
  mem_rm(temps);
}


/*******************************************************************************
 *
 ******************************************************************************/

struct int_descs {
  unsigned front, back, connect;
};
#define INT_DESCS_NULL__ { UINT_MAX, UINT_MAX, UINT_MAX }
static const struct int_descs INT_DESCS_NULL = INT_DESCS_NULL__;

static INLINE char
eq_desc(const struct int_descs* a, const struct int_descs* b)
{
  return (char)(a->front == b->front && a->back == b->back
    && a->connect == b->connect);
}

static INLINE size_t
hash_desc(struct int_descs const* key)
{
#ifdef ARCH_32BITS
  return (size_t)hash_fnv32(key, 3 * sizeof(unsigned));
#elif defined(ARCH_64BITS)
  return (size_t)hash_fnv64(key, 3 * sizeof(unsigned));
#else
#error "Unexpected architecture"
#endif
}

#include <rsys/hash_table.h>
/* Declare the hash table that map a vertex to its index */
#define HTABLE_NAME intface
#define HTABLE_DATA struct sdis_interface*
#define HTABLE_KEY struct int_descs
#define HTABLE_KEY_FUNCTOR_EQ eq_desc
#define HTABLE_KEY_FUNCTOR_HASH hash_desc
#include <rsys/hash_table.h>

static res_T
create_fluid
  (struct sdis_device* dev,
   const char* name,
   const double rho,
   const double cp,
   const int is_green,
   const int is_outside,
   const char* tinit_expr,
   const char* t_expr,
   struct sdis_medium*** media_ptr,
   unsigned* out_id)
{
  res_T res = RES_OK;
  struct sdis_fluid_shader fluid_shader = SDIS_FLUID_SHADER_NULL;
  struct sdis_data* data = NULL;
  struct fluid* fluid_props;
  size_t sz;
  /* Could be less restrictive if green output included positions/dates */
  int prohibited = is_green ? NO_VAR_ALLOWED : ALL_VARS_ALLOWED;

  ASSERT(dev && rho >= 0 && cp >= 0 && media_ptr && out_id);
  fluid_shader.calorific_capacity = fluid_get_calorific_capacity;
  fluid_shader.volumic_mass = fluid_get_volumic_mass;
  fluid_shader.temperature = fluid_get_temperature;
  res = sdis_data_create(dev, sizeof(struct fluid), ALIGNOF(struct fluid),
    release_fluid_data, &data);
  if (res != RES_OK) goto error;

  sz = sa_size(*media_ptr);
  ASSERT(sz < INT_MAX);
  fluid_props = sdis_data_get(data); /* Fetch the allocated memory space */
  if (name) strncpy(fluid_props->name, name, sizeof(fluid_props->name));
  else fluid_props->name[0] = '\0';
  fluid_props->cp = cp;
  fluid_props->rho = rho;
  fluid_props->t_init = NULL;
  fluid_props->temp = NULL;
  fluid_props->is_green = is_green;
  fluid_props->is_outside = is_outside;
  fluid_props->t0 = 0;
  fluid_props->id = (unsigned)sz;

  if (tinit_expr) {
    res = compile_expr_to_fn(&fluid_props->t_init, tinit_expr,
      prohibited | T_PROHIBITED, NULL);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid initial temperature expression: %s\n",
        t_expr);
      goto error;
    }
  }
  if (t_expr) {
    res = compile_expr_to_fn(&fluid_props->temp, t_expr, prohibited, NULL);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid temperature expression: %s\n",
        t_expr);
      goto error;
    }
  }
  res = sdis_fluid_create(dev, &fluid_shader, data, sa_add(*media_ptr, 1));
  if (res != RES_OK) goto error;
  *out_id = fluid_props->id;

end:
  if (data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}

static res_T
create_solid
  (struct sdis_device* dev,
   const char* name,
   const double lambda,
   const double rho,
   const double cp,
   const double delta,
   const int is_green,
   const int is_outside,
   const char* tinit_expr, /* Can be NULL if not used by getter */
   const char* t_expr, /* Can be NULL if not used by getter */
   const char* power_expr, /* Can be NULL */
   struct sdis_medium*** media_ptr,
   int* out_has_power, /* Can be NULL */
   unsigned* out_id)
{
  res_T res = RES_OK;
  struct sdis_solid_shader solid_shader = SDIS_SOLID_SHADER_NULL;
  struct sdis_data* data = NULL;
  struct solid* solid_props;
  size_t sz;
  /* Could be less restrictive if green output included positions/dates */
  int prohibited = is_green ? NO_VAR_ALLOWED : ALL_VARS_ALLOWED;

  ASSERT(dev && lambda >= 0 && rho >= 0 && cp >= 0 && delta > 0
    && media_ptr && out_id);
  solid_shader.calorific_capacity = solid_get_calorific_capacity;
  solid_shader.thermal_conductivity = solid_get_thermal_conductivity;
  solid_shader.volumic_mass = solid_get_volumic_mass;
  solid_shader.delta_solid = solid_get_delta;
#if Stardis_VERSION_MINOR == 3
  solid_shader.delta_boundary = solid_get_delta_boundary;
#endif
  solid_shader.temperature = solid_get_temperature;
  res = sdis_data_create(dev, sizeof(struct solid), ALIGNOF(struct solid),
    release_solid_data, &data);
  if (res != RES_OK) goto error;

  sz = sa_size(*media_ptr);
  ASSERT(sz < INT_MAX);
  solid_props = sdis_data_get(data); /* Fetch the allocated memory space */
  if (name) strncpy(solid_props->name, name, sizeof(solid_props->name));
  else solid_props->name[0] = '\0';
  solid_props->lambda = lambda;
  solid_props->rho = rho;
  solid_props->cp = cp;
  solid_props->delta = delta;
  solid_props->t_init = NULL;
  solid_props->temp = NULL;
  solid_props->power = NULL;
  solid_props->is_green = is_green;
  solid_props->is_outside = is_outside;
  solid_props->t0 = 0;
  solid_props->id = (unsigned)sz;
  if (tinit_expr) {
    res = compile_expr_to_fn(&solid_props->t_init, tinit_expr,
      prohibited | T_PROHIBITED, NULL);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid initial temperature expression: %s\n",
        t_expr);
      goto error;
    }
  }
  if (t_expr) {
    res = compile_expr_to_fn(&solid_props->temp, t_expr, prohibited, NULL);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid temperature expression: %s\n",
        t_expr);
      goto error;
    }
  }
  if (power_expr) {
    int has_power;
    res = compile_expr_to_fn(&solid_props->power, power_expr, prohibited, &has_power);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid volumic power expression: %s\n",
        power_expr);
      goto error;
    }
    if (out_has_power) *out_has_power = has_power;
    if(has_power) solid_shader.volumic_power = solid_get_power;
  }
  else {
    if (out_has_power) *out_has_power = 0;
  }
  res = sdis_solid_create(dev, &solid_shader, data, sa_add(*media_ptr, 1));
  if (res != RES_OK) goto error;
  *out_id = solid_props->id;

end:
  if (data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}

static res_T
create_edge_file_if
  (struct sdis_scene* scn,
   struct mem_allocator* allocator)
{
  res_T res = RES_OK;
  unsigned i, comp, scount, vcount;
  char name[64];
  FILE* obj = NULL;
  struct htable_vrtx_rank ranks;
  char structs_initialized = 0;
  struct senc_descriptor* analyze = NULL;
  struct darray_uint comps;
  struct darray_uint edges;
  unsigned cc_count = 0;
  unsigned *cc, *ee, uimax = UINT_MAX;

  res = sdis_scene_get_analysis(scn, &analyze);
  if (res != RES_OK) goto error;
  res = sdis_scene_release_analysis(scn);
  if (res != RES_OK) goto error;
  
  ASSERT(scn && allocator);

  res = senc_descriptor_get_frontier_segments_count(analyze, &scount);
  if (res != RES_OK) goto error;

  if(scount == 0)
    /* No edge to exit */
    goto end;

  fprintf(stderr, "Some frontier edges found: create OBJ files\n");

  htable_vrtx_rank_init(allocator, &ranks);
  darray_uint_init(allocator, &comps);
  darray_uint_init(allocator, &edges);
  structs_initialized = 1;

  /* Exit vertices, with a new numbering scheme */
  vcount = 0;
  FOR_EACH(i, 0, scount) {
    unsigned v, edge[2], rk[2];
    res = senc_descriptor_get_frontier_segment(analyze, i, edge);
    if(res != RES_OK) goto error;
    FOR_EACH(v, 0, 2) {
      unsigned* p_rank;
      p_rank = htable_vrtx_rank_find(&ranks, edge + v);
      if(p_rank) {
        rk[v] = *p_rank;
      } else {
        /* First appearance: allocate a rank and exit it */
        unsigned rank;
        size_t tmp = htable_vrtx_rank_size_get(&ranks);
        ASSERT(tmp < UINT_MAX);
        rank = (unsigned) tmp;
        res = htable_vrtx_rank_set(&ranks, edge + v, &rank);
        if (res != RES_OK) goto error;
        rk[v] = rank;
        darray_uint_push_back(&comps, &uimax);
        darray_uint_push_back(&edges, edge + v);
        vcount++;
      }
      /* Manage components */
      cc = darray_uint_data_get(&comps);
      if(cc[rk[0]] != UINT_MAX || cc[rk[1]] != UINT_MAX) {
        /* If both components known, they must be the same */
        ASSERT(cc[rk[0]] == UINT_MAX || cc[rk[1]] == UINT_MAX
          || cc[rk[0]] == cc[rk[1]]);
        FOR_EACH(v, 0, 2) {
          unsigned w = 1 - v; /* The other vertex */
          if (cc[rk[v]] == UINT_MAX) cc[rk[v]] = cc[rk[w]];
        }
      } else {
        /* None are in a know component: create a new one */
        cc[rk[0]] = cc[rk[1]] = cc_count++;
      }
    }
  }

  /* Exit segments by component using the new numbering scheme */
  cc = darray_uint_data_get(&comps);
  ee = darray_uint_data_get(&edges);
  FOR_EACH(comp, 0, cc_count) {
    unsigned v;
    snprintf(name, sizeof(name), "frontier_component_%u.obj", comp);
    obj = fopen(name, "w");
    if(!obj) goto error;
    /* Exit all vertices even unused
     * (same numbering scheme for all components) */
    FOR_EACH(v, 0, vcount) {
      double coord[3];
      res = senc_descriptor_get_global_vertex(analyze, ee[v], coord);
      if(res != RES_OK) goto error;
      fprintf(obj, "v %f %f %f\n", SPLIT3(coord));
    }
    FOR_EACH(i, 0, scount) {
      unsigned edge[2], new_ranks[2];
      res = senc_descriptor_get_frontier_segment(analyze, i, edge);
      if(res != RES_OK) goto error;
      ASSERT(cc[edge[0]] == cc[edge[1]]);
      if(cc[edge[0]] != comp)
        /* Segment not in this component */
        continue;
      FOR_EACH(v, 0, 2) {
        unsigned* p_rank;
        p_rank = htable_vrtx_rank_find(&ranks, edge + v);
        ASSERT(p_rank && *p_rank < vcount);
        new_ranks[v] = *p_rank;
      }
#define OBJ_IDX(Rank) ((Rank)+1)
      fprintf(obj, "l %u %u\n", OBJ_IDX(new_ranks[0]), OBJ_IDX(new_ranks[1]));
#undef OBJ_IDX
    }
    fclose(obj); obj = NULL;
  }

end:
  if(obj) fclose(obj);
  if(analyze) senc_descriptor_ref_put(analyze);
  if(structs_initialized) {
    htable_vrtx_rank_release(&ranks);
    darray_uint_release(&comps);
    darray_uint_release(&edges);
  }
  return res;
error:
  goto end;
}

static res_T
dump_path
  (const struct sdis_heat_path* path,
   void* context)
{
  FILE* stream = context;
  enum sdis_heat_path_flag status = SDIS_HEAT_PATH_NONE;
  size_t i, vcount;

  /* Header */
  fprintf(stream, "---\n");
  fprintf(stream, "# vtk DataFile Version 2.0\n");
  fprintf(stream, "Heat paths\n");
  fprintf(stream, "ASCII\n");
  fprintf(stream, "DATASET POLYDATA\n");
  /* Write path positions */
  sdis_heat_path_get_vertices_count(path, &vcount);
  fprintf(stream, "POINTS %lu double\n", (unsigned long)vcount);
  FOR_EACH(i, 0, vcount) {
	struct sdis_heat_vertex vtx;
	CHK(sdis_heat_path_get_vertex(path, i, &vtx) == RES_OK);
	fprintf(stream, "%g %g %g\n", SPLIT3(vtx.P));
  }
  /* Write the segment of the path */
  fprintf(stream, "LINES %lu %lu\n", 1lu, (unsigned long)(1 + vcount));
  fprintf(stream, "%lu", (unsigned long)vcount);
  FOR_EACH(i, 0, vcount) fprintf(stream, " %lu", (unsigned long)i);
  fprintf(stream, "\n");
  fprintf(stream, "POINT_DATA %lu\n", (unsigned long)vcount);
  /* Write the type of the random walk vertices */
  fprintf(stream, "SCALARS Vertex_Type float 1\n");
  fprintf(stream, "LOOKUP_TABLE vertex_type\n");
  FOR_EACH(i, 0, vcount) {
	  struct sdis_heat_vertex vtx;
	  CHK(sdis_heat_path_get_vertex(path, i, &vtx) == RES_OK);
	  switch (vtx.type) {
	  case SDIS_HEAT_VERTEX_CONDUCTION: fprintf(stream, "0.0\n"); break;
	  case SDIS_HEAT_VERTEX_CONVECTION: fprintf(stream, "0.5\n"); break;
	  case SDIS_HEAT_VERTEX_RADIATIVE:  fprintf(stream, "1.0\n"); break;
	  default: FATAL("Unreachable code.\n"); break;
	  }
  }
  fprintf(stream, "LOOKUP_TABLE vertex_type 3\n");
  fprintf(stream, "0.0 1.0 1.0 1.0\n"); /* 0.0 = Magenta: conduction */
  fprintf(stream, "1.0 1.0 0.0 1.0\n"); /* 0.5 = Yellow: convection */
  fprintf(stream, "1.0 0.0 1.0 1.0\n"); /* 1.0 = Purple: radiative */
  /* Write the weights of the random walk vertices */
  fprintf(stream, "SCALARS Weight double 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(i, 0, vcount) {
	  struct sdis_heat_vertex vtx;
	  CHK(sdis_heat_path_get_vertex(path, i, &vtx) == RES_OK);
	  fprintf(stream, "%g\n", vtx.weight);
  }
  /* Write the time of the random walk vertices */
  fprintf(stream, "SCALARS Time double 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(i, 0, vcount) {
	  struct sdis_heat_vertex vtx;
	  CHK(sdis_heat_path_get_vertex(path, i, &vtx) == RES_OK);
	  fprintf(stream, "%g\n", IS_INF(vtx.time) ? FLT_MAX : vtx.time);
  }
  /* Write path type */
  fprintf(stream, "CELL_DATA %lu\n", 1lu);
  fprintf(stream, "SCALARS Path_Type float 1\n");
  fprintf(stream, "LOOKUP_TABLE path_type\n");
  CHK(sdis_heat_path_get_status(path, &status) == RES_OK);
  switch (status) {
  case SDIS_HEAT_PATH_SUCCEED: fprintf(stream, "0.0\n"); break;
  case SDIS_HEAT_PATH_FAILED: fprintf(stream, "1.0\n"); break;
  default: FATAL("Unreachable code.\n"); break;
  }
  fprintf(stream, "LOOKUP_TABLE path_type 2\n");
  fprintf(stream, "0.0 0.0 1.0 1.0\n"); /* 0.0 = Blue: success */
  fprintf(stream, "1.0 0.0 0.0 1.0\n"); /* 1.0 = Red: failure */
  return RES_OK;
}

static res_T
print_power_term
  (struct sdis_medium* mdm,
   const double power_term,
   void* ctx)
{
  struct sdis_data* data = NULL;
  enum sdis_medium_type type;
  unsigned id;
  struct w_ctx* w_ctx = ctx;
  (void)w_ctx;
  CHK(mdm && ctx);

  data = sdis_medium_get_data(mdm);
  type = sdis_medium_get_type(mdm);

  switch (type) {
  case SDIS_FLUID: {
    FATAL("Unexpected power term in fluid");
#if 0
    struct fluid* d__ = sdis_data_get(data);
    id = d__->id;
    desc = (struct description*)ctx + id;
    ASSERT(id == desc->d.fluid.fluid_id);
    printf("\tF\t%u\t%g", id, power_term);
    break;
#endif
  }
  case SDIS_SOLID: {
    struct solid* d__ = sdis_data_get(data);
    id = d__->id;
    ASSERT(id == w_ctx->desc[id].d.solid.solid_id);
    printf("\tS\t%u\t%g", id, power_term);
    break;
  }
  default: FATAL("Unreachable code.\n"); break;
  }
  return RES_OK;
}

static res_T
get_flux_terms
  (struct sdis_interface* interf,
   const enum sdis_side side,
   const double flux_term,
   void* ctx)
{
  struct sdis_data* data = NULL;
  struct intface* d__;
  unsigned id;
  struct w_ctx* w_ctx = ctx;
  res_T res = RES_OK;

  CHK(interf && ctx);

  data = sdis_interface_get_data(interf);
  d__ = sdis_data_get(data);
  id = (side == SDIS_FRONT) ? d__->front_boundary_id : d__->back_boundary_id;

  switch (w_ctx->desc[id].type) {
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_T_FOR_FLUID:
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    FATAL("Cannot have a flux term here.\n"); break;
  case DESC_BOUND_F_FOR_SOLID: {
    double* w;
    ASSERT(id == w_ctx->desc[id].d.f_boundary.mat_id);
    w = htable_weigth_find(&w_ctx->weigths, &id);
    if (w)
      *w += flux_term;
    else {
      res = htable_weigth_set(&w_ctx->weigths, &id, &flux_term);
    }
    break;
  }
  default: FATAL("Unreachable code.\n"); break;
  }
  return res;
}

static res_T
print_sample
  (struct sdis_green_path* path,
   void* ctx)
{
  struct sdis_point pt = SDIS_POINT_NULL;
  struct sdis_data* data = NULL;
  enum sdis_medium_type type;
  struct htable_weigth_iterator it, end;
  unsigned id;
  size_t pcount, fcount;
  struct w_ctx* w_ctx = ctx;
  res_T res = RES_OK;
  CHK(path && ctx);

  res = sdis_green_path_get_limit_point(path, &pt);
  if (res != RES_OK) goto error;


  /* For each path, prints:
   * # end #power_terms #flux_terms power_term_1  ... power_term_n flux_term_1 ... flux_term_n
   * with:
   * - end = end_type end_id; end_type = T | H | X | R | F | S
   * - power_term_i = power_type_i power_id_i factor_i
   * - flux_term_i = flux_id_i factor_i
   */

  switch (pt.type) {
  case SDIS_FRAGMENT: {
    struct intface* d__;
    data = sdis_interface_get_data(pt.data.itfrag.intface);
    d__ = sdis_data_get(data);
    id = (pt.data.itfrag.fragment.side == SDIS_FRONT)
      ? d__->front_boundary_id : d__->back_boundary_id;
    ASSERT(id != UINT_MAX);
    switch (w_ctx->desc[id].type) {
    case DESC_BOUND_T_FOR_SOLID:
    case DESC_BOUND_T_FOR_FLUID:
      ASSERT(id == w_ctx->desc[id].d.t_boundary.mat_id);
      printf("T\t%u", id);
      break;
    case DESC_BOUND_H_FOR_SOLID:
    case DESC_BOUND_H_FOR_FLUID:
      ASSERT(id == w_ctx->desc[id].d.h_boundary.mat_id);
      printf("H\t%u", id);
      break;
    case DESC_BOUND_F_FOR_SOLID:
      ASSERT(id == w_ctx->desc[id].d.f_boundary.mat_id);
      printf("X\t%u", id);
      break;
    default: FATAL("Unreachable code.\n"); break;
    }
    break;
  }
  case SDIS_VERTEX:
    type = sdis_medium_get_type(pt.data.mdmvert.medium);
    data = sdis_medium_get_data(pt.data.mdmvert.medium);
    if (pt.data.mdmvert.vertex.P[0] == INF) {
      /* Radiative output */
      printf("R\t%u", (unsigned)sa_size(w_ctx->desc));
    }
    else if (type == SDIS_FLUID) {
      struct fluid* d__ = sdis_data_get(data);
      id = d__->id;
      ASSERT(id == w_ctx->desc[id].d.fluid.fluid_id);
      if (d__->is_outside)
        /* If outside the model and in a fluid with known temperature,
         * its a fluid attached to a H boundary */
        printf("H\t%u", id);
      /* In a standard fluid with known temperature */
      else printf("F\t%u", id);

    } else {
      struct solid* d__ = sdis_data_get(data);
      ASSERT(type == SDIS_SOLID);
      id = d__->id;
      ASSERT(id == w_ctx->desc[id].d.solid.solid_id);
      ASSERT(!d__->is_outside); /* FIXME: what if in external solid? */
      printf("S\t%u", id);
    }
    break;
  default: FATAL("Unreachable code.\n"); break;
  }

  res = sdis_green_function_get_power_terms_count(path, &pcount);
  if (res != RES_OK) goto error;

  htable_weigth_clear(&w_ctx->weigths);
  res = sdis_green_path_for_each_flux_term(path, get_flux_terms, w_ctx);
  if (res != RES_OK) goto error;
  fcount = htable_weigth_size_get(&w_ctx->weigths);

  printf("\t%zu\t%zu", pcount, fcount);

  res = sdis_green_path_for_each_power_term(path, print_power_term, ctx);
  if (res != RES_OK) goto error;

  htable_weigth_begin(&w_ctx->weigths, &it);
  htable_weigth_end(&w_ctx->weigths, &end);
  while (!htable_weigth_iterator_eq(&it, &end)) {
    double* w = htable_weigth_iterator_data_get(&it);
    unsigned* k = htable_weigth_iterator_key_get(&it);
    printf("\t%u\t%g", *k, *w);
    htable_weigth_iterator_next(&it);
  }
  printf("\n");

error:
  return res;
}

res_T
stardis_compute(struct stardis* stardis, enum stardis_mode mode)
{
  res_T res = RES_OK;
  struct sdis_device* dev = NULL;
  struct sdis_medium** media = NULL;
  struct sdis_interface** interfaces = NULL;
  struct sdis_interface** interf_bytrg = NULL;
  struct sdis_data* data = NULL;

  struct sdis_scene* scn = NULL;
  struct sdis_estimator* estimator = NULL;
  struct sdis_mc temperature;
  struct sdis_estimator_buffer* buf = NULL;
  struct sdis_camera* cam = NULL;

  struct sdis_medium* dummy_fluid = NULL;
  unsigned dummy_fluid_id = UINT_MAX;
  struct sdis_medium* dummy_solid = NULL;
  unsigned dummy_solid_id = UINT_MAX;

  struct htable_intface htable_interfaces;
  int htable_interfaces_initialised = 0;
  double pos[3] = { 0,0,0 };
  double time[2] = { 0, 0 };
  size_t nfailures;
  size_t smed_count = 0, fmed_count = 0,
    tbound_count = 0, hbound_count = 0, fbound_count = 0, sfconnect_count = 0;
  unsigned i = 0;

  res = sdis_device_create(NULL, &stardis->allocator, stardis->nthreads, 1, &dev);
  if (res != RES_OK) goto error;

  /* Create media and property holders found in descriptions */
  for (i = 0; i < sa_size(stardis->descriptions); ++i) {
    struct description* desc = stardis->descriptions + i;
    /* Could be less restrictive if green output included positions/dates */
    int prohibited = (mode & GREEN_MODE) ? NO_VAR_ALLOWED : ALL_VARS_ALLOWED;

    switch (desc->type) {
    case DESC_BOUND_H_FOR_SOLID:
      hbound_count++;
      /* Create an external fluid with bound temp as fluid temp */
      res = create_fluid(dev, NULL, 1, 1, (mode & GREEN_MODE), 1, NULL,
        desc->d.h_boundary.T, &media, &desc->d.h_boundary.mat_id);
      if (res != RES_OK) goto error;
      break;
    case DESC_BOUND_H_FOR_FLUID:
      /* Create an external solid with bound temp as solid temp */
      hbound_count++;
      res = create_solid(dev, NULL, INF, 1, 1, 1, (mode & GREEN_MODE), 1, NULL,
        desc->d.h_boundary.T, NULL, &media, NULL, &desc->d.h_boundary.mat_id);
      if (res != RES_OK) goto error;
      break;
    case DESC_BOUND_T_FOR_SOLID:
      tbound_count++;
      ASSERT(desc->d.t_boundary.T != NULL);
      res = compile_expr_to_fn(&desc->d.t_boundary.te_temperature,
        desc->d.t_boundary.T, prohibited, NULL);
      if (res != RES_OK) {
        fprintf(stderr, "Invalid boundary temperature expression: %s\n",
          desc->d.t_boundary.T);
        goto error;
      }
      if (dummy_fluid) {
        /* Reuse external dummy fluid */
        desc->d.t_boundary.mat_id = dummy_fluid_id;
      } else {
        /* Create dummy fluid */
        res = create_fluid(dev, NULL, 1, 1, (mode & GREEN_MODE), 1, NULL,
          NULL, &media, &desc->d.t_boundary.mat_id);
        if (res != RES_OK) goto error;
        dummy_fluid = sa_last(media);
        dummy_fluid_id = desc->d.t_boundary.mat_id;
      }
      break;
    case DESC_BOUND_T_FOR_FLUID:
      tbound_count++;
      ASSERT(desc->d.t_boundary.T != NULL);
      res = compile_expr_to_fn(&desc->d.t_boundary.te_temperature,
        desc->d.t_boundary.T, prohibited, NULL);
      if (res != RES_OK) {
        fprintf(stderr, "Invalid boundary temperature expression: %s\n",
          desc->d.t_boundary.T);
        goto error;
      }
      if (dummy_solid) {
        /* Reuse external dummy solid */
        desc->d.t_boundary.mat_id = dummy_solid_id;
      } else {
        /* Create dummy solid */
        res = create_solid(dev, NULL, 1, 1, 1, 1, (mode & GREEN_MODE), 1, NULL,
          NULL, NULL, &media, NULL, &desc->d.t_boundary.mat_id);
        if (res != RES_OK) goto error;
        dummy_solid = sa_last(media);
        dummy_solid_id = desc->d.t_boundary.mat_id;
      }
      break;
    case DESC_BOUND_F_FOR_SOLID:
      fbound_count++;
      ASSERT(desc->d.f_boundary.flux != NULL);
      res = compile_expr_to_fn(&desc->d.f_boundary.te_flux,
        desc->d.f_boundary.flux, 1, NULL);
      if (res != RES_OK) {
        fprintf(stderr, "Invalid boundary flux expression: %s\n",
          desc->d.f_boundary.flux);
        goto error;
      }
      if (dummy_fluid) {
        /* Reuse external dummy fluid */
        desc->d.f_boundary.mat_id = dummy_fluid_id;
      } else {
        /* Create dummy fluid */
        res = create_fluid(dev, NULL, 1, 1, (mode & GREEN_MODE), 1, NULL, NULL,
          &media, &desc->d.f_boundary.mat_id);
        if (res != RES_OK) goto error;
        dummy_fluid = sa_last(media);
        dummy_fluid_id = desc->d.f_boundary.mat_id;
      }
      break;
    case DESC_SOLID_FLUID_CONNECT:
      sfconnect_count++;
      ASSERT(desc->d.sf_connect.specular_fraction >= 0
        && desc->d.sf_connect.specular_fraction <= 1);
      ASSERT(desc->d.sf_connect.emissivity >= 0);
      ASSERT(desc->d.sf_connect.hc >= 0);
      break;
    case DESC_MAT_SOLID:
      smed_count++;
      res = create_solid(dev,
        stardis->descriptions[i].d.solid.name,
        stardis->descriptions[i].d.solid.lambda,
        stardis->descriptions[i].d.solid.rho,
        stardis->descriptions[i].d.solid.cp,
        stardis->descriptions[i].d.solid.delta,
        (mode & GREEN_MODE),
        0,
        stardis->descriptions[i].d.solid.Tinit,
        stardis->descriptions[i].d.solid.Temp,
        stardis->descriptions[i].d.solid.power,
        &media,
        &stardis->descriptions[i].d.solid.has_power,
        &desc->d.solid.solid_id);
      if (res != RES_OK) goto error;
      break;
    case DESC_MAT_FLUID:
      fmed_count++;
      res = create_fluid(dev,
        stardis->descriptions[i].d.fluid.name,
        stardis->descriptions[i].d.fluid.rho,
        stardis->descriptions[i].d.fluid.cp,
        (mode & GREEN_MODE),
        0,
        stardis->descriptions[i].d.fluid.Tinit,
        stardis->descriptions[i].d.fluid.Temp,
        &media,
        &desc->d.fluid.fluid_id);
      if (res != RES_OK) goto error;

      break;
    default: FATAL("Invalid type.\n");
    }
  }

  /* Create interfaces */
  htable_intface_init(&stardis->allocator, &htable_interfaces);
  htable_interfaces_initialised = 1;
  for (i = 0; i < sa_size(stardis->geometry.triangle); ++i) {
    struct triangle *trg = stardis->geometry.triangle + i;
    struct int_descs int_descs = INT_DESCS_NULL;
    struct sdis_interface** p_intface;
    struct sdis_interface* intface;
    struct sdis_medium* front_med = NULL;
    struct sdis_medium* back_med = NULL;
    struct sdis_interface_side_shader* fluid_side_shader = NULL;
    unsigned fd = trg->front_description;
    unsigned bd = trg->back_description;
    unsigned cd = trg->connection_description;
    int fluid_count = 0, solid_count = 0;
    int solid_fluid_connection_count = 0, connection_count = 0, boundary_count = 0;

    /* Create key */
    int_descs.front = fd;
    int_descs.back = bd;
    int_descs.connect = trg->connection_description;

    /* Search if interface already exists, or create it */
    p_intface = htable_intface_find(&htable_interfaces, &int_descs);
    if (p_intface) {
      intface = *p_intface;
    } else {
      /* create new interface and register */
      struct sdis_interface_shader interface_shader = SDIS_INTERFACE_SHADER_NULL;
      struct intface* interface_props = NULL;
      unsigned id;
      int front_defined = (fd != UINT_MAX);
      int back_defined = (bd != UINT_MAX);
      int connect_defined = (cd != UINT_MAX);

      SDIS(data_create(dev, sizeof(struct intface), ALIGNOF(struct intface),
        release_interface_data, &data));
      interface_props = sdis_data_get(data);
      interface_props->temperature = NULL;
      interface_props->flux = NULL;
      interface_props->front_boundary_id = UINT_MAX;
      interface_props->back_boundary_id = UINT_MAX;

      if (front_defined) {
        switch (stardis->descriptions[fd].type) {
        case DESC_MAT_SOLID:
          id = stardis->descriptions[fd].d.solid.solid_id;
          solid_count++;
          front_med = media[id];
          break;
        case DESC_MAT_FLUID:
          fluid_count++;
          id = stardis->descriptions[fd].d.fluid.fluid_id;
          front_med = media[id];
          fluid_side_shader = &interface_shader.front;
          break;
        default: FATAL("Invalid type.\n");
        }
      }
      if (back_defined) {
        switch (stardis->descriptions[bd].type) {
        case DESC_MAT_SOLID:
          id = stardis->descriptions[bd].d.solid.solid_id;
          solid_count++;
          back_med = media[id];
          break;
        case DESC_MAT_FLUID:
          fluid_count++;
          id = stardis->descriptions[bd].d.fluid.fluid_id;
          back_med = media[id];
          fluid_side_shader = &interface_shader.back;
          break;
        default: FATAL("Invalid type.\n");
        }
      }
      if (connect_defined) {
        const struct description* connect = stardis->descriptions + cd;
        int type_checked = 0;
        struct sdis_medium* def_medium = NULL;
        unsigned ext_id;
        if (connect->type == DESC_SOLID_FLUID_CONNECT) {
          if (solid_count != 1 || fluid_count != 1) {
            ASSERT(front_defined && back_defined);
            fprintf(stderr,
              "Can only define a DESC_SOLID_FLUID_CONNECT between a fluid and a solid\n");
            res = RES_BAD_ARG;
            goto error;
          }
        } else {
          if (front_defined == back_defined) {
            fprintf(stderr,
              "Cannot define a boundary between 2 %s regions\n",
              front_defined ? "defined" : "undefined");
            res = RES_BAD_ARG;
            goto error;
          }
          def_medium = front_defined ? front_med : back_med;
          if (front_defined)
            interface_props->front_boundary_id = cd;
          else interface_props->back_boundary_id = cd;
        }
        switch (connect->type) {
        case DESC_BOUND_H_FOR_FLUID:
          if (sdis_medium_get_type(def_medium) != SDIS_FLUID) {
            fprintf(stderr,
              "Can only define a DESC_BOUND_H_FOR_FLUID boundary for a fluid region\n");
            res = RES_BAD_ARG;
            goto error;
          }
          type_checked = 1;
          /* fall through */
        case DESC_BOUND_H_FOR_SOLID:
          if (!type_checked
            &&  sdis_medium_get_type(def_medium) != SDIS_SOLID) {
            fprintf(stderr,
              "Can only define a DESC_BOUND_H_FOR_SOLID boundary for a solid region\n");
            res = RES_BAD_ARG;
            goto error;
          }
          ext_id = connect->d.h_boundary.mat_id; /* External material id */
          ASSERT(ext_id < sa_size(media));
          ASSERT(sdis_medium_get_type(media[ext_id]) ==
            (connect->type == DESC_BOUND_H_FOR_SOLID ? SDIS_FLUID : SDIS_SOLID));
          connection_count++;
          boundary_count++;
          if (front_defined) back_med = media[ext_id];
          else front_med = media[ext_id];
          interface_shader.convection_coef = interface_get_convection_coef;
          interface_shader.convection_coef_upper_bound = connect->d.h_boundary.hc_max;
          interface_props->hc = connect->d.h_boundary.hc;
          ASSERT(!fluid_side_shader); /* Cause defined medium is solid */
          if (connect->d.h_boundary.has_emissivity) {
            fluid_side_shader =
              front_defined ? &interface_shader.back : &interface_shader.front;
            fluid_side_shader->emissivity = interface_get_emissivity;
            interface_props->emissivity = connect->d.h_boundary.emissivity;
            interface_props->alpha = connect->d.h_boundary.specular_fraction;
          }
          break;
        case DESC_BOUND_T_FOR_FLUID:
          if (sdis_medium_get_type(def_medium) != SDIS_FLUID) {
            fprintf(stderr,
              "Can only define a DESC_BOUND_T_FOR_FLUID boundary for a fluid region\n");
            res = RES_BAD_ARG;
            goto error;
          }
          type_checked = 1;
          /* fall through */
        case DESC_BOUND_T_FOR_SOLID:
          if (!type_checked
            &&  sdis_medium_get_type(def_medium) != SDIS_SOLID) {
            fprintf(stderr,
              "Can only define a DESC_BOUND_T_FOR_SOLID boundary for a solid region\n");
            res = RES_BAD_ARG;
            goto error;
          }
          ext_id = connect->d.t_boundary.mat_id; /* External material id */
          ASSERT(ext_id < sa_size(media));
          ASSERT(sdis_medium_get_type(media[ext_id]) == SDIS_FLUID);
          connection_count++;
          boundary_count++;
          if (front_defined) {
            back_med = media[ext_id];
            /* We set the known T inside
             * TODO: should be outside to allow contact resistances */
            interface_shader.front.temperature = interface_get_temperature;
          } else {
            front_med = media[ext_id];
            /* We set the known T inside
             * TODO: should be outside to allow contact resistances */
            interface_shader.back.temperature = interface_get_temperature;
          }
          ASSERT(connect->d.t_boundary.te_temperature);
          interface_props->temperature
            = te_duplicate(connect->d.t_boundary.te_temperature);
          if (connect->d.t_boundary.has_hc) {
            ASSERT(connect->type == DESC_BOUND_T_FOR_FLUID);
            interface_shader.convection_coef = interface_get_convection_coef;
            interface_shader.convection_coef_upper_bound = connect->d.t_boundary.hc_max;
            interface_props->hc = connect->d.t_boundary.hc;
          }
          break;
        case DESC_BOUND_F_FOR_SOLID:
          if (sdis_medium_get_type(def_medium) != SDIS_SOLID) {
            fprintf(stderr,
              "Can only define a DESC_BOUND_F_FOR_SOLID boundary for a solid region\n");
            res = RES_BAD_ARG;
            goto error;
          }
          connection_count++;
          boundary_count++;
          if (front_defined) {
            back_med = media[connect->d.f_boundary.mat_id];
            interface_shader.front.flux = interface_get_flux;
          } else {
            front_med = media[connect->d.f_boundary.mat_id];
            interface_shader.back.flux = interface_get_flux;
          }
          ASSERT(connect->d.f_boundary.te_flux);
          interface_props->flux = te_duplicate(connect->d.f_boundary.te_flux);
          break;
        case DESC_SOLID_FLUID_CONNECT:
          /* Both front and back should be defined;
           * if not will raise an error below */
          connection_count++;
          solid_fluid_connection_count++;
          ASSERT(fluid_side_shader);
          if (connect->d.sf_connect.has_hc) {
            interface_shader.convection_coef = interface_get_convection_coef;
            interface_shader.convection_coef_upper_bound = connect->d.sf_connect.hc_max;
            interface_props->hc = connect->d.sf_connect.hc;
          }
          if (connect->d.sf_connect.has_emissivity) {
            fluid_side_shader->emissivity = interface_get_emissivity;
            interface_props->emissivity = connect->d.sf_connect.emissivity;
            interface_props->alpha = connect->d.sf_connect.specular_fraction;
          }
          break;
        default: FATAL("Invalid type.\n");
        }
      }

      if ((fluid_count == 2)
        || (fluid_count + solid_count + connection_count < 2)
        || (boundary_count ?
        (fluid_count + solid_count != 1) : (fluid_count + solid_count != 2))
        || (solid_fluid_connection_count && (fluid_count != 1 || solid_count != 1)))
      {
        /* Incoherent triangle description */
        fprintf(stderr, "Incoherent triangle description (%u)\n", i);
        print_trg_as_obj(stderr, stardis->geometry.vertex, trg->indices.data);
        fprintf(stderr, "Front: ");
        if (!front_defined) fprintf(stderr, "undefined\n");
        else print_description(stderr, &stardis->descriptions[fd]);
        fprintf(stderr, "Back: ");
        if (!back_defined) fprintf(stderr, "undefined\n");
        else print_description(stderr, &stardis->descriptions[bd]);
        fprintf(stderr, "Connection: ");
        if (!connect_defined) fprintf(stderr, "undefined\n");
        else print_description(stderr, &stardis->descriptions[cd]);
        res = RES_BAD_ARG;
        goto error;
      }

      res = sdis_interface_create(dev, front_med, back_med,
        &interface_shader, data, &intface);
      SDIS(data_ref_put(data)); data = NULL;
      if (res != RES_OK) {
        fprintf(stderr,
          "Cannot create interface associated to triangle %u\n", i);
        goto error;
      }
      sa_push(interfaces, intface);
      res = htable_intface_set(&htable_interfaces, &int_descs, &intface);
      if (res != RES_OK) goto error;
    }
    sa_push(interf_bytrg, intface);
  }
  stardis->geometry.interf_bytrg = interf_bytrg;
  stardis->geometry.interfaces = interfaces;

  res = sdis_scene_create(dev,
    sa_size(stardis->geometry.triangle),
    geometry_get_indices, geometry_get_interface,
    sa_size(stardis->geometry.vertex),
    geometry_get_position, &stardis->geometry, &scn);
  if (res != RES_OK) goto error;

  res = create_edge_file_if(scn, &stardis->allocator);
  if (res != RES_OK) goto error;

  if (mode & GREEN_MODE) {
    struct sdis_green_function* green = NULL;
    size_t ok_count, failed_count;
    struct w_ctx w_ctx;
    ASSERT((mode & PROBE_COMPUTE) || (mode & PROBE_COMPUTE_ON_INTERFACE)
      || (mode & BOUNDARY_COMPUTE) || (mode & MEDIUM_COMPUTE));

    if (mode & PROBE_COMPUTE || mode & PROBE_COMPUTE_ON_INTERFACE) {
      double uv[2] = { 0,0 };
      size_t iprim = SIZE_MAX;
      /* Launch the probe simulation */
      pos[0] = stardis->probe[0];
      pos[1] = stardis->probe[1];
      pos[2] = stardis->probe[2];

      res = select_probe_type(scn,
        stardis->geometry.triangle,
        stardis->geometry.vertex,
        stardis->descriptions,
        (mode & PROBE_COMPUTE_ON_INTERFACE),
        pos,
        &iprim,
        uv);
      if (res != RES_OK) goto error;

      if (iprim == SIZE_MAX) {
        res = sdis_solve_probe_green_function(scn,
          stardis->N,
          pos,
          stardis->scale_factor,
          stardis->radiative_temp[0],
          stardis->radiative_temp[1],
          &green);
      } else {
        res = sdis_solve_probe_boundary_green_function(scn,
          stardis->N,
          iprim,
          uv,
          SDIS_FRONT,
          stardis->scale_factor,
          stardis->radiative_temp[0],
          stardis->radiative_temp[1],
          &green);
      }
    }
    else if (mode & BOUNDARY_COMPUTE) {
      ASSERT(sa_size(stardis->boundary.primitives)
        == sa_size(stardis->boundary.sides));
      if (sa_size(stardis->boundary.sides) == 0) {
        fprintf(stderr, "Cannot solve boundary %s (empty geometry)\n",
          stardis->solve_name);
      }

      res = sdis_solve_boundary_green_function(scn,
        stardis->N,
        stardis->boundary.primitives,
        stardis->boundary.sides,
        sa_size(stardis->boundary.sides),
        stardis->scale_factor,
        stardis->radiative_temp[0],
        stardis->radiative_temp[1],
        &green);
    }
    else if (mode & MEDIUM_COMPUTE) {
      size_t sz, ii;
      struct sdis_medium* medium = NULL;
      /* Find medium */
      sz = sa_size(stardis->descriptions);
      FOR_EACH(ii, 0, sz) {
        struct description* desc = stardis->descriptions + ii;
        if (desc->type == DESC_MAT_SOLID) {
          if (0 == strcmp(stardis->solve_name, desc->d.solid.name)) {
            ASSERT(sa_size(media) > ii);
            medium = media[ii];
            break;
          }
        }
        else if (desc->type == DESC_MAT_FLUID) {
          if (0 == strcmp(stardis->solve_name, desc->d.fluid.name)) {
            ASSERT(sa_size(media) > ii);
            medium = media[ii];
            break;
          }
        }
      }
      if (medium == NULL) {
        /* Not found */
        fprintf(stderr, "Cannot solve medium %s (unknown medium)\n",
          stardis->solve_name);
        res = RES_BAD_ARG;
        goto error;
      }
      res = sdis_solve_medium_green_function(scn,
        stardis->N,
        medium,
        stardis->scale_factor,
        stardis->radiative_temp[0],
        stardis->radiative_temp[1],
        &green);
    }
    if (res != RES_OK) goto error;

    /* Output the green function */
    CHK(sdis_green_function_get_invalid_paths_count(green, &failed_count) == RES_OK);
    CHK(sdis_green_function_get_paths_count(green, &ok_count) == RES_OK);

    /* Output counts */
    printf("---BEGIN GREEN---\n");
    printf("# #solids #fluids #t_boundaries #h_boundaries #f_boundaries #ok #failures\n");
    printf("%zu %zu %zu %zu %zu %zu %zu\n",
      smed_count, fmed_count,
      tbound_count, hbound_count, fbound_count,
      ok_count, failed_count);

    /* List Media */
    if (smed_count) {
      printf("# Solids\n");
      printf("# ID Name lambda rho cp power\n");
      FOR_EACH(i, 0, (unsigned)sa_size(stardis->descriptions)) {
        struct description* desc = stardis->descriptions + i;
        struct mat_solid* sl;
        if (desc->type != DESC_MAT_SOLID) continue;
        sl = &desc->d.solid;
        printf("%u\t%s\t%g\t%g\t%g\t%s\n",
          i, sl->name, sl->lambda, sl->rho, sl->cp, (sl->has_power ? sl->power : "0"));
      }
    }
    if (fmed_count) {
      printf("# Fluids\n");
      printf("# ID Name rho cp\n");
      FOR_EACH(i, 0, sa_size(stardis->descriptions)) {
        struct description* desc = stardis->descriptions + i;
        struct mat_fluid* fl;
        fl = &desc->d.fluid;
        printf("%u\t%s\t%g\t%g\n", i, fl->name, fl->rho, fl->cp);
      }
    }

    /* List Boundaries */
    if (tbound_count) {
      printf("# T Boundaries\n");
      printf("# ID Name temperature\n");
      FOR_EACH(i, 0, sa_size(stardis->descriptions)) {
        struct description* desc = stardis->descriptions + i;
        struct t_boundary* bd;
        if (desc->type != DESC_BOUND_T_FOR_SOLID
          && desc->type != DESC_BOUND_T_FOR_FLUID) continue;
        bd = &desc->d.t_boundary;
        printf("%u\t%s\t%s\n", i, bd->name, bd->T);
      }
    }
    if (hbound_count) {
      printf("# H Boundaries\n");
      printf("# ID Name emissivity specular_fraction hc hc_max T_env\n");
      FOR_EACH(i, 0, sa_size(stardis->descriptions)) {
        struct description* desc = stardis->descriptions + i;
        struct h_boundary* bd;
        if (desc->type != DESC_BOUND_H_FOR_SOLID
          && desc->type != DESC_BOUND_H_FOR_FLUID) continue;
        bd = &desc->d.h_boundary;
        printf("%u\t%s\t%g\t%g\t%g\t%g\t%s\n",
          i, bd->name, (bd->has_emissivity ? bd->emissivity : 0),
          (bd->has_emissivity ? bd->specular_fraction : 0),
          (bd->has_hc ? bd->hc : 0), (bd->has_hc ? bd->hc_max : 0), bd->T);
      }
    }
    if (fbound_count) {
      printf("# F Boundaries\n");
      printf("# ID Name flux\n");
      FOR_EACH(i, 0, sa_size(stardis->descriptions)) {
        struct description* desc = stardis->descriptions + i;
        struct f_boundary* bd;
        if (desc->type != DESC_BOUND_F_FOR_SOLID) continue;
        bd = &desc->d.f_boundary;
        printf("%u\t%s\t%s\n",
          i, bd->name, bd->flux);
      }
    }

    /* Radiative Temperatures */
    printf("# Radiative Temperatures\n");
    printf("# ID Rad_Temp Lin_Temp\n");
    printf("%u\t%g\t%g\n", (unsigned)sa_size(stardis->descriptions),
      stardis->radiative_temp[0], stardis->radiative_temp[1]);

    printf("# Samples\n");
    printf("# end #power_terms #flux_terms power_term_1 ... power_term_n flux_term_1 ... flux_term_n\n");
    printf("# end = end_type end_id; end_type = T | H | X | R | F | S\n");
    printf("# power_term_i = power_type_i power_id_i factor_i\n");
    printf("# flux_term_i = flux_id_i factor_i\n");

    w_ctx.desc = stardis->descriptions;
    htable_weigth_init(NULL, &w_ctx.weigths);

    res = sdis_green_function_for_each_path(green, print_sample, &w_ctx);
    htable_weigth_release(&w_ctx.weigths);
    if (res != RES_OK) goto error;


    printf("---END GREEN---\n");

    CHK(sdis_green_function_ref_put(green) == RES_OK);
  } else {
    if (mode & IR_COMPUTE) {
      size_t width = (size_t)stardis->camera.img[0];
      size_t height = (size_t)stardis->camera.img[1];

      /* Setup the camera */
      SDIS(camera_create(dev, &cam));
      SDIS(camera_set_proj_ratio(cam, (double)width / (double)height));
      SDIS(camera_set_fov(cam, MDEG2RAD(stardis->camera.fov)));
      SDIS(camera_look_at(cam,
        stardis->camera.pos,
        stardis->camera.tgt,
        stardis->camera.up));

      /* Launch the simulation */
      time[0] = time[1] = stardis->camera.u.time;
      res = sdis_solve_camera(scn,
        cam,
        time,
        stardis->scale_factor,
        stardis->radiative_temp[0],
        stardis->radiative_temp[1],
        width,
        height,
        (size_t)stardis->camera.spp,
        stardis->dump_paths,
        &buf);
      if (res != RES_OK) goto error;

      /* Write the image */
      dump_image(buf);
    }
    else if (mode & PROBE_COMPUTE || mode & PROBE_COMPUTE_ON_INTERFACE) {
      double uv[2] = { 0,0 };
      size_t iprim = SIZE_MAX;
      /* Launch the probe simulation */
      pos[0] = stardis->probe[0];
      pos[1] = stardis->probe[1];
      pos[2] = stardis->probe[2];
      time[0] = time[1] = stardis->probe[3];

      res = select_probe_type(scn,
        stardis->geometry.triangle,
        stardis->geometry.vertex,
        stardis->descriptions,
        (mode & PROBE_COMPUTE_ON_INTERFACE),
        pos,
        &iprim,
        uv);
      if (res != RES_OK) goto error;

      if (iprim == SIZE_MAX) {
        res = sdis_solve_probe(scn,
          stardis->N,
          pos,
          time,
          stardis->scale_factor,
          stardis->radiative_temp[0],
          stardis->radiative_temp[1],
          stardis->dump_paths,
          &estimator);
      } else {
        res = sdis_solve_probe_boundary(scn,
          stardis->N,
          iprim,
          uv,
          time,
          SDIS_FRONT,
          stardis->scale_factor,
          stardis->radiative_temp[0],
          stardis->radiative_temp[1],
          stardis->dump_paths,
          &estimator);
      }
    }
    else if (mode & MEDIUM_COMPUTE) {
      size_t sz, ii;
      struct sdis_medium* medium = NULL;
      /* Find medium */
      sz = sa_size(stardis->descriptions);
      FOR_EACH(ii, 0, sz) {
        struct description* desc = stardis->descriptions + ii;
        if (desc->type == DESC_MAT_SOLID) {
          if (0 == strcmp(stardis->solve_name, desc->d.solid.name)) {
            ASSERT(sa_size(media) > ii);
            medium = media[ii];
            break;
          }
        }
        else if (desc->type == DESC_MAT_FLUID) {
          if (0 == strcmp(stardis->solve_name, desc->d.fluid.name)) {
            ASSERT(sa_size(media) > ii);
            medium = media[ii];
            break;
          }
        }
      }
      if (medium == NULL) {
        /* Not found */
        fprintf(stderr, "Cannot solve medium %s (unknown medium)\n",
          stardis->solve_name);
        res = RES_BAD_ARG;
        goto error;
      }
      time[0] = time[1] = stardis->probe[3];
      res = sdis_solve_medium(scn,
        stardis->N,
        medium,
        time,
        stardis->scale_factor,
        stardis->radiative_temp[0],
        stardis->radiative_temp[1],
        stardis->dump_paths,
        &estimator);
    }
    else if (mode & BOUNDARY_COMPUTE) {
      ASSERT(sa_size(stardis->boundary.primitives)
        == sa_size(stardis->boundary.sides));
      if (sa_size(stardis->boundary.sides) == 0) {
        fprintf(stderr, "Cannot solve boundary %s (empty geometry)\n",
          stardis->solve_name);
      }

      time[0] = time[1] = stardis->probe[3];
      res = sdis_solve_boundary(scn,
        stardis->N,
        stardis->boundary.primitives,
        stardis->boundary.sides,
        sa_size(stardis->boundary.sides),
        time,
        stardis->scale_factor,
        stardis->radiative_temp[0],
        stardis->radiative_temp[1],
        stardis->dump_paths,
        &estimator);
    } else {
      CHK(0);
    }
    if (res != RES_OK) goto error;
    if (mode & PROBE_COMPUTE
      || mode & PROBE_COMPUTE_ON_INTERFACE
      || mode & MEDIUM_COMPUTE
      || mode & BOUNDARY_COMPUTE) {
      /* Fetch the estimation data */
      SDIS(estimator_get_temperature(estimator, &temperature));
      SDIS(estimator_get_failure_count(estimator, &nfailures));

      /* Print the results */
      switch (mode & COMPUTE_MODES) {
      case PROBE_COMPUTE:
      case PROBE_COMPUTE_ON_INTERFACE:
        printf("Temperature at t=[%g, %g, %g, %g] = %g +/- %g\n",
          pos[0], pos[1], pos[2], time[0],
          temperature.E, /* Expected value */
          temperature.SE); /* Standard error */
        break;
      case MEDIUM_COMPUTE:
        printf("Temperature in medium %s at t=%g = %g +/- %g\n",
          stardis->solve_name, time[0],
          temperature.E, /* Expected value */
          temperature.SE); /* Standard error */
        break;
      case BOUNDARY_COMPUTE:
        printf("Temperature at boundary %s at t=%g = %g +/- %g\n",
          stardis->solve_name, time[0],
          temperature.E, /* Expected value */
          temperature.SE); /* Standard error */
        break;
      default: FATAL("Invalid mode.");
      }
      printf("#failures: %lu/%lu\n",
        (unsigned long)nfailures,
        (unsigned long)stardis->N);

      /* Dump paths according to user settings */
      sdis_estimator_for_each_path(estimator, dump_path, stdout);
    }
  }

end:
  if (data) SDIS(data_ref_put(data));
  if (interfaces) {
    for (i = 0; i < sa_size(interfaces); ++i)
      SDIS(interface_ref_put(interfaces[i]));
    sa_release(interfaces);
    stardis->geometry.interfaces = NULL;
  }
  sa_release(interf_bytrg);
  stardis->geometry.interf_bytrg = NULL;
  if (htable_interfaces_initialised) htable_intface_release(&htable_interfaces);
  for (i = 0; i < sa_size(media); ++i)
    SDIS(medium_ref_put(media[i]));
  sa_release(media);

  if (cam) SDIS(camera_ref_put(cam));
  if (buf) SDIS(estimator_buffer_ref_put(buf));
  if (estimator) SDIS(estimator_ref_put(estimator));
  if (scn) SDIS(scene_ref_put(scn));
  if (dev) SDIS(device_ref_put(dev));

  return res;
error:
  goto end;
}
