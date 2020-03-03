#include "stardis-output.h"
#include "stardis-compute.h"
#include "stardis-fluid.h"
#include "stardis-solid.h"
#include "stardis-intface.h"
#include "stardis-app.h"

#include <sdis.h>

#include<star/senc3d.h>

#include <rsys/math.h>
#include <rsys/mem_allocator.h>
#include <rsys/dynamic_array_uint.h>

#include <stdio.h>

struct w_ctx {
  const struct darray_descriptions* desc;
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
  const struct w_ctx* w_ctx = ctx;

  ASSERT(mdm && ctx);

  data = sdis_medium_get_data(mdm);
  type = sdis_medium_get_type(mdm);

  switch (type) {
  case SDIS_FLUID: {
    FATAL("Unexpected power term in fluid");
  }
  case SDIS_SOLID: {
    struct solid* d__ = sdis_data_get(data);
    const struct description* descs = darray_descriptions_cdata_get(w_ctx->desc);
    id = d__->id;
    ASSERT(id == descs[id].d.solid.solid_id); (void)descs;
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
  const struct description* descs;

  ASSERT(interf && ctx);

  data = sdis_interface_get_data(interf);
  d__ = sdis_data_get(data);
  id = (side == SDIS_FRONT) ? d__->front_boundary_id : d__->back_boundary_id;
  descs = darray_descriptions_cdata_get(w_ctx->desc);

  switch (descs[id].type) {
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_T_FOR_FLUID:
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    FATAL("Cannot have a flux term here.\n"); break;
  case DESC_BOUND_F_FOR_SOLID: {
    double* w;
    ASSERT(id == descs[id].d.f_boundary.mat_id);
    w = htable_weigth_find(&w_ctx->weigths, &id);
    if(w) *w += flux_term;
    else ERR(htable_weigth_set(&w_ctx->weigths, &id, &flux_term));
    break;
  }
  default: FATAL("Unreachable code.\n"); break;
  }
end:
  return res;
error:
  goto end;
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
    ERR(sdis_heat_path_get_vertex(path, i, &vtx));
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
    ERR(sdis_heat_path_get_vertex(path, i, &vtx));
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
    ERR(sdis_heat_path_get_vertex(path, i, &vtx));
    fprintf(stream, "%g\n", vtx.weight);
  }
  /* Write the time of the random walk vertices */
  fprintf(stream, "SCALARS Time double 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(i, 0, vcount) {
    struct sdis_heat_vertex vtx;
    ERR(sdis_heat_path_get_vertex(path, i, &vtx));
    fprintf(stream, "%g\n", IS_INF(vtx.time) ? FLT_MAX : vtx.time);
  }
  /* Write path type */
  fprintf(stream, "CELL_DATA %u\n", 1u);
  fprintf(stream, "SCALARS Path_Type float 1\n");
  fprintf(stream, "LOOKUP_TABLE path_type\n");
  ERR(sdis_heat_path_get_status(path, &status));
  switch (status) {
  case SDIS_HEAT_PATH_SUCCESS: fprintf(stream, "0.0\n"); break;
  case SDIS_HEAT_PATH_FAILURE: fprintf(stream, "1.0\n"); break;
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
  const struct description* descs;
  CHK(path && ctx);

  ERR(sdis_green_path_get_limit_point(path, &pt));

  /* For each path, prints:
   * # end #power_terms #flux_terms power_term_1  ... power_term_n flux_term_1 ... flux_term_n
   * with:
   * - end = end_type end_id; end_type = T | H | X | R | F | S
   * - power_term_i = power_type_i power_id_i factor_i
   * - flux_term_i = flux_id_i factor_i
   */

  descs = darray_descriptions_cdata_get(w_ctx->desc);
  switch (pt.type) {
  case SDIS_FRAGMENT: {
    struct intface* d__;
    data = sdis_interface_get_data(pt.data.itfrag.intface);
    d__ = sdis_data_get(data);
    id = (pt.data.itfrag.fragment.side == SDIS_FRONT)
      ? d__->front_boundary_id : d__->back_boundary_id;
    ASSERT(id != UINT_MAX);
    switch (descs[id].type) {
    case DESC_BOUND_T_FOR_SOLID:
    case DESC_BOUND_T_FOR_FLUID:
      ASSERT(id == descs[id].d.t_boundary.mat_id);
      printf("T\t%u", id);
      break;
    case DESC_BOUND_H_FOR_SOLID:
    case DESC_BOUND_H_FOR_FLUID:
      ASSERT(id == descs[id].d.h_boundary.mat_id);
      printf("H\t%u", id);
      break;
    case DESC_BOUND_F_FOR_SOLID:
      ASSERT(id == descs[id].d.f_boundary.mat_id);
      printf("X\t%u", id);
      break;
    default: FATAL("Unreachable code.\n"); break;
    }
    break;
  }
  case SDIS_VERTEX:
    type = sdis_medium_get_type(pt.data.mdmvert.medium);
    data = sdis_medium_get_data(pt.data.mdmvert.medium);
    if(pt.data.mdmvert.vertex.P[0] == INF) {
      /* Radiative output */
      size_t sz = darray_descriptions_size_get(w_ctx->desc);
      ASSERT(sz <= UINT_MAX);
      printf("R\t%u", (unsigned)sz);
    }
    else if(type == SDIS_FLUID) {
      struct fluid* d__ = sdis_data_get(data);
      id = d__->id;
      ASSERT(id == descs[id].d.fluid.fluid_id);
      if(d__->is_outside)
        /* If outside the model and in a fluid with known temperature,
         * its a fluid attached to a H boundary */
        printf("H\t%u", id);
      /* In a standard fluid with known temperature */
      else printf("F\t%u", id);
    } else {
      struct solid* d__ = sdis_data_get(data);
      ASSERT(type == SDIS_SOLID);
      id = d__->id;
      ASSERT(id == descs[id].d.solid.solid_id);
      ASSERT(!d__->is_outside); /* FIXME: what if in external solid? */
      printf("S\t%u", id);
    }
    break;
  default: FATAL("Unreachable code.\n"); break;
  }

  ERR(sdis_green_function_get_power_terms_count(path, &pcount));

  htable_weigth_clear(&w_ctx->weigths);
  ERR(sdis_green_path_for_each_flux_term(path, get_flux_terms, w_ctx));
  fcount = htable_weigth_size_get(&w_ctx->weigths);

  printf("\t%zu\t%zu", pcount, fcount);

  ERR(sdis_green_path_for_each_power_term(path, print_power_term, ctx));

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
  ERR(sdis_estimator_buffer_get_definition(buf, definition));

  temps = mem_alloc(definition[0] * definition[1] * sizeof(double));
  if(temps == NULL) {
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
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_get_temperature(estimator, &T));
      fprintf(stdout, "%f\n", T.E);
    }
  }
  fprintf(stdout, "SCALARS temperature_std_dev float 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc T;
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_get_temperature(estimator, &T));
      fprintf(stdout, "%f\n", T.SE);
    }
  }
  fprintf(stdout, "SCALARS computation_time float 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc time;
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_get_realisation_time(estimator, &time));
      fprintf(stdout, "%f\n", time.E);
    }
  }
  fprintf(stdout, "SCALARS computation_time_std_dev float 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc time;
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_get_realisation_time(estimator, &time));
      fprintf(stdout, "%f\n", time.SE);
    }
  }
  fprintf(stdout, "SCALARS temperature_failures_count unsigned_long_long 1\n");
  fprintf(stdout, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      size_t nfails;
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_get_failure_count(estimator, &nfails));
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
   const struct darray_descriptions* descriptions,
   const double radiative_temps[2])
{
  res_T res = RES_OK;
  size_t ok_count, failed_count, sz;
  struct w_ctx w_ctx;
  int table_initialized = 0;
  unsigned i, szd;
  const struct description* descs;

  ASSERT(green && counts && descriptions && radiative_temps);

  ERR(sdis_green_function_get_invalid_paths_count(green, &failed_count));
  ERR(sdis_green_function_get_paths_count(green, &ok_count));
  sz = darray_descriptions_size_get(descriptions);
  ASSERT(sz <= UINT_MAX);
  szd = (unsigned)sz;
  descs = darray_descriptions_cdata_get(descriptions);

  /* Output counts */
  printf("---BEGIN GREEN---\n");
  printf("# #solids #fluids #t_boundaries #h_boundaries #f_boundaries #ok #failures\n");
  printf("%zu %zu %zu %zu %zu %zu %zu\n",
    counts->smed_count, counts->fmed_count,
    counts->tbound_count, counts->hbound_count, counts->fbound_count,
    ok_count, failed_count);

  /* List Media */
  if(counts->smed_count) {
    printf("# Solids\n");
    printf("# ID Name lambda rho cp power\n");
    FOR_EACH(i, 0, szd) {
      const struct description* desc = descs + i;
      const struct mat_solid* sl;
      if(desc->type != DESC_MAT_SOLID) continue;
      sl = &desc->d.solid;
      printf("%u\t%s\t%g\t%g\t%g\t%g\n",
        i, str_cget(&sl->name), sl->lambda, sl->rho, sl->cp, sl->vpower);
    }
  }
  if(counts->fmed_count) {
    printf("# Fluids\n");
    printf("# ID Name rho cp\n");
    FOR_EACH(i, 0, szd) {
      const struct description* desc = descs + i;
      const struct mat_fluid* fl;
      fl = &desc->d.fluid;
      printf("%u\t%s\t%g\t%g\n", i, str_cget(&fl->name), fl->rho, fl->cp);
    }
  }

  /* List Boundaries */
  if(counts->tbound_count) {
    printf("# T Boundaries\n");
    printf("# ID Name temperature\n");
    FOR_EACH(i, 0, szd) {
      const struct description* desc = descs + i;
      const struct t_boundary* bd;
      if(desc->type != DESC_BOUND_T_FOR_SOLID
        && desc->type != DESC_BOUND_T_FOR_FLUID) continue;
      bd = &desc->d.t_boundary;
      printf("%u\t%s\t%g\n", i, str_cget(&bd->name), bd->imposed_temperature);
    }
  }
  if(counts->hbound_count) {
    printf("# H Boundaries\n");
    printf("# ID Name emissivity specular_fraction hc hc_max T_env\n");
    FOR_EACH(i, 0, szd) {
      const struct description* desc = descs + i;
      const struct h_boundary* bd;
      if(desc->type != DESC_BOUND_H_FOR_SOLID
        && desc->type != DESC_BOUND_H_FOR_FLUID) continue;
      bd = &desc->d.h_boundary;
      printf("%u\t%s\t%g\t%g\t%g\t%g\n",
        i, str_cget(&bd->name), bd->emissivity, bd->specular_fraction,
        bd->hc, bd->imposed_temperature);
    }
  }
  if(counts->fbound_count) {
    printf("# F Boundaries\n");
    printf("# ID Name flux\n");
    FOR_EACH(i, 0, szd) {
      const struct description* desc = descs + i;
      const struct f_boundary* bd;
      if(desc->type != DESC_BOUND_F_FOR_SOLID) continue;
      bd = &desc->d.f_boundary;
      printf("%u\t%s\t%g\n",
        i, str_cget(&bd->name), bd->imposed_flux);
    }
  }

  /* Radiative Temperatures */
  printf("# Radiative Temperatures\n");
  printf("# ID Rad_Temp Lin_Temp\n");
  printf("%u\t%g\t%g\n", szd, radiative_temps[0], radiative_temps[1]);

  printf("# Samples\n");
  printf("# end #power_terms #flux_terms power_term_1 ... power_term_n flux_term_1 ... flux_term_n\n");
  printf("# end = end_type end_id; end_type = T | H | X | R | F | S\n");
  printf("# power_term_i = power_type_i power_id_i factor_i\n");
  printf("# flux_term_i = flux_id_i factor_i\n");

  w_ctx.desc = descriptions;
  htable_weigth_init(NULL, &w_ctx.weigths);
  table_initialized = 1;

  ERR(sdis_green_function_for_each_path(green, print_sample, &w_ctx));

  printf("---END GREEN---\n");

  ERR(sdis_green_function_ref_put(green));

end:
  if(table_initialized) htable_weigth_release(&w_ctx.weigths);
  return res;
error:
  goto end;
}

int
has_holes(const struct stardis* stardis)
{
  struct senc3d_scene* senc3d_scn;
  unsigned count;
  SDIS(scene_get_senc3d_scene(stardis->geometry.sdis_scn, &senc3d_scn));
  SENC3D(scene_get_frontier_segments_count(senc3d_scn, &count));
  return count != 0;
}

res_T
dump_holes_at_the_end_of_vtk
  (const struct stardis* stardis,
   FILE* stream)
{
  res_T res = RES_OK;
  unsigned* trgs = NULL;
  struct senc3d_scene* senc3d_scn = NULL;
  unsigned tsz, scount, i;
  ASSERT(stardis && stream);
  ASSERT(stardis);

  ERR(sg3d_geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tsz));
  /* Store frontier triangles */
  trgs = MEM_CALLOC(stardis->allocator, tsz, sizeof(*trgs));
  if(!trgs) {
    res = RES_MEM_ERR;
    goto error;
  }

  /* Keep the involved segments (not the vertices) */
  ERR(sdis_scene_get_senc3d_scene(stardis->geometry.sdis_scn, &senc3d_scn));
  ERR(senc3d_scene_get_frontier_segments_count(senc3d_scn, &scount));
  FOR_EACH(i, 0, scount) {
    unsigned vrtc[2], trid;
    ERR(senc3d_scene_get_frontier_segment(senc3d_scn, i, vrtc, &trid));
    trgs[trid] = 1;
  }

  fprintf(stream, "SCALARS Holes_frontiers int\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(i, 0, tsz) fprintf(stream, "%d\n", trgs[i]);

exit:
  if(trgs) MEM_RM(stardis->allocator, trgs);
  if(senc3d_scn) senc3d_scene_ref_put(senc3d_scn);
  return res;
error:
  goto exit;
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
  ERR(sdis_estimator_get_temperature(estimator, &result));
  ERR(sdis_estimator_get_failure_count(estimator, &nfailures));

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
      str_cget(&stardis->solve_name), stardis->probe[3],
      result.E, /* Expected value */
      result.SE); /* Standard error */
    break;
  case BOUNDARY_COMPUTE:
    printf("Temperature at boundary %s at t=%g = %g +/- %g\n",
      str_cget(&stardis->solve_name), stardis->probe[3],
      result.E, /* Expected value */
      result.SE); /* Standard error */
    break;
  case FLUX_BOUNDARY_COMPUTE: {
    enum sdis_estimator_type type;
    ERR(sdis_estimator_get_type(estimator, &type));
    ASSERT(type == SDIS_ESTIMATOR_FLUX);

    printf("Temperature at boundary %s at t=%g = %g +/- %g\n",
      str_cget(&stardis->solve_name), stardis->probe[3],
      result.E, /* Expected value */
      result.SE); /* Standard error */
    ERR(sdis_estimator_get_convective_flux(estimator, &result));
    printf("Convective flux at boundary %s at t=%g = %g +/- %g\n",
      str_cget(&stardis->solve_name), stardis->probe[3],
      result.E, /* Expected value */
      result.SE); /* Standard error */
    ERR(sdis_estimator_get_radiative_flux(estimator, &result));
    printf("Radiative flux at boundary %s at t=%g = %g +/- %g\n",
      str_cget(&stardis->solve_name), stardis->probe[3],
      result.E, /* Expected value */
      result.SE); /* Standard error */
    ERR(sdis_estimator_get_total_flux(estimator, &result));
    printf("Total flux Flux at boundary %s at t=%g = %g +/- %g\n",
      str_cget(&stardis->solve_name), stardis->probe[3],
      result.E, /* Expected value */
      result.SE); /* Standard error */
    break;
  }
  default: FATAL("Invalid mode.");
  }
  printf("#failures: %zu/%zu\n", nfailures, stardis->samples);
  if(nfailures)
    fprintf(stderr, "#failures: %zu/%zu\n", nfailures, stardis->samples);

  /* Dump paths according to user settings */
  ERR(sdis_estimator_for_each_path(estimator, dump_path, stdout));
end:
  return res;
error:
  goto end;
}

void
dump_map
  (const struct stardis* stardis,
   const struct darray_estimators* estimators)
{
  unsigned i, vcount, tcount, last_v = 0;
  const size_t* idx;
  size_t sz;
  unsigned szp;
  const struct sdis_estimator* const* est;

  ASSERT(stardis && estimators);

  est = darray_estimators_cdata_get(estimators);
  idx = darray_size_t_cdata_get(&stardis->compute_region.primitives);
  sz = darray_size_t_size_get(&stardis->compute_region.primitives);
  ASSERT(sz <= UINT_MAX);
  szp = (unsigned)sz;
  SG3D(geometry_get_unique_vertices_count(stardis->geometry.sg3d, &vcount));
  SG3D(geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tcount));

  /* Find last used vertex */
  for(i = 0; i < szp; ++i) {
    unsigned t;
    unsigned indices[3];
    ASSERT(idx[i] <= UINT_MAX);
    t = (unsigned)idx[i];
    SG3D(geometry_get_unique_triangle_vertices(stardis->geometry.sg3d, t,
      indices));
    last_v = MMAX(MMAX(last_v, indices[0]), MMAX(indices[1], indices[2]));
  }

  /* Dump vertices up to last_v, even unused ones, to avoid reindexing */
  printf("# vtk DataFile Version 2.0\nvtk output\nASCII\nDATASET POLYDATA\n");
  printf("POINTS %u float\n\n", last_v + 1);
  for(i = 0; i <= last_v; ++i) {
    double coord[3];
    SG3D(geometry_get_unique_vertex(stardis->geometry.sg3d, i, coord));
    printf("%f %f %f\n", SPLIT3(coord));
  }
  /* Dump only primitives in boundary */
  printf("\nPOLYGONS %u %u\n", szp, 4 * szp);
  for(i = 0; i < szp; ++i) {
    unsigned t;
    unsigned indices[3];
    ASSERT(idx[i] <= UINT_MAX);
    t = (unsigned)idx[i];
    SG3D(geometry_get_unique_triangle_vertices(stardis->geometry.sg3d, t,
      indices));
    printf("3 %u %u %u\n", SPLIT3(indices));
  }
  printf("\nCELL_DATA %u\n", szp);
  printf("SCALARS temperature_estimate float 1\n");
  printf("LOOKUP_TABLE default\n");
  for(i = 0; i < szp; ++i) {
    struct sdis_mc T;
    SDIS(estimator_get_temperature(est[i], &T));
    printf("%f\n", T.E);
  }
  printf("SCALARS temperature_std_dev float 1\n");
  printf("LOOKUP_TABLE default\n");
  for(i = 0; i < szp; ++i) {
    struct sdis_mc T;
    SDIS(estimator_get_temperature(est[i], &T));
    printf("%f\n", T.SE);
  }
  printf("SCALARS temperature_failures_count unsigned_long_long 1\n");
  printf("LOOKUP_TABLE default\n");
  for(i = 0; i < szp; ++i) {
    size_t nfails;
    SDIS(estimator_get_failure_count(est[i], &nfails));
    ASSERT(nfails <= UINT_MAX);
    printf("%u\n", (unsigned)nfails);
  }
  printf("SCALARS computation_time_estimate float 1\n");
  printf("LOOKUP_TABLE default\n");
  for(i = 0; i < szp; ++i) {
    struct sdis_mc time;
    SDIS(estimator_get_realisation_time(est[i], &time));
    printf("%f\n", time.E);
  }
  printf("SCALARS computation_time_std_dev float 1\n");
  printf("LOOKUP_TABLE default\n");
  for(i = 0; i < szp; ++i) {
    struct sdis_mc time;
    SDIS(estimator_get_realisation_time(est[i], &time));
    printf("%f\n", time.SE);
  }
}

res_T
dump_compute_region_at_the_end_of_vtk
  (const struct stardis* stardis,
   FILE* stream)
{
  res_T res = RES_OK;
  unsigned char* v = NULL;
  unsigned tsz, i;
  size_t j;
  ASSERT(stardis && stream);
  ASSERT(darray_size_t_size_get(&stardis->compute_region.primitives)
  == darray_sides_size_get(&stardis->compute_region.sides));

  ERR(sg3d_geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tsz));
  /* For triangles not in compute region v==0 */
  v = MEM_CALLOC(stardis->allocator, tsz, sizeof(*v));
  if(!v) {
    res = RES_MEM_ERR;
    goto error;
  }

  /* For triangles in compute region, v==1 if FRONT or v==2 for BACK */
  FOR_EACH(j, 0, darray_size_t_size_get(&stardis->compute_region.primitives)) {
    size_t prim = darray_size_t_cdata_get(&stardis->compute_region.primitives)[j];
    enum sdis_side side = darray_sides_cdata_get(&stardis->compute_region.sides)[j];
    ASSERT(prim <= tsz);
    ASSERT(v[(unsigned)prim] == 0); /* Not the same triangle twice */
    v[(unsigned)prim] = (side == SDIS_FRONT) ? 1 : 2;   
  }

  /* For triangles in compute region with error v==MAX */
  FOR_EACH(j, 0, darray_uint_size_get(&stardis->compute_region.err_triangles)) {
    unsigned prim = darray_uint_cdata_get(&stardis->compute_region.err_triangles)[j];
    ASSERT(prim <= tsz);
    ASSERT(v[prim] == 0); /* Not the same triangle twice */
    v[(unsigned)prim] = UCHAR_MAX;
  }

  fprintf(stream, "SCALARS Compute_region int\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(i, 0, tsz)
    fprintf(stream, "%d\n", v[i] == UCHAR_MAX ? INT_MAX : v[i]);

exit:
  if(v) MEM_RM(stardis->allocator, v);
  return res;
error:
  goto exit;
}