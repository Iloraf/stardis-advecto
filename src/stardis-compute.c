/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#include "stardis-app.h"
#include <sdis.h>
#include <sdis_version.h>
#include <rsys/double3.h>
#include <rsys/dynamic_array_uint.h>

#include <rsys/image.h>
#include<star/senc.h>

#include <rsys/hash_table.h>
#define HTABLE_NAME vrtx_rank
#define HTABLE_DATA unsigned
#define HTABLE_KEY unsigned
#include <rsys/hash_table.h>

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
   const int t_allowed,
   int* is_zero)
{
  te_variable vars[4] = {
    TE_DEF_OFFSET("x", offsetof(struct sdis_rwalk_vertex, P[0])),
    TE_DEF_OFFSET("y", offsetof(struct sdis_rwalk_vertex, P[1])),
    TE_DEF_OFFSET("z", offsetof(struct sdis_rwalk_vertex, P[2])),
    TE_DEF_OFFSET("t", offsetof(struct sdis_rwalk_vertex, time))
  };

  ASSERT(math_expr);
  *f = te_compile(math_expr, vars, t_allowed ? 4 : 3, NULL);
  if (!*f) {
    if (!t_allowed && te_compile(math_expr, vars, 4, NULL))
      printf("Expression use variable t and should not.\n");
    return RES_BAD_ARG;
  }
  if (is_zero) *is_zero = !((*f)->type == TE_CONSTANT && (*f)->v.value == 0);
  return RES_OK;
}

/*******************************************************************************
 * Fluid data
 ******************************************************************************/
struct fluid {
  double cp; /* Calorific capacity */
  double rho; /* Volumic mass */
  /* TinyExpr stuff to compute temperature */
  struct te_expr *temperature;
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
  return te_eval(fluid_props->temperature, vtx);
}

static double
fluid_get_tinit
(const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  if(vtx->time > 0) {
    return -1;
  }
  return te_eval(fluid_props->temperature, vtx);
}

static void
release_fluid_data(void* f)
{
  struct fluid* fluid = (struct fluid*)f;
  te_free(fluid->temperature);
}

/*******************************************************************************
 * Solid data
 ******************************************************************************/
struct solid {
  double cp; /* Calorific capacity */
  double lambda; /* Conductivity */
  double rho; /* Volumic mass */
  double delta; /* Numerical parameter */
  /* TinyExpr stuff to compute temperature & power */
  struct te_expr *t_init;
  struct te_expr *power;
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
solid_dont_get_temperature
(const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  (void)vtx; (void)data;
  ASSERT(0);
  return 0;
}

static double
solid_get_temperature
(const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return te_eval(solid_props->t_init, vtx);
}

static double
solid_get_tinit
(const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  if (vtx->time > 0) {
    return -1;
  }
  return te_eval(solid_props->t_init, vtx);
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
   const struct description* descriptions,
   double pos[3], 
   size_t* iprim, 
   double uv[2])
{
  res_T res = RES_OK;
  size_t i = 0;

  for (i = 0; i < sa_size(triangle); ++i) {
    double delta = 0, dp[3], projected_pos[3];
    res = sdis_scene_boundary_project_position(scn, i, pos, uv);
    if (res != RES_OK) return res;
    res = sdis_scene_get_boundary_position(scn, i, uv, projected_pos);
    if (res != RES_OK) return res;
    d3_sub(dp, pos, projected_pos);

    if (triangle[i].front_description != UINT_MAX) {
      const struct description *desc = descriptions + triangle[i].front_description;
      if (desc->type == DESC_MAT_SOLID) delta = MMAX(desc->d.solid.delta, delta);
    }
    if (triangle[i].back_description != UINT_MAX) {
      const struct description *desc = descriptions + triangle[i].back_description;
      if (desc->type == DESC_MAT_SOLID) delta = MMAX(desc->d.solid.delta, delta);
    }

    if (d3_len(dp) < delta){
      *iprim = i;
      fprintf(stderr,"The probe is very close to the primitive %llu.\n",
        (long long int)*iprim );
      fprintf(stderr,"So the probe is moved to this primitive at the position: %g %g %g\n",
          SPLIT3(projected_pos));
      d3_set(pos, projected_pos);
      break;
    }
  }
  return res;
}

static void
dump_image(const struct sdis_accum_buffer* buf)
{
  struct sdis_accum_buffer_layout layout = SDIS_ACCUM_BUFFER_LAYOUT_NULL;
  double* temps = NULL;
  const struct sdis_accum* accums = NULL;
  size_t ix, iy;

  CHK(buf != NULL);
  CHK(sdis_accum_buffer_get_layout(buf, &layout) == RES_OK);

  temps = mem_alloc(layout.width*layout.height*sizeof(double));
  CHK(temps != NULL);

  /* Compute the per pixel temperature */
  CHK(sdis_accum_buffer_map(buf, &accums) == RES_OK);
  fprintf(stdout,"# vtk DataFile Version 2.0\nvtk output\nASCII\nDATASET STRUCTURED_POINTS\n");
  fprintf(stdout,"DIMENSIONS %i %i 1\n", (int)layout.width, (int)layout.height);
  fprintf(stdout,"ORIGIN 0 0 0\n");
  fprintf(stdout,"SPACING 1 1 1\n");
  fprintf(stdout,"POINT_DATA %i\n", (int)layout.width*(int)layout.height);
  fprintf(stdout,"SCALARS temperature float 1\n");
  fprintf(stdout,"LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, layout.height) {
    const struct sdis_accum* row_accums = accums + (layout.height - 1 - iy) * layout.width;
    double* row = temps + iy * layout.width;
    FOR_EACH(ix, 0, layout.width) {
      row[ix] = row_accums[ix].sum_weights / (double)row_accums[ix].nweights;
      fprintf(stdout,"%f\n",row[ix]);
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
   const double rho,
   const double cp,
   const int t_allowed,
   const char* t_expr,
   struct sdis_medium*** media_ptr,
   sdis_medium_getter_T get_temp,
   unsigned* out_id)
{
  struct sdis_fluid_shader fluid_shader = SDIS_FLUID_SHADER_NULL;
  struct sdis_data* data = NULL;
  struct fluid* fluid_props;
  size_t sz;
  res_T res = RES_OK;

  ASSERT(dev && rho >= 0 && cp >= 0 && t_expr && media_ptr && out_id);
  fluid_shader.calorific_capacity = fluid_get_calorific_capacity;
  fluid_shader.volumic_mass = fluid_get_volumic_mass;
  fluid_shader.temperature = get_temp;
  res = sdis_data_create(dev, sizeof(struct fluid), ALIGNOF(struct fluid),
    release_fluid_data, &data);
  if (res != RES_OK) goto error;
  fluid_props = sdis_data_get(data); /* Fetch the allocated memory space */
  fluid_props->cp = cp;
  fluid_props->rho = rho;
  res = compile_expr_to_fn(&fluid_props->temperature, t_expr, t_allowed, NULL);
  if (res != RES_OK) {
    fprintf(stderr, "Invalid initial temperature expression: %s\n",
      t_expr);
    goto error;
  }
  res = sdis_fluid_create(dev, &fluid_shader, data, sa_add(*media_ptr, 1));
  if (res != RES_OK) goto error;
  sz = sa_size(*media_ptr) - 1;
  ASSERT(sz < INT_MAX);
  *out_id = (unsigned)sz;

end:
  if (data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}

static res_T
create_solid
  (struct sdis_device* dev,
   const double lambda,
   const double rho,
   const double cp,
   const double delta,
   const int t_allowed,
   const char* t_expr, /* Can be NULL if not used by getter */
   const char* power_expr, /* Can be NULL */
   struct sdis_medium*** media_ptr,
   sdis_medium_getter_T get_temp,
   int* out_has_power, /* Can be NULL */
   unsigned* out_id)
{
  struct sdis_solid_shader solid_shader = SDIS_SOLID_SHADER_NULL;
  struct sdis_data* data = NULL;
  struct solid* solid_props;
  size_t sz;
  res_T res = RES_OK;

  ASSERT(dev && lambda >= 0 && rho >= 0 && cp >= 0 && delta > 0
    && media_ptr && get_temp && out_id);
  solid_shader.calorific_capacity = solid_get_calorific_capacity;
  solid_shader.thermal_conductivity = solid_get_thermal_conductivity;
  solid_shader.volumic_mass = solid_get_volumic_mass;
  solid_shader.delta_solid = solid_get_delta;
#if Stardis_VERSION_MINOR == 3
  solid_shader.delta_boundary = solid_get_delta_boundary;
#endif
  solid_shader.temperature = get_temp;
  res = sdis_data_create(dev, sizeof(struct solid), ALIGNOF(struct solid),
    release_solid_data, &data);
  if (res != RES_OK) goto error;

  solid_props = sdis_data_get(data); /* Fetch the allocated memory space */
  solid_props->lambda = lambda;
  solid_props->rho = rho;
  solid_props->cp = cp;
  solid_props->delta = delta;
  solid_props->t_init = NULL;
  solid_props->power = NULL;
  if (t_expr) {
    res = compile_expr_to_fn(&solid_props->t_init, t_expr, t_allowed, NULL);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid initial temperature expression: %s\n",
        t_expr);
      goto error;
    }
  }
  if (power_expr) {
    int has_power;
    res = compile_expr_to_fn(&solid_props->power, power_expr, 1, &has_power);
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
  sz = sa_size(*media_ptr) - 1;
  ASSERT(sz < INT_MAX);
  *out_id = (unsigned)sz;

end:
  if (data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}
res_T
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

  res = sdis_get_scene_analyze(scn, &analyze);
  if (res != RES_OK) goto error;
  res = sdis_release_scene_analyze(scn);
  if (res != RES_OK) goto error;
  
  ASSERT(scn && allocator);

  res = senc_descriptor_get_frontier_segments_count(analyze, &scount);
  if (res != RES_OK) goto error;

  if(scount == 0)
    /* No edge to exit */
    return RES_OK;

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
  struct sdis_accum_buffer* buf = NULL;
  struct sdis_camera* cam = NULL;

  struct htable_intface htable_interfaces;
  int htable_interfaces_initialised = 0;
  double pos[3] = {0,0,0};
  double time[2] = { 0, 0};
  size_t nfailures;
  unsigned i = 0;
  const int t_allowed = stardis->probe[3] < INF; /* Can condition use the variable t? */

  res = sdis_device_create(NULL, &stardis->allocator, stardis->nthreads, 1, &dev);
  if (res != RES_OK) goto error;

  /* Create media and property holders found in descriptions */
  for (i = 0; i < sa_size(stardis->descriptions); ++i) {
    struct description* desc = stardis->descriptions + i;

    switch (desc->type) {
    case DESC_BOUND_H_FOR_SOLID:
      /* Create an external fluid */
      res = create_fluid(dev, 1, 1, t_allowed, desc->d.h_boundary.T, &media,
        fluid_get_temperature, &desc->d.h_boundary.mat_id);
      if (res != RES_OK) goto error;
      break;
    case DESC_BOUND_H_FOR_FLUID:
      /* Create an external solid */
      res = create_solid(dev, INF, 1, 1, 1, t_allowed, desc->d.h_boundary.T,
        NULL, &media, solid_get_temperature, NULL, &desc->d.h_boundary.mat_id);
      if (res != RES_OK) goto error;
      break;
    case DESC_BOUND_T_FOR_SOLID:
    case DESC_BOUND_T_FOR_FLUID:
      ASSERT(desc->d.t_boundary.T != NULL);
      res = compile_expr_to_fn(&desc->d.t_boundary.te_temperature, 
        desc->d.t_boundary.T, 1, NULL);
      if (res != RES_OK) {
        fprintf(stderr, "Invalid boundary temperature expression: %s\n",
          desc->d.h_boundary.T);
        goto error;
      }
      /* Create an external solid */
      res = create_solid(dev, 1, 1, 1, 1, t_allowed, NULL, NULL, &media,
        solid_dont_get_temperature, NULL, &desc->d.t_boundary.mat_id);
      if (res != RES_OK) goto error;
      break;
    case DESC_BOUND_F_FOR_SOLID:
      ASSERT(desc->d.f_boundary.flux != NULL);
      res = compile_expr_to_fn(&desc->d.f_boundary.te_flux,
        desc->d.f_boundary.flux, 1, NULL);
      if (res != RES_OK) {
        fprintf(stderr, "Invalid boundary flux expression: %s\n",
          desc->d.h_boundary.T);
        goto error;
      }
      /* Create an external solid */
      res = create_solid(dev, 1, 1, 1, 1, t_allowed, NULL, NULL, &media, NULL,
        NULL, &desc->d.f_boundary.mat_id);
      if (res != RES_OK) goto error;
      break;
    case DESC_SOLID_FLUID_CONNECT:
      ASSERT(desc->d.sf_connect.specular_fraction >= 0
        && desc->d.sf_connect.specular_fraction <= 1);
      ASSERT(desc->d.sf_connect.emissivity >= 0);
      ASSERT(desc->d.sf_connect.hc >= 0);
      break;
    case DESC_MAT_SOLID:
      res = create_solid(dev,
        stardis->descriptions[i].d.solid.lambda,
        stardis->descriptions[i].d.solid.rho,
        stardis->descriptions[i].d.solid.cp,
        stardis->descriptions[i].d.solid.delta,
        t_allowed,
        stardis->descriptions[i].d.solid.Tinit,
        stardis->descriptions[i].d.solid.power,
        &media,
        solid_get_tinit, 
        &stardis->descriptions[i].d.solid.has_power,
        &desc->d.solid.solid_id);
      if (res != RES_OK) goto error;
      break;
    case DESC_MAT_FLUID:
      res = create_fluid(dev,
        stardis->descriptions[i].d.fluid.rho, 
        stardis->descriptions[i].d.fluid.cp,
        t_allowed,
        stardis->descriptions[i].d.fluid.Tinit,
        &media,
        fluid_get_tinit,
        &desc->d.fluid.fluid_id);
      if (res != RES_OK) goto error;
      break;
    default: FATAL("Invalid type.\n");
    }
  }

  /* Create interfaces */
  htable_intface_init(&stardis->allocator, &htable_interfaces);
  htable_interfaces_initialised = 1;
  for (i=0; i<sa_size(stardis->geometry.triangle); ++i) {
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
    }
    else {
      /* create new interface and register */
      struct sdis_interface_shader interface_shader = SDIS_INTERFACE_SHADER_NULL;
      struct intface* interface_props = NULL;
      int front_defined = (fd != UINT_MAX);
      int back_defined = (bd != UINT_MAX);
      int connect_defined = (cd != UINT_MAX);

      SDIS(data_create(dev, sizeof(struct intface), ALIGNOF(struct intface),
        release_interface_data, &data));
      interface_props = sdis_data_get(data);
      interface_props->temperature = NULL;
      interface_props->flux = NULL;

      if (front_defined) {
        switch (stardis->descriptions[fd].type) {
        case DESC_MAT_SOLID:
          solid_count++;
          front_med = media[fd];
          break;
        case DESC_MAT_FLUID:
          fluid_count++;
          front_med = media[fd];
          fluid_side_shader = &interface_shader.front;
          break;
        default: FATAL("Invalid type.\n");
        }
      }
      if (back_defined) {
        switch (stardis->descriptions[bd].type) {
        case DESC_MAT_SOLID:
          solid_count++;
          back_med = media[bd];
          break;
        case DESC_MAT_FLUID:
          fluid_count++;
           back_med = media[bd];
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
          ASSERT(sdis_medium_get_type(media[ext_id]) == SDIS_SOLID);
          connection_count++;
          boundary_count++;
          if (front_defined) {
            back_med = media[ext_id];
            /* We set the known T inside
             * TODO: should be outside to allow contact resistances */
            interface_shader.front.temperature = interface_get_temperature;
          }
          else {
            front_med = media[ext_id];
            /* We set the known T inside
             * TODO: should be outside to allow contact resistances */
            interface_shader.back.temperature = interface_get_temperature;
          }
          ASSERT(connect->d.t_boundary.te_temperature);
          interface_props->temperature = connect->d.t_boundary.te_temperature;
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
          /* Reuse same solid for the external side */
          if (front_defined) {
            back_med = front_med;
            interface_shader.front.flux = interface_get_flux;
          } else {
            front_med = back_med;
            interface_shader.back.flux = interface_get_flux;
          }
          ASSERT(connect->d.f_boundary.te_flux);
          interface_props->flux = connect->d.f_boundary.te_flux;
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

      if((fluid_count == 2)
        || (fluid_count + solid_count + connection_count < 2)
        || (boundary_count ?
             (fluid_count + solid_count != 1) : (fluid_count + solid_count != 2))
        || (solid_fluid_connection_count && (fluid_count != 1 || solid_count != 1)))
      {
        /* Incoherent triangle description */
        fprintf(stderr, "Incoherent triangle description (%u)\n", i);
        print_trg_as_obj(stderr, stardis->geometry.vertex, trg);
        fprintf(stderr, "Front: ");
        if(!front_defined) fprintf(stderr, "undefined\n");
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
      if (res != RES_OK) {
        fprintf(stderr,
          "Cannot create interface associated to triangle %u\n", i);
        goto error;
      }
      SDIS(data_ref_put(data)); data = NULL;
      res = htable_intface_set(&htable_interfaces, &int_descs, &intface);
      if (res != RES_OK) goto error;
      sa_push(interfaces, intface);
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

  if (mode == IR_COMPUTE){
   size_t width = (size_t)stardis->camera.img[0];
   size_t height = (size_t)stardis->camera.img[1];

    /* Setup the camera */
    SDIS(camera_create(dev, &cam));
    SDIS(camera_set_proj_ratio(cam, (double)width/(double)height));
    SDIS(camera_set_fov(cam, MDEG2RAD(stardis->camera.fov)));
    SDIS(camera_look_at(cam, 
          stardis->camera.pos,
          stardis->camera.tgt,
          stardis->camera.up));

    /* Create the accum buffer */
    SDIS(accum_buffer_create(dev,
          width,
          height,
          &buf));

    /* Launch the simulation */
    SDIS(solve_camera(scn, 
          cam,
          INF,
          stardis->scale_factor,
          stardis->radiative_temp[0], 
          stardis->radiative_temp[1], 
          width,
          height,
          (size_t)stardis->camera.spp,
          sdis_accum_buffer_write,
          buf));

    /* Write the image */
    dump_image(buf);
  }

  if (mode == PROBE_COMPUTE){
    /* Launch the probe simulation */
    pos[0] = stardis->probe[0];
    pos[1] = stardis->probe[1];
    pos[2] = stardis->probe[2];
    time[0] = time[1]  = stardis->probe[3];
    {
      double uv[2] = {0,0};
      size_t iprim = SIZE_MAX;

      res = select_probe_type(scn,
          stardis->geometry.triangle,
          stardis->descriptions,
          pos,
          &iprim,
          uv);

      if(iprim == SIZE_MAX){
        SDIS(solve_probe(scn,
              stardis->N, 
              pos,
              time, 
              stardis->scale_factor,
              stardis->radiative_temp[0], 
              stardis->radiative_temp[1], 
              &estimator));
      } else {
        SDIS(solve_probe_boundary(scn,
              stardis->N,
              iprim,
              uv,
              time,
              SDIS_FRONT,
              stardis->scale_factor,
              stardis->radiative_temp[0], 
              stardis->radiative_temp[1], 
              &estimator));
      }
    }

    /* Fetch the estimation data */
    SDIS(estimator_get_temperature(estimator, &temperature));
    SDIS(estimator_get_failure_count(estimator, &nfailures));

    /* Print the results */
    printf("Temperature at [%g, %g, %g, %g] = %g +/- %g\n",
        pos[0], pos[1], pos[2], time[0],
        temperature.E, /* Expected value */
        temperature.SE); /* Standard error */
    printf("#failures: %lu/%lu\n", 
        (unsigned long)nfailures,
        (unsigned long)stardis->N);
  }

end:
  if (data) SDIS(data_ref_put(data));;
  if(htable_interfaces_initialised) htable_intface_release(&htable_interfaces);
  for (i=0; i<sa_size(media); ++i)
    SDIS(medium_ref_put(media[i]));
  sa_release(media);

  if (cam) SDIS(camera_ref_put(cam));
  if (buf) SDIS(accum_buffer_ref_put(buf));
  if (estimator) SDIS(estimator_ref_put(estimator));
  if (scn) SDIS(scene_ref_put(scn));
  if (dev) SDIS(device_ref_put(dev));

  return res;
error:
  goto end;
}
