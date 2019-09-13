#include "stardis-output.h"
#include "stardis-compute.h"
#include "stardis-fluid.h"
#include "stardis-solid.h"
#include "stardis-intface.h"
#include "stardis-app.h"

#include <sdis.h>

#include<star/senc.h>

#include <rsys/math.h>
#include <rsys/mem_allocator.h>
#include <rsys/dynamic_array_uint.h>

#include <stdio.h>

struct w_ctx {
  const struct description* desc;
  struct htable_weigth weigths;
};

#include <rsys/hash_table.h>
#define HTABLE_NAME vrtx_rank
#define HTABLE_DATA unsigned
#define HTABLE_KEY unsigned
#include <rsys/hash_table.h>

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

static res_T
print_power_term
  (struct sdis_medium* mdm,
   const double power_term,
   void* ctx)
{
  res_T res = RES_OK;
  struct sdis_data* data = NULL;
  enum sdis_medium_type type;
  unsigned id;
  ASSERT(mdm && ctx); (void)ctx;

  data = sdis_medium_get_data(mdm);
  type = sdis_medium_get_type(mdm);

  switch (type) {
  case SDIS_FLUID: {
    FATAL("Unexpected power term in fluid");
  }
  case SDIS_SOLID: {
    struct solid* d__ = sdis_data_get(data);
    id = d__->id;
    ASSERT(id == ((struct w_ctx*)ctx)->desc[id].d.solid.solid_id);
    printf("\tS\t%u\t%g", id, power_term);
    break;
  }
  default: FATAL("Unreachable code.\n"); break;
  }
  return res;
}

static res_T
get_flux_terms
  (struct sdis_interface* interf,
   const enum sdis_side side,
   const double flux_term,
   void* ctx)
{
  res_T res = RES_OK;
  struct sdis_data* data = NULL;
  struct intface* d__;
  unsigned id;
  struct w_ctx* w_ctx = ctx;

  ASSERT(interf && ctx);

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
dump_path
  (const struct sdis_heat_path* path,
   void* context)
{
  res_T res = RES_OK;
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
  fprintf(stream, "POINTS %zu double\n", vcount);
  FOR_EACH(i, 0, vcount) {
    struct sdis_heat_vertex vtx;
    res = sdis_heat_path_get_vertex(path, i, &vtx);
    if (res != RES_OK) goto error;
    fprintf(stream, "%g %g %g\n", SPLIT3(vtx.P));
  }
  /* Write the segment of the path */
  fprintf(stream, "LINES %u %zu\n", 1u, 1 + vcount);
  fprintf(stream, "%zu", vcount);
  FOR_EACH(i, 0, vcount) fprintf(stream, " %zu", i);
  fprintf(stream, "\n");
  fprintf(stream, "POINT_DATA %zu\n", vcount);
  /* Write the type of the random walk vertices */
  fprintf(stream, "SCALARS Vertex_Type float 1\n");
  fprintf(stream, "LOOKUP_TABLE vertex_type\n");
  FOR_EACH(i, 0, vcount) {
    struct sdis_heat_vertex vtx;
    res = sdis_heat_path_get_vertex(path, i, &vtx);
    if (res != RES_OK) goto error;
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
    res = sdis_heat_path_get_vertex(path, i, &vtx);
    if (res != RES_OK) goto error;
    fprintf(stream, "%g\n", vtx.weight);
  }
  /* Write the time of the random walk vertices */
  fprintf(stream, "SCALARS Time double 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(i, 0, vcount) {
    struct sdis_heat_vertex vtx;
    res = sdis_heat_path_get_vertex(path, i, &vtx);
    if (res != RES_OK) goto error;
    fprintf(stream, "%g\n", IS_INF(vtx.time) ? FLT_MAX : vtx.time);
  }
  /* Write path type */
  fprintf(stream, "CELL_DATA %u\n", 1u);
  fprintf(stream, "SCALARS Path_Type float 1\n");
  fprintf(stream, "LOOKUP_TABLE path_type\n");
  res = sdis_heat_path_get_status(path, &status);
  if (res != RES_OK) goto error;
  switch (status) {
  case SDIS_HEAT_PATH_SUCCEED: fprintf(stream, "0.0\n"); break;
  case SDIS_HEAT_PATH_FAILED: fprintf(stream, "1.0\n"); break;
  default: FATAL("Unreachable code.\n"); break;
  }
  fprintf(stream, "LOOKUP_TABLE path_type 2\n");
  fprintf(stream, "0.0 0.0 1.0 1.0\n"); /* 0.0 = Blue: success */
  fprintf(stream, "1.0 0.0 0.0 1.0\n"); /* 1.0 = Red: failure */

end:
  return res;
error:
  goto end;
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
print_sample
  (struct sdis_green_path* path,
   void* ctx)
{
  res_T res = RES_OK;
  struct sdis_point pt = SDIS_POINT_NULL;
  struct sdis_data* data = NULL;
  enum sdis_medium_type type;
  struct htable_weigth_iterator it, end;
  unsigned id;
  size_t pcount, fcount;
  struct w_ctx* w_ctx = ctx;
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

end:
  return res;
error:
  goto end;
}

res_T
dump_image
  (const struct sdis_estimator_buffer* buf)
{
  res_T res = RES_OK;
  size_t definition[2];
  double* temps = NULL;
  size_t ix, iy;

  CHK(buf != NULL);
  res = sdis_estimator_buffer_get_definition(buf, definition);
  if (res != RES_OK) goto error;

  temps = mem_alloc(definition[0] * definition[1] * sizeof(double));
  if (temps == NULL) {
    res = RES_MEM_ERR;
    goto error;
  }

  /* Compute the per pixel temperature */
  fprintf(stdout, "# vtk DataFile Version 2.0\nvtk output\nASCII\nDATASET STRUCTURED_POINTS\n");
  fprintf(stdout, "DIMENSIONS %zu %zu 1\n", definition[0], definition[1]);
  fprintf(stdout, "ORIGIN 0 0 0\n");
  fprintf(stdout, "SPACING 1 1 1\n");
  fprintf(stdout, "POINT_DATA %zu\n", definition[0] * definition[1]);
  fprintf(stdout, "SCALARS temperature_estimate float 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc T;
      res = sdis_estimator_buffer_at(buf, ix, iy, &estimator);
      if (res != RES_OK) goto error;
      res = sdis_estimator_get_temperature(estimator, &T);
      if (res != RES_OK) goto error;
      fprintf(stdout, "%f\n", T.E);
    }
  }
  fprintf(stdout, "SCALARS temperature_std_dev float 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc T;
      res = sdis_estimator_buffer_at(buf, ix, iy, &estimator);
      if (res != RES_OK) goto error;
      res = sdis_estimator_get_temperature(estimator, &T);
      if (res != RES_OK) goto error;
      fprintf(stdout, "%f\n", T.SE);
    }
  }
  fprintf(stdout, "SCALARS computation_time float 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc time;
      res = sdis_estimator_buffer_at(buf, ix, iy, &estimator);
      if (res != RES_OK) goto error;
      res = sdis_estimator_get_realisation_time(estimator, &time);
      if (res != RES_OK) goto error;
      fprintf(stdout, "%f\n", time.E);
    }
  }
  fprintf(stdout, "SCALARS computation_time_std_dev float 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc time;
      res = sdis_estimator_buffer_at(buf, ix, iy, &estimator);
      if (res != RES_OK) goto error;
      res = sdis_estimator_get_realisation_time(estimator, &time);
      if (res != RES_OK) goto error;
      fprintf(stdout, "%f\n", time.SE);
    }
  }
  fprintf(stdout, "SCALARS temperature_failures_count unsigned_long_long 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      size_t nfails;
      res = sdis_estimator_buffer_at(buf, ix, iy, &estimator);
      if (res != RES_OK) goto error;
      res = sdis_estimator_get_failure_count(estimator, &nfails);
      if (res != RES_OK) goto error;
      fprintf(stdout, "%zu\n", nfails);
    }
  }
  mem_rm(temps);

end:
  return res;
error:
  goto end;
}

res_T
dump_green
  (struct sdis_green_function* green,
   const struct counts* counts,
   const struct description* descriptions,
   const double radiative_temps[2])
{
  res_T res = RES_OK;
  size_t ok_count, failed_count;
  struct w_ctx w_ctx;
  unsigned i;

  ASSERT(green && counts && descriptions && radiative_temps);

  res = sdis_green_function_get_invalid_paths_count(green, &failed_count);
  if (res != RES_OK) goto error;

  res = sdis_green_function_get_paths_count(green, &ok_count);
  if (res != RES_OK) goto error;

  /* Output counts */
  printf("---BEGIN GREEN---\n");
  printf("# #solids #fluids #t_boundaries #h_boundaries #f_boundaries #ok #failures\n");
  printf("%zu %zu %zu %zu %zu %zu %zu\n",
    counts->smed_count, counts->fmed_count,
    counts->tbound_count, counts->hbound_count, counts->fbound_count,
    ok_count, failed_count);

  /* List Media */
  if (counts->smed_count) {
    printf("# Solids\n");
    printf("# ID Name lambda rho cp power\n");
    FOR_EACH(i, 0, (unsigned)sa_size(descriptions)) {
      const struct description* desc = descriptions + i;
      const struct mat_solid* sl;
      if (desc->type != DESC_MAT_SOLID) continue;
      sl = &desc->d.solid;
      printf("%u\t%s\t%g\t%g\t%g\t%s\n",
        i, sl->name, sl->lambda, sl->rho, sl->cp, (sl->has_power ? sl->power : "0"));
    }
  }
  if (counts->fmed_count) {
    printf("# Fluids\n");
    printf("# ID Name rho cp\n");
    FOR_EACH(i, 0, sa_size(descriptions)) {
      const struct description* desc = descriptions + i;
      const struct mat_fluid* fl;
      fl = &desc->d.fluid;
      printf("%u\t%s\t%g\t%g\n", i, fl->name, fl->rho, fl->cp);
    }
  }

  /* List Boundaries */
  if (counts->tbound_count) {
    printf("# T Boundaries\n");
    printf("# ID Name temperature\n");
    FOR_EACH(i, 0, sa_size(descriptions)) {
      const struct description* desc = descriptions + i;
      const struct t_boundary* bd;
      if (desc->type != DESC_BOUND_T_FOR_SOLID
        && desc->type != DESC_BOUND_T_FOR_FLUID) continue;
      bd = &desc->d.t_boundary;
      printf("%u\t%s\t%s\n", i, bd->name, bd->T);
    }
  }
  if (counts->hbound_count) {
    printf("# H Boundaries\n");
    printf("# ID Name emissivity specular_fraction hc hc_max T_env\n");
    FOR_EACH(i, 0, sa_size(descriptions)) {
      const struct description* desc = descriptions + i;
      const struct h_boundary* bd;
      if (desc->type != DESC_BOUND_H_FOR_SOLID
        && desc->type != DESC_BOUND_H_FOR_FLUID) continue;
      bd = &desc->d.h_boundary;
      printf("%u\t%s\t%g\t%g\t%g\t%g\t%s\n",
        i, bd->name, (bd->has_emissivity ? bd->emissivity : 0),
        (bd->has_emissivity ? bd->specular_fraction : 0),
        (bd->has_hc ? bd->hc : 0), (bd->has_hc ? bd->hc_max : 0), bd->T);
    }
  }
  if (counts->fbound_count) {
    printf("# F Boundaries\n");
    printf("# ID Name flux\n");
    FOR_EACH(i, 0, sa_size(descriptions)) {
      const struct description* desc = descriptions + i;
      const struct f_boundary* bd;
      if (desc->type != DESC_BOUND_F_FOR_SOLID) continue;
      bd = &desc->d.f_boundary;
      printf("%u\t%s\t%s\n",
        i, bd->name, bd->flux);
    }
  }

  /* Radiative Temperatures */
  printf("# Radiative Temperatures\n");
  printf("# ID Rad_Temp Lin_Temp\n");
  printf("%u\t%g\t%g\n", (unsigned)sa_size(descriptions),
    radiative_temps[0], radiative_temps[1]);

  printf("# Samples\n");
  printf("# end #power_terms #flux_terms power_term_1 ... power_term_n flux_term_1 ... flux_term_n\n");
  printf("# end = end_type end_id; end_type = T | H | X | R | F | S\n");
  printf("# power_term_i = power_type_i power_id_i factor_i\n");
  printf("# flux_term_i = flux_id_i factor_i\n");

  w_ctx.desc = descriptions;
  htable_weigth_init(NULL, &w_ctx.weigths);

  res = sdis_green_function_for_each_path(green, print_sample, &w_ctx);
  htable_weigth_release(&w_ctx.weigths);
  if (res != RES_OK) goto error;


  printf("---END GREEN---\n");

  CHK(sdis_green_function_ref_put(green) == RES_OK);

end:
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

  res = sdis_scene_get_analysis(scn, &analyze);
  if (res != RES_OK) goto error;
  res = sdis_scene_release_analysis(scn);
  if (res != RES_OK) goto error;

  ASSERT(scn && allocator);

  res = senc_descriptor_get_frontier_segments_count(analyze, &scount);
  if (res != RES_OK) goto error;

  if (scount == 0)
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
    if (res != RES_OK) goto error;
    FOR_EACH(v, 0, 2) {
      unsigned* p_rank;
      p_rank = htable_vrtx_rank_find(&ranks, edge + v);
      if (p_rank) {
        rk[v] = *p_rank;
      } else {
        /* First appearance: allocate a rank and exit it */
        unsigned rank;
        size_t tmp = htable_vrtx_rank_size_get(&ranks);
        ASSERT(tmp < UINT_MAX);
        rank = (unsigned)tmp;
        res = htable_vrtx_rank_set(&ranks, edge + v, &rank);
        if (res != RES_OK) goto error;
        rk[v] = rank;
        darray_uint_push_back(&comps, &uimax);
        darray_uint_push_back(&edges, edge + v);
        vcount++;
      }
      /* Manage components */
      cc = darray_uint_data_get(&comps);
      if (cc[rk[0]] != UINT_MAX || cc[rk[1]] != UINT_MAX) {
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
    if (!obj) goto error;
    /* Exit all vertices even unused
     * (same numbering scheme for all components) */
    FOR_EACH(v, 0, vcount) {
      double coord[3];
      res = senc_descriptor_get_global_vertex(analyze, ee[v], coord);
      if (res != RES_OK) goto error;
      fprintf(obj, "v %f %f %f\n", SPLIT3(coord));
    }
    FOR_EACH(i, 0, scount) {
      unsigned edge[2], new_ranks[2];
      res = senc_descriptor_get_frontier_segment(analyze, i, edge);
      if (res != RES_OK) goto error;
      ASSERT(cc[edge[0]] == cc[edge[1]]);
      if (cc[edge[0]] != comp)
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
  if (obj) fclose(obj);
  if (analyze) senc_descriptor_ref_put(analyze);
  if (structs_initialized) {
    htable_vrtx_rank_release(&ranks);
    darray_uint_release(&comps);
    darray_uint_release(&edges);
  }
  return res;
error:
  goto end;
}

res_T
print_single_MC_result
  (const int mode,
   struct sdis_estimator* estimator,
   struct stardis* stardis)
{
  res_T res = RES_OK;
  struct sdis_mc result;
  size_t nfailures;

  ASSERT(estimator && stardis);

  /* Fetch the estimation data */
  res = sdis_estimator_get_temperature(estimator, &result);
  if (res != RES_OK) goto error;
  res = sdis_estimator_get_failure_count(estimator, &nfailures);
  if (res != RES_OK) goto error;

  /* Print the results */
  switch (mode & COMPUTE_MODES) {
  case PROBE_COMPUTE:
  case PROBE_COMPUTE_ON_INTERFACE:
    printf("Temperature at [%g, %g, %g], t=%g = %g +/- %g\n",
      SPLIT4(stardis->probe),
      result.E, /* Expected value */
      result.SE); /* Standard error */
    break;
  case MEDIUM_COMPUTE:
    printf("Temperature in medium %s at t=%g = %g +/- %g\n",
      stardis->solve_name, stardis->probe[3],
      result.E, /* Expected value */
      result.SE); /* Standard error */
    break;
  case BOUNDARY_COMPUTE:
    printf("Temperature at boundary %s at t=%g = %g +/- %g\n",
      stardis->solve_name, stardis->probe[3],
      result.E, /* Expected value */
      result.SE); /* Standard error */
    break;
  case FLUX_BOUNDARY_COMPUTE:
    printf("Flux at boundary %s at t=%g = %g +/- %g\n",
      stardis->solve_name, stardis->probe[3],
      result.E, /* Expected value */
      result.SE); /* Standard error */
    break;
  default: FATAL("Invalid mode.");
  }
  printf("#failures: %zu/%zu\n", nfailures, stardis->N);
  if (nfailures)
    fprintf(stderr, "#failures: %zu/%zu\n", nfailures, stardis->N);

  /* Dump paths according to user settings */
  res = sdis_estimator_for_each_path(estimator, dump_path, stdout);
  if (res != RES_OK) goto error;
end:
  return res;
error:
  goto end;
}

void
dump_vtk
  (const struct geometry* geometry)
{
  size_t i;
  struct vertex* vtx;
  struct triangle* tri;

  ASSERT(geometry);

  vtx = geometry->vertex;
  tri = geometry->triangle;

  printf("# vtk DataFile Version 2.0\nvtk output\nASCII\nDATASET POLYDATA\n");
  printf("POINTS %zu float\n\n", sa_size(vtx));
  for (i = 0; i < sa_size(vtx); ++i) {
    printf("%f %f %f\n", SPLIT3(vtx[i].xyz));
  }
  printf("\nPOLYGONS %zu %zu\n", sa_size(tri), 4 * sa_size(tri));
  for (i = 0; i < sa_size(tri); ++i) {
    printf("3 %u %u %u\n", SPLIT3(tri[i].indices.data));
  }
  printf("\nCELL_DATA %zu\n", sa_size(tri));
  printf("SCALARS front_description long_long_int 1\n");
  printf("LOOKUP_TABLE default\n");
  for (i = 0; i < sa_size(tri); ++i) {
    printf("%lld\n", (long long int)tri[i].front_description);
  }
  printf("SCALARS back_description long_long_int 1\n");
  printf("LOOKUP_TABLE default\n");
  for (i = 0; i < sa_size(tri); ++i) {
    printf("%lld\n", (long long int)tri[i].back_description);
  }
}

void
dump_map
  (const struct stardis* stardis,
   struct sdis_estimator** estimators)
{
  size_t i, last_v = 0;
  size_t* idx;
  struct vertex* vtx;
  struct triangle* tri;

  ASSERT(stardis);

  idx = stardis->boundary.primitives;
  vtx = stardis->geometry.vertex;
  tri = stardis->geometry.triangle;

  /* Find last used vertex */
  for (i = 0; i < sa_size(idx); ++i) {
    size_t t = idx[i];
    last_v = MMAX(MMAX(last_v, tri[t].indices.data[0]),
      MMAX(tri[t].indices.data[1], tri[t].indices.data[2]));
  }

  /* Dump vertices up to last_v, even unused ones, to avoid reindexing */
  printf("# vtk DataFile Version 2.0\nvtk output\nASCII\nDATASET POLYDATA\n");
  printf("POINTS %zu float\n\n", last_v + 1);
  for (i = 0; i <= last_v; ++i) {
    printf("%f %f %f\n", SPLIT3(vtx[i].xyz));
  }
  /* Dump only primitives in boundary */
  printf("\nPOLYGONS %zu %zu\n", sa_size(idx), 4 * sa_size(idx));
  for (i = 0; i < sa_size(idx); ++i) {
    size_t t = idx[i];
    printf("3 %u %u %u\n", SPLIT3(tri[t].indices.data));
  }
  printf("\nCELL_DATA %zu\n", sa_size(idx));
  printf("SCALARS temperature_estimate float 1\n");
  printf("LOOKUP_TABLE default\n");
  for (i = 0; i < sa_size(idx); ++i) {
    const struct sdis_estimator* estimator = estimators[i];
    struct sdis_mc T;
    SDIS(estimator_get_temperature(estimator, &T));
    printf("%f\n", T.E);
  }
  printf("SCALARS temperature_std_dev float 1\n");
  printf("LOOKUP_TABLE default\n");
  for (i = 0; i < sa_size(idx); ++i) {
    const struct sdis_estimator* estimator = estimators[i];
    struct sdis_mc T;
    SDIS(estimator_get_temperature(estimator, &T));
    printf("%f\n", T.SE);
  }
  printf("SCALARS temperature_failures_count unsigned_long_long 1\n");
  printf("LOOKUP_TABLE default\n");
  for (i = 0; i < sa_size(idx); ++i) {
    const struct sdis_estimator* estimator = estimators[i];
    size_t nfails;
    SDIS(estimator_get_failure_count(estimator, &nfails));
    printf("%zu\n", nfails);
  }
  printf("SCALARS computation_time_estimate float 1\n");
  printf("LOOKUP_TABLE default\n");
  for (i = 0; i < sa_size(idx); ++i) {
    const struct sdis_estimator* estimator = estimators[i];
    struct sdis_mc time;
    SDIS(estimator_get_realisation_time(estimator, &time));
    printf("%f\n", time.E);
  }
  printf("SCALARS computation_time_std_dev float 1\n");
  printf("LOOKUP_TABLE default\n");
  for (i = 0; i < sa_size(idx); ++i) {
    const struct sdis_estimator* estimator = estimators[i];
    struct sdis_mc time;
    SDIS(estimator_get_realisation_time(estimator, &time));
    printf("%f\n", time.SE);
  }
}

void
print_trg_as_obj
  (FILE* stream,
   const struct vertex* vertices,
   const unsigned* indices)
{
  ASSERT(stream && vertices && indices);
  fprintf(stream, "v %.8f %.8f %.8f\nv %.8f %.8f %.8f\nv %.8f %.8f %.8f\nf 1 2 3\n",
    SPLIT3(vertices[indices[0]].xyz),
    SPLIT3(vertices[indices[1]].xyz),
    SPLIT3(vertices[indices[2]].xyz));
}
