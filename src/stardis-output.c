/* Copyright (C) 2018-2021 |Meso|Star> (contact@meso-star.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>. */

#define _POSIX_C_SOURCE 200112L /* snprintf */
#include "stardis-output.h"
#include "stardis-compute.h"
#include "stardis-fluid.h"
#include "stardis-solid.h"
#include "stardis-intface.h"
#include "stardis-app.h"

#include <sdis.h>

#include<star/senc3d.h>
#include<star/sg3d.h>
#include<star/ssp.h>

#include <rsys/math.h>
#include <rsys/mem_allocator.h>
#include <rsys/dynamic_array_uint.h>
#include <rsys/hash_table.h>
#include <rsys/logger.h>
#include <rsys/str.h>
#include <rsys/clock_time.h>

#include <limits.h>
#include <string.h>
#include <stdio.h>

#define HTABLE_NAME weigth
#define HTABLE_DATA double
#define HTABLE_KEY unsigned
#include <rsys/hash_table.h>

struct w_ctx {
  struct mem_allocator* alloc;
  const struct darray_descriptions* desc;
  struct htable_weigth pw;
  struct htable_weigth flux;
  FILE* stream;
};

struct e_ctx {
  const struct darray_descriptions* desc;
  FILE* stream;
};

/*******************************************************************************
 * Local Type; for documentation purpose
 * These values are used in dumps
 ******************************************************************************/
enum enclosure_errors_t {
  NO_ENCLOSURE_ERROR = BIT(0),
  ENCLOSURE_WITH_N_MEDIA = BIT(1),
  ENCLOSURE_WITH_UNDEF_MEDIUM = BIT(2)
};

/******************************************************************************
 * Local Type used to write binary Green the way sgreen expects it
 * (its mainly a matter of record size and layout)
 *****************************************************************************/
struct green_description {
  enum description_type type;
  union {
    struct fluid fluid;
    struct solid solid;
    struct t_boundary t_boundary;
    struct f_boundary f_boundary;
    struct h_boundary h_boundary;
    struct solid_fluid_connect sf_connect;
    struct solid_solid_connect ss_connect;
  } d;
};

static res_T
copy_desc_to_green_desc
  (struct green_description* gdesc,
   const struct darray_descriptions* descriptions,
   const size_t idx)
{
  size_t sz;
  const struct description* desc;
  ASSERT(gdesc && descriptions);
  sz = darray_descriptions_size_get(descriptions);
  CHK(idx < sz);
  desc = darray_descriptions_cdata_get(descriptions) + idx;
  gdesc->type = desc->type;
  switch(desc->type) {
    case DESC_MAT_FLUID:
      gdesc->d.fluid = *desc->d.fluid;
      CHK(desc->d.fluid->desc_id < sz);
      break;
    case DESC_MAT_SOLID:
      gdesc->d.solid = *desc->d.solid;
      CHK(desc->d.solid->desc_id < sz);
      break;
    case DESC_BOUND_H_FOR_FLUID:
    case DESC_BOUND_H_FOR_SOLID:
      gdesc->d.h_boundary = *desc->d.h_boundary;
      CHK(desc->d.h_boundary->mat_id < sz);
      break;
    case DESC_BOUND_T_FOR_SOLID:
      gdesc->d.t_boundary = *desc->d.t_boundary;
      CHK(desc->d.t_boundary->mat_id < sz);
      break;
    case DESC_BOUND_F_FOR_SOLID:
      gdesc->d.f_boundary = *desc->d.f_boundary;
      CHK(desc->d.f_boundary->mat_id < sz);
      break;
    case DESC_SOLID_FLUID_CONNECT:
      gdesc->d.sf_connect = *desc->d.sf_connect;
      CHK(desc->d.sf_connect->connection_id < sz);
      break;
    case DESC_SOLID_SOLID_CONNECT:
      gdesc->d.ss_connect = *desc->d.ss_connect;
      CHK(desc->d.ss_connect->connection_id < sz);
      break;
    default: return RES_BAD_ARG;
  }
  return RES_OK;
}

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

static res_T
merge_flux_terms
  (struct sdis_interface* interf,
   const enum sdis_side side,
   const double flux_term,
   void* ctx)
{
  res_T res = RES_OK;
  struct sdis_data* data = NULL;
  struct intface* d__;
  unsigned desc_id;
  struct w_ctx* w_ctx = ctx;
  const struct description* descs;

  ASSERT(interf && w_ctx);
  (void)side;

  data = sdis_interface_get_data(interf);
  d__ = sdis_data_get(data);
  desc_id = d__->desc_id;
  CHK(desc_id < darray_descriptions_size_get(w_ctx->desc));
  descs = darray_descriptions_cdata_get(w_ctx->desc);

  switch (descs[desc_id].type) {
  case DESC_BOUND_T_FOR_SOLID:
  case DESC_BOUND_H_FOR_SOLID:
  case DESC_BOUND_H_FOR_FLUID:
    FATAL("Cannot have a flux term here.\n"); break;
  case DESC_BOUND_F_FOR_SOLID: {
    double* w;
    w = htable_weigth_find(&w_ctx->flux, &desc_id);
    if(w) *w += flux_term;
    else ERR(htable_weigth_set(&w_ctx->flux, &desc_id, &flux_term));
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
merge_power_terms
  (struct sdis_medium* mdm,
   const double power_term,
   void* ctx)
{
  res_T res = RES_OK;
  struct sdis_data* data = NULL;
  enum sdis_medium_type type;
  struct w_ctx* w_ctx = ctx;
  size_t sz;

  ASSERT(mdm && w_ctx);

  sz = darray_descriptions_size_get(w_ctx->desc);
  data = sdis_medium_get_data(mdm);
  type = sdis_medium_get_type(mdm);

  switch (type) {
  case SDIS_FLUID: {
    /* Could be OK, but unimplemented in stardis */
    FATAL("Unexpected power term in fluid");
  }
  case SDIS_SOLID: {
    struct solid** psolid = sdis_data_get(data);
    double* w;
    unsigned id = (*psolid)->desc_id;
    CHK(id < sz);
    w = htable_weigth_find(&w_ctx->pw, &id);
    if(w) *w += power_term;
    else ERR(htable_weigth_set(&w_ctx->pw, &id, &power_term));
    break;
  }
  default: FATAL("Unreachable code.\n"); break;
  }
end:
  return res;
error:
  goto end;
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
dump_path
  (const struct sdis_heat_path* path,
   void* context)
{
  res_T res = RES_OK;
  struct dump_path_context* dump_ctx = context;
  FILE* stream = NULL;
  char* name = NULL;
  enum sdis_heat_path_flag status = SDIS_HEAT_PATH_NONE;
  size_t vcount_, name_sz;
  unsigned long i, vcount;

  ASSERT(path && dump_ctx
    && dump_ctx->stardis
    && !str_is_empty(&dump_ctx->stardis->paths_filename));

  ERR(sdis_heat_path_get_status(path, &status));

  name_sz = 20 + str_len(&dump_ctx->stardis->paths_filename);
  name = MEM_CALLOC(dump_ctx->stardis->allocator, name_sz, sizeof(*name));
  if(!name) {
    res = RES_MEM_ERR;
    goto error;
  }
  snprintf(name, name_sz, "%s%08lu%s.vtk",
    str_cget(&dump_ctx->stardis->paths_filename), dump_ctx->rank++,
    (status == SDIS_HEAT_PATH_FAILURE ? "_err" : ""));

  stream = fopen(name, "w");
  if(!stream) {
    res = RES_IO_ERR;
    goto error;
  }
  /* Header */
  fprintf(stream, "# vtk DataFile Version 2.0\n");
  fprintf(stream, "Heat path\n");
  fprintf(stream, "ASCII\n");
  fprintf(stream, "DATASET POLYDATA\n");
  /* Write path positions */
  sdis_heat_path_get_vertices_count(path, &vcount_);
  ASSERT(vcount_ < ULONG_MAX);
  vcount = (unsigned long)vcount_;
  fprintf(stream, "POINTS %lu double\n", vcount);
  FOR_EACH(i, 0, vcount) {
    struct sdis_heat_vertex vtx;
    ERR(sdis_heat_path_get_vertex(path, i, &vtx));
    fprintf(stream, "%g %g %g\n", SPLIT3(vtx.P));
  }
  /* Write the segment of the path */
  fprintf(stream, "LINES %d %lu\n", 1, 1 + vcount);
  fprintf(stream, "%lu", vcount);
  FOR_EACH(i, 0, vcount) fprintf(stream, " %lu", i);
  fprintf(stream, "\n");

  /* Write path type */
  fprintf(stream, "CELL_DATA %d\n", 1);
  fprintf(stream, "SCALARS Path_Type unsigned_char 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  switch (status) {
  case SDIS_HEAT_PATH_SUCCESS: fprintf(stream, "0\n"); break;
  case SDIS_HEAT_PATH_FAILURE: fprintf(stream, "1\n"); break;
  default: FATAL("Unreachable code.\n"); break;
  }

  fprintf(stream, "POINT_DATA %lu\n", vcount);
  /* Write the type of the random walk vertices */
  fprintf(stream, "SCALARS Vertex_Type unsigned_char 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(i, 0, vcount) {
    struct sdis_heat_vertex vtx;
    ERR(sdis_heat_path_get_vertex(path, i, &vtx));
    ASSERT((size_t)vtx.type <= UCHAR_MAX);
    fprintf(stream, "%d\n", vtx.type);
  }
  /* Write the weights of the random walk vertices */
  fprintf(stream, "SCALARS Weight double 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(i, 0, vcount) {
    struct sdis_heat_vertex vtx;
    ERR(sdis_heat_path_get_vertex(path, i, &vtx));
    fprintf(stream, "%g\n", vtx.weight);
  }
  /* If computation time is not INF for every vertex,
   * write the time of the random walk vertices */
  FOR_EACH(i, 0, vcount) {
    struct sdis_heat_vertex vtx;
    ERR(sdis_heat_path_get_vertex(path, i, &vtx));
    if(i == 0) {
      if(IS_INF(vtx.time)) break;
      fprintf(stream, "SCALARS Time double 1\n");
      fprintf(stream, "LOOKUP_TABLE default\n");
    }
    ASSERT(!IS_INF(vtx.time));
    fprintf(stream, "%g\n", vtx.time);
  }

end:
  MEM_RM(dump_ctx->stardis->allocator, name);
  if(stream) fclose(stream);
  return res;
error:
  goto end;
}

res_T
dump_vtk_image
  (const struct sdis_estimator_buffer* buf,
   FILE* stream)
{
  res_T res = RES_OK;
  size_t def[2];
  unsigned long definition[2];
  double* temps = NULL;
  size_t ix, iy;

  ASSERT(buf && stream);
  ERR(sdis_estimator_buffer_get_definition(buf, def));
  ASSERT(def[0] != 0 && def[1] != 0 && def[0] * def[1] <= ULONG_MAX);
  definition[0] = (unsigned long)def[0];
  definition[1] = (unsigned long)def[1];

  temps = mem_alloc(definition[0] * definition[1] * sizeof(double));
  if(temps == NULL) {
    res = RES_MEM_ERR;
    goto error;
  }

  /* Compute the per pixel temperature */
  fprintf(stream, "# vtk DataFile Version 2.0\n");
  fprintf(stream, "Infrared Image\n");
  fprintf(stream, "ASCII\n");
  fprintf(stream, "DATASET STRUCTURED_POINTS\n");
  fprintf(stream, "DIMENSIONS %lu %lu 1\n", definition[0], definition[1]);
  fprintf(stream, "ORIGIN 0 0 0\n");
  fprintf(stream, "SPACING 1 1 1\n");
  fprintf(stream, "POINT_DATA %lu\n", definition[0] * definition[1]);
  fprintf(stream, "SCALARS temperature_estimate float 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc T;
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_get_temperature(estimator, &T));
      fprintf(stream, "%f\n", T.E);
    }
  }
  fprintf(stream, "SCALARS temperature_std_dev float 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc T;
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_get_temperature(estimator, &T));
      fprintf(stream, "%f\n", T.SE);
    }
  }
  fprintf(stream, "SCALARS computation_time float 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc time;
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_get_realisation_time(estimator, &time));
      fprintf(stream, "%f\n", time.E);
    }
  }
  fprintf(stream, "SCALARS computation_time_std_dev float 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc time;
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_get_realisation_time(estimator, &time));
      fprintf(stream, "%f\n", time.SE);
    }
  }
  fprintf(stream, "SCALARS failures_count unsigned_long_long 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      size_t nfails;
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_get_failure_count(estimator, &nfails));
      ASSERT(nfails <= ULONG_MAX);
      fprintf(stream, "%lu\n", (unsigned long)nfails);
    }
  }
  mem_rm(temps);

end:
  return res;
error:
  goto end;
}

res_T
dump_ht_image
  (const struct sdis_estimator_buffer* buf,
   FILE* stream)
{
  res_T res = RES_OK;
  size_t def[2];
  unsigned long definition[2];
  size_t ix, iy;

  ASSERT(buf && stream);
  ERR(sdis_estimator_buffer_get_definition(buf, def));
  ASSERT(def[0] <= ULONG_MAX && def[1] <= ULONG_MAX);
  definition[0] = (unsigned long)def[0];
  definition[1] = (unsigned long)def[1];

  fprintf(stream, "%lu %lu\n", definition[0], definition[1]);
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      struct sdis_mc T;
      struct sdis_mc time;
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_get_realisation_time(estimator, &time));
      ERR(sdis_estimator_get_temperature(estimator, &T));
      fprintf(stream, "%f %f 0 0 0 0 %f %f\n",
        T.E, T.SE, time.E, time.SE);
    }
  };

end:
  return res;
error:
  goto end;
}

#define FW(Ptr, Count) \
  if((Count) != fwrite((Ptr), sizeof(*(Ptr)), (Count), stream)) { \
    res = RES_IO_ERR; \
    goto error; \
  }

struct path_header {
  unsigned end_id;
  unsigned pcount, fcount;
  char at_initial;
};

static FINLINE double
medium_get_t0
  (struct sdis_medium* medium)
{
  struct sdis_data* data = NULL;
  enum sdis_medium_type type;
  ASSERT(medium);
  type = sdis_medium_get_type(medium);
  data = sdis_medium_get_data(medium);
  if(type == SDIS_FLUID) {
    const struct fluid* fluid_props = sdis_data_cget(data);
    return fluid_props->t0;
  } else {
    const struct solid* solid_props = sdis_data_cget(data);
    ASSERT(type == SDIS_SOLID);
    return solid_props->t0;
  }
}

static res_T
dump_sample_end
  (struct sdis_green_path* path,
   void* ctx)
{
  res_T res = RES_OK;
  struct e_ctx* e_ctx = ctx;
  enum sdis_green_path_end_type end_type;
  FILE* stream;
  double elapsed;
  size_t sz;
  unsigned trad_id;

  CHK(path && ctx);

  stream = e_ctx->stream;
  ERR(sdis_green_path_get_elapsed_time(path, &elapsed));
  ERR(sdis_green_path_get_end_type(path, &end_type));
  sz = darray_descriptions_size_get(e_ctx->desc);
  ASSERT(sz <= UINT_MAX);
  trad_id = (unsigned)sz;
  if(end_type == SDIS_GREEN_PATH_END_RADIATIVE) {
    /* End, End ID, X, Y, Z, Elapsed time */
    fprintf(stream, "TRAD, %u, 0, 0, 0, %g\n", trad_id, elapsed);
  } else {
    struct sdis_point pt = SDIS_POINT_NULL;
    struct sdis_data* data = NULL;
    enum sdis_medium_type type;
    const struct description* descs;
    double* pos;
    unsigned id;

    ERR(sdis_green_path_get_limit_point(path, &pt));

    descs = darray_descriptions_cdata_get(e_ctx->desc);
    switch(pt.type) {
      case SDIS_FRAGMENT: {
        struct intface* d__;
        data = sdis_interface_get_data(pt.data.itfrag.intface);
        pos = pt.data.itfrag.fragment.P;
        d__ = sdis_data_get(data);
        id = d__->desc_id;
        CHK(DESC_IS_T(descs+id) || DESC_IS_H(descs+id));
        break;
      }
      case SDIS_VERTEX:
        type = sdis_medium_get_type(pt.data.mdmvert.medium);
        data = sdis_medium_get_data(pt.data.mdmvert.medium);
        pos = pt.data.mdmvert.vertex.P;
        if(pt.data.mdmvert.vertex.P[0] == INF) {
          /* Radiative output (Trad) */
          id = trad_id;
        }
        else if(type == SDIS_FLUID) {
          struct fluid** pfluid = sdis_data_get(data);
          id = (*pfluid )->desc_id;
        } else {
          struct solid** psolid = sdis_data_get(data);
          ASSERT(type == SDIS_SOLID);
          ASSERT(!(*psolid)->is_outside); /* FIXME: what if in external solid? */
          id = (*psolid)->desc_id;
        }
        break;
      default: FATAL("Unreachable code.\n"); break;
    }
    /* End, End ID, X, Y, Z, Elapsed time */
    fprintf(stream, "%s, %u, %g, %g, %g, %g\n",
      str_cget(get_description_name(descs + id)), id, SPLIT3(pos), elapsed);
  }

end:
  return res;
error:
  goto end;
}

static res_T
dump_sample
  (struct sdis_green_path* path,
   void* ctx)
{
  res_T res = RES_OK;
  struct htable_weigth_iterator it, end;
  struct path_header header;
  struct w_ctx* w_ctx = ctx;
  enum sdis_green_path_end_type end_type;
  FILE* stream;
  unsigned* ids = NULL;
  double* weights = NULL;
  size_t sz, i;
  unsigned trad_id;
  CHK(path && ctx);

  stream = w_ctx->stream;
  ERR(sdis_green_path_get_end_type(path, &end_type));
  sz = darray_descriptions_size_get(w_ctx->desc);
  ASSERT(sz <= UINT_MAX);
  trad_id = (unsigned)sz;
  if(end_type == SDIS_GREEN_PATH_END_RADIATIVE) {
    header.at_initial = 0;
    header.end_id = trad_id;
  } else {
    struct sdis_point pt = SDIS_POINT_NULL;
    struct sdis_data* data = NULL;
    enum sdis_medium_type type;
    const struct description* descs;
    double t0;

    ERR(sdis_green_path_get_limit_point(path, &pt));

    /* For each path, dump:
     * # end_id #power_terms #flux_terms
     *    power_id_1  ... power_id_n flux_id_1 ... flux_id_n
     *    power_factor_1  ... power_factor_n flux_factor_1 ... flux_factor_n
     */

    descs = darray_descriptions_cdata_get(w_ctx->desc);
    switch(pt.type) {
      case SDIS_FRAGMENT: {
        struct intface* d__;
        unsigned desc_id;
        data = sdis_interface_get_data(pt.data.itfrag.intface);
        d__ = sdis_data_get(data);
        desc_id = d__->desc_id;
        CHK(DESC_IS_T(descs+desc_id) || DESC_IS_H(descs+desc_id));
        header.end_id = desc_id;
        header.at_initial = 0;
        break;
      }
      case SDIS_VERTEX:
        type = sdis_medium_get_type(pt.data.mdmvert.medium);
        data = sdis_medium_get_data(pt.data.mdmvert.medium);
        t0 = medium_get_t0(pt.data.mdmvert.medium);
        header.at_initial = (pt.data.mdmvert.vertex.time <= t0);
        if(pt.data.mdmvert.vertex.P[0] == INF) {
          /* Radiative output (Trad) */
          header.end_id = trad_id;
        }
        else if(type == SDIS_FLUID) {
          struct fluid** pfluid = sdis_data_get(data);
          header.end_id = (*pfluid)->desc_id;
        } else {
          struct solid** psolid = sdis_data_get(data);
          ASSERT(type == SDIS_SOLID);
          ASSERT(!(*psolid)->is_outside); /* FIXME: what if in external solid? */
          header.end_id = (*psolid)->desc_id;
        }
        break;
      default: FATAL("Unreachable code.\n"); break;
    }
  }

  /* Merge power and flux terms */
  htable_weigth_clear(&w_ctx->pw);
  htable_weigth_clear(&w_ctx->flux);
  ERR(sdis_green_path_for_each_power_term(path, merge_power_terms, w_ctx));
  ERR(sdis_green_path_for_each_flux_term(path, merge_flux_terms, w_ctx));
  sz = htable_weigth_size_get(&w_ctx->pw);
  ASSERT(sz <= UINT_MAX);
  header.pcount = (unsigned)sz;
  sz = htable_weigth_size_get(&w_ctx->flux);
  ASSERT(sz <= UINT_MAX);
  header.fcount = (unsigned)sz;

  /* Write path's header */
  FW(&header, 1);

  /* Allocate buffers */
  sz = header.pcount + header.fcount;
  ids = MEM_CALLOC(w_ctx->alloc, sz, sizeof(*ids));
  weights = MEM_CALLOC(w_ctx->alloc, sz, sizeof(*weights));
  if(!ids || !weights) {
    res = RES_MEM_ERR;
    goto error;
  }

  /* Write terms */
  htable_weigth_begin(&w_ctx->pw, &it);
  htable_weigth_end(&w_ctx->pw, &end);
  i = 0;
  while(!htable_weigth_iterator_eq(&it, &end)) {
    double* w = htable_weigth_iterator_data_get(&it);
    unsigned* k = htable_weigth_iterator_key_get(&it);
    CHK(*k <= trad_id);
    ids[i] = *k;
    weights[i] = *w;
    htable_weigth_iterator_next(&it);
    i++;
  }
  CHK(i == header.pcount);

  htable_weigth_begin(&w_ctx->flux, &it);
  htable_weigth_end(&w_ctx->flux, &end);
  while (!htable_weigth_iterator_eq(&it, &end)) {
    double* w = htable_weigth_iterator_data_get(&it);
    unsigned* k = htable_weigth_iterator_key_get(&it);
    CHK(*k <= trad_id);
    ids[i] = *k;
    weights[i] = *w;
    htable_weigth_iterator_next(&it);
    i++;
  }
  CHK(i == header.pcount + header.fcount);

  FW(ids, sz);
  FW(weights, sz);

end:
  MEM_RM(w_ctx->alloc, ids);
  MEM_RM(w_ctx->alloc, weights);
  return res;
error:
  goto end;
}

res_T
dump_green_bin
  (struct sdis_green_function* green,
   const struct stardis* stardis,
   FILE* stream)
{
  res_T res = RES_OK;
  size_t ok_count, failed_count;
  size_t sz;
  struct w_ctx w_ctx;
  int table_initialized = 0;
  unsigned i, szd;
  const struct description* descs;
  unsigned name_pool_sz = 0;
  char* name_pool = NULL;
  char* pool_ptr;
  const char green_string[] = "GREEN_BIN_FILE:";
  const unsigned file_fmt_version = 4;
  /* The following type must be identical to its stardis-green counterpart! */
  struct bfile_green_counts {
    unsigned desc_count, smed_count, fmed_count, tbound_count, hbound_count,
      fbound_count, sfconnect_count, ssconnect_count, name_pool_sz;
    size_t ok_count, failed_count;
  } file_counts;

  ASSERT(green && stardis && stream);

  ERR(sdis_green_function_get_paths_count(green, &ok_count));
  ERR(sdis_green_function_get_invalid_paths_count(green, &failed_count));
  sz = darray_descriptions_size_get(&stardis->descriptions);
  ASSERT(sz <= UINT_MAX);
  szd = (unsigned)sz;
  ASSERT(szd ==
    (stardis->counts.smed_count + stardis->counts.fmed_count
      + stardis->counts.tbound_count + stardis->counts.hbound_count
      + stardis->counts.fbound_count + stardis->counts.sfconnect_count
      + stardis->counts.ssconnect_count));
  descs = darray_descriptions_cdata_get(&stardis->descriptions);

  /* Save names that do not fit inplace */
  FOR_EACH(i, 0, szd) {
    const struct description* desc = descs + i;
    const struct str* name = get_description_name(desc);
    const size_t len = str_len(name);
    ASSERT(name_pool_sz + len + 1 <= UINT_MAX);
    name_pool_sz += (unsigned)(len + 1);
  }
  pool_ptr = name_pool = MEM_ALLOC(stardis->allocator, name_pool_sz);
  if(!name_pool) {
    res = RES_MEM_ERR;
    goto error;
  }
  FOR_EACH(i, 0, szd) {
    const struct description* desc = descs + i;
    const struct str* name = get_description_name(desc);
    const size_t len = str_len(name);
    strcpy(pool_ptr, name->cstr);
    pool_ptr += len + 1;
  }
  ASSERT(pool_ptr == name_pool + name_pool_sz);
  /* Write Green string and file format version */
  FW(green_string, sizeof(green_string));
  FW(&file_fmt_version, 1);

  /* Write counts */
  file_counts.desc_count = szd;
  file_counts.smed_count = stardis->counts.smed_count;
  file_counts.fmed_count = stardis->counts.fmed_count;
  file_counts.tbound_count = stardis->counts.tbound_count;
  file_counts.hbound_count = stardis->counts.hbound_count;
  file_counts.fbound_count = stardis->counts.fbound_count;
  file_counts.sfconnect_count = stardis->counts.sfconnect_count;
  file_counts.ssconnect_count = stardis->counts.ssconnect_count;
  file_counts.name_pool_sz = name_pool_sz;
  file_counts.ok_count = ok_count;
  file_counts.failed_count = failed_count;
  FW(&file_counts, 1);

  /* Write descriptions*/
  for(i = 0; i < szd; i++) {
    struct green_description desc;
    ERR(copy_desc_to_green_desc(&desc, &stardis->descriptions, i));
    FW(&desc, 1);
  }

  /* Write names */
  if(name_pool_sz)
    FW(name_pool, name_pool_sz);

  /* Write radiative temperatures */
  FW(&stardis->trad, 1);
  FW(&stardis->trad_ref, 1);

  /* Write time range */
  FW(&stardis->time_range, 2);

  w_ctx.alloc = stardis->allocator;
  w_ctx.desc = &stardis->descriptions;
  htable_weigth_init(stardis->allocator, &w_ctx.pw);
  htable_weigth_init(stardis->allocator, &w_ctx.flux);
  w_ctx.stream = stream;
  table_initialized = 1;

  /* Write samples */
  ERR(sdis_green_function_for_each_path(green, dump_sample, &w_ctx));

end:
  MEM_RM(stardis->allocator, name_pool);
  if(table_initialized) htable_weigth_release(&w_ctx.pw);
  if(table_initialized) htable_weigth_release(&w_ctx.flux);
  return res;
error:
  goto end;
}

res_T
dump_paths_end
  (struct sdis_green_function* green,
   const struct stardis* stardis,
   FILE* stream)
{
  res_T res = RES_OK;
  struct e_ctx e_ctx = { 0 };

  ASSERT(green && stardis && stream);

  e_ctx.desc = &stardis->descriptions;
  e_ctx.stream = stream;

  fprintf(stream, "\"End\", \"End ID\", \"X\", \"Y\", \"Z\", \"Elapsed time\"\n");
  ERR(sdis_green_function_for_each_path(green, dump_sample_end, &e_ctx));

end:
  return res;
error:
  goto end;
}

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
  unsigned desc_id;
  size_t pcount, fcount;
  struct w_ctx* w_ctx = ctx;
  const struct description* descs;
  CHK(path && ctx);

  ERR(sdis_green_path_get_limit_point(path, &pt));

  /* For each path, prints:
   * # end #power_terms #flux_terms power_term_1  ... power_term_n flux_term_1 ... flux_term_n
   * with:
   * - end = end_type end_id; end_type = T | H | R | F | S
   * - power_term_i = power_type_i power_id_i factor_i
   * - flux_term_i = flux_id_i factor_i
   */

  descs = darray_descriptions_cdata_get(w_ctx->desc);
  switch (pt.type) {
  case SDIS_FRAGMENT: {
    struct intface* d__;
    data = sdis_interface_get_data(pt.data.itfrag.intface);
    d__ = sdis_data_get(data);
    desc_id = d__->desc_id;
    switch (descs[desc_id].type) {
    case DESC_BOUND_T_FOR_SOLID:
      fprintf(w_ctx->stream, "T\t%u", desc_id);
      break;
    case DESC_BOUND_H_FOR_SOLID:
    case DESC_BOUND_H_FOR_FLUID:
      fprintf(w_ctx->stream, "H\t%u", desc_id);
      break;
    case DESC_BOUND_F_FOR_SOLID:
      FATAL("Heat path cannot end at a flux boundary.\n"); break;
      break;
    default: FATAL("Unreachable code.\n"); break;
    }
    break;
  }
  case SDIS_VERTEX:
    type = sdis_medium_get_type(pt.data.mdmvert.medium);
    data = sdis_medium_get_data(pt.data.mdmvert.medium);
    if(pt.data.mdmvert.vertex.P[0] == INF) {
      /* Radiative output (Trad)*/
      size_t sz = darray_descriptions_size_get(w_ctx->desc);
      ASSERT(sz <= UINT_MAX);
      fprintf(w_ctx->stream, "R\t%u", (unsigned)sz);
    }
    else if(type == SDIS_FLUID) {
      struct fluid** pfluid = sdis_data_get(data);
      desc_id = (*pfluid)->desc_id;
      if((*pfluid)->is_outside)
        /* If outside the model and in a fluid with known temperature,
         * its a fluid attached to a H boundary */
        fprintf(w_ctx->stream, "H\t%u", desc_id);
      /* In a standard fluid with known temperature */
      else fprintf(w_ctx->stream, "F\t%u", desc_id);
    } else {
      struct solid** psolid = sdis_data_get(data);
      ASSERT(type == SDIS_SOLID);
      ASSERT(!(*psolid)->is_outside); /* FIXME: what if in external solid? */
      desc_id = (*psolid)->desc_id;
      fprintf(w_ctx->stream, "S\t%u", desc_id);
    }
    break;
  default: FATAL("Unreachable code.\n"); break;
  }

  ERR(sdis_green_function_get_power_terms_count(path, &pcount));

  htable_weigth_clear(&w_ctx->pw);
  htable_weigth_clear(&w_ctx->flux);
  ERR(sdis_green_path_for_each_power_term(path, merge_power_terms, w_ctx));
  ERR(sdis_green_path_for_each_flux_term(path, merge_flux_terms, w_ctx));
  fcount = htable_weigth_size_get(&w_ctx->flux);

  ASSERT(pcount <= ULONG_MAX && fcount <= ULONG_MAX);
  fprintf(w_ctx->stream, "\t%lu\t%lu",
    (unsigned long)pcount, (unsigned long)fcount);

  htable_weigth_begin(&w_ctx->pw, &it);
  htable_weigth_end(&w_ctx->pw, &end);
  while(!htable_weigth_iterator_eq(&it, &end)) {
    double* w = htable_weigth_iterator_data_get(&it);
    unsigned* k = htable_weigth_iterator_key_get(&it);
    fprintf(w_ctx->stream, "\t%u\t%g", *k, *w);
    htable_weigth_iterator_next(&it);
  }

  htable_weigth_begin(&w_ctx->flux, &it);
  htable_weigth_end(&w_ctx->flux, &end);
  while (!htable_weigth_iterator_eq(&it, &end)) {
    double* w = htable_weigth_iterator_data_get(&it);
    unsigned* k = htable_weigth_iterator_key_get(&it);
    fprintf(w_ctx->stream, "\t%u\t%g", *k, *w);
    htable_weigth_iterator_next(&it);
  }
  fprintf(w_ctx->stream, "\n");

end:
  return res;
error:
  goto end;
}

res_T
dump_green_ascii
  (struct sdis_green_function* green,
   const struct stardis* stardis,
   FILE* stream)
{
  res_T res = RES_OK;
  unsigned ok_count, failed_count;
  size_t sz;
  struct w_ctx w_ctx;
  int table_initialized = 0;
  unsigned i, szd;
  const struct description* descs;

  ASSERT(green && stardis && stream);

  ERR(sdis_green_function_get_paths_count(green, &sz));
  ASSERT(sz <= UINT_MAX);
  ok_count = (unsigned)sz;
  ERR(sdis_green_function_get_invalid_paths_count(green, &sz));
  ASSERT(sz <= UINT_MAX);
  failed_count = (unsigned)sz;
  sz = darray_descriptions_size_get(&stardis->descriptions);
  ASSERT(sz <= UINT_MAX);
  szd = (unsigned)sz;
  descs = darray_descriptions_cdata_get(&stardis->descriptions);

  /* Output counts */
  fprintf(stream, "---BEGIN GREEN---\n");
  fprintf(stream, "# time range\n");
  fprintf(stream, "%g %g\n",
    SPLIT2(stardis->time_range));
  fprintf(stream,
    "# #solids #fluids #t_boundaries #h_boundaries #f_boundaries #ok #failures\n");
  fprintf(stream, "%u %u %u %u %u %u %u\n",
    stardis->counts.smed_count, stardis->counts.fmed_count,
    stardis->counts.tbound_count, stardis->counts.hbound_count,
    stardis->counts.fbound_count, ok_count, failed_count);

  /* List Media */
  if(stardis->counts.smed_count) {
    fprintf(stream, "# Solids\n");
    fprintf(stream, "# ID Name lambda rho cp power initial_temp imposed_temp\n");
    FOR_EACH(i, 0, szd) {
      const struct description* desc = descs + i;
      const struct solid* sl;
      if(desc->type != DESC_MAT_SOLID) continue;
      sl = desc->d.solid;
      fprintf(stream, "%u\t%s\t%g\t%g\t%g\t%g",
        i, str_cget(&sl->name), sl->lambda, sl->rho, sl->cp, sl->vpower);
      if(sl->tinit >= 0) {
        fprintf(stream, "\t%g", sl->tinit);
      } else {
        fprintf(stream, "\tNONE");
      }
      if(sl->imposed_temperature >= 0) {
        fprintf(stream, "\t%g\n", sl->imposed_temperature);
      } else {
        fprintf(stream, "\tNONE\n");
      }
    }
  }
  if(stardis->counts.fmed_count) {
    fprintf(stream, "# Fluids\n");
    fprintf(stream, "# ID Name rho cp initial_temp imposed_temp\n");
    FOR_EACH(i, 0, szd) {
      const struct description* desc = descs + i;
      const struct fluid* fl;
      if(desc->type != DESC_MAT_FLUID) continue;
      fl = desc->d.fluid;
      if(fl->imposed_temperature >= 0) {
        fprintf(stream, "%u\t%s\t%g\t%g",
          i, str_cget(&fl->name), fl->rho, fl->cp);
      } else {
        fprintf(stream, "%u\t%s\t%g\t%g",
          i, str_cget(&fl->name), fl->rho, fl->cp);
      }
      if(fl->tinit >= 0) {
        fprintf(stream, "\t%g", fl->tinit);
      } else {
        fprintf(stream, "\tNONE");
      }
      if(fl->imposed_temperature >= 0) {
        fprintf(stream, "\t%g\n", fl->imposed_temperature);
      } else {
        fprintf(stream, "\tNONE\n");
      }
    }
  }

  /* List Boundaries */
  if(stardis->counts.tbound_count) {
    fprintf(stream, "# T Boundaries\n");
    fprintf(stream, "# ID Name temperature\n");
    FOR_EACH(i, 0, szd) {
      const struct description* desc = descs + i;
      const struct t_boundary* bd;
      bd = desc->d.t_boundary;
      fprintf(stream, "%u\t%s\t%g\n",
        i, str_cget(&bd->name), bd->imposed_temperature);
    }
  }
  if(stardis->counts.hbound_count) {
    fprintf(stream, "# H Boundaries\n");
    fprintf(stream, "# ID Name ref_temperature emissivity specular_fraction hc T_env\n");
    FOR_EACH(i, 0, szd) {
      const struct description* desc = descs + i;
      const struct h_boundary* bd;
      if(desc->type != DESC_BOUND_H_FOR_SOLID
        && desc->type != DESC_BOUND_H_FOR_FLUID) continue;
      bd = desc->d.h_boundary;
      fprintf(stream, "%u\t%s\t%g\t%g\t%g\t%g\t%g\n",
        i, str_cget(&bd->name), bd->ref_temperature, bd->emissivity,
        bd->specular_fraction, bd->hc, bd->imposed_temperature);
    }
  }
  if(stardis->counts.fbound_count) {
    fprintf(stream, "# F Boundaries\n");
    fprintf(stream, "# ID Name flux\n");
    FOR_EACH(i, 0, szd) {
      const struct description* desc = descs + i;
      const struct f_boundary* bd;
      if(desc->type != DESC_BOUND_F_FOR_SOLID) continue;
      bd = desc->d.f_boundary;
      fprintf(stream, "%u\t%s\t%g\n",
        i, str_cget(&bd->name), bd->imposed_flux);
    }
  }

  /* Radiative Temperature */
  fprintf(stream, "# Radiative Temperatures\n");
  fprintf(stream, "# ID Trad Trad_Ref\n");
  fprintf(stream, "%u\t%g\t%g\n",
    szd, stardis->trad, stardis->trad_ref);

  fprintf(stream, "# Samples\n");
  fprintf(stream,
    "# end #power_terms #flux_terms power_term_1 ... power_term_n flux_term_1 ... flux_term_n\n");
  fprintf(stream, "# end = end_type end_id; end_type = T | H | R | F | S\n");
  fprintf(stream, "# power_term_i = power_id_i factor_i\n");
  fprintf(stream, "# flux_term_i = flux_id_i factor_i\n");

  w_ctx.alloc = stardis->allocator;
  w_ctx.desc = &stardis->descriptions;
  htable_weigth_init(stardis->allocator, &w_ctx.pw);
  htable_weigth_init(stardis->allocator, &w_ctx.flux);
  w_ctx.stream = stream;
  table_initialized = 1;

  ERR(sdis_green_function_for_each_path(green, print_sample, &w_ctx));

  fprintf(stream, "---END GREEN---\n");

end:
  if(table_initialized) htable_weigth_release(&w_ctx.pw);
  if(table_initialized) htable_weigth_release(&w_ctx.flux);
  return res;
error:
  goto end;
}

res_T
dump_boundaries_at_the_end_of_vtk
  (const struct stardis* stardis,
   FILE* stream)
{
  res_T res = RES_OK;
  const struct description* descriptions;
  unsigned tsz, t;
  ASSERT(stardis && stream);

  ERR(sg3d_geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tsz));
  descriptions = darray_descriptions_cdata_get(&stardis->descriptions);

  fprintf(stream, "SCALARS Boundaries unsigned_int 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(t, 0, tsz) {
    unsigned descr[SG3D_PROP_TYPES_COUNT__];
    ERR(sg3d_geometry_get_unique_triangle_properties(stardis->geometry.sg3d, t,
      descr));
    if(descr[SG3D_INTFACE] != SG3D_UNSPECIFIED_PROPERTY
      && DESC_IS_BOUNDARY(descriptions+descr[SG3D_INTFACE]))
      /* Descriptions are numbered from 1 in the log (so the 1+ below) */
      fprintf(stream, "%u\n", 1 + descr[SG3D_INTFACE]);
    else fprintf(stream, "%u\n", SG3D_UNSPECIFIED_PROPERTY);
  }

exit:
  return res;
error:
  goto exit;
}

res_T
dump_enclosure_related_stuff_at_the_end_of_vtk
  (struct stardis* stardis,
   FILE* stream)
{
  res_T res = RES_OK;
  unsigned* trgs = NULL;
  struct senc3d_enclosure* enc = NULL;
  unsigned tsz, e, s, t, scount, ecount, ocount;
  int* enc_status = NULL;
  const struct description* descriptions;
  int invalid_enclosures_count = 0;
  ASSERT(stardis && stream);

  descriptions = darray_descriptions_cdata_get(&stardis->descriptions);
  ERR(sg3d_geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tsz));
  trgs = MEM_CALLOC(stardis->allocator, tsz, sizeof(*trgs));
  if(!trgs) {
    res = RES_MEM_ERR;
    goto error;
  }

  /* If enclosure where not extracted, dump only errors */
  ERR(senc3d_scene_get_overlapping_triangles_count(stardis->senc3d_scn, &ocount));
  if(ocount) {
    FOR_EACH(t, 0, tsz) trgs[t] = 0;
    FOR_EACH(t, 0, ocount) {
      unsigned trid;
      ERR(senc3d_scene_get_overlapping_triangle(stardis->senc3d_scn, t, &trid));
      trgs[trid] = 1;
    }
    fprintf(stream, "SCALARS Overlapping_triangles unsigned_int 1\n");
    fprintf(stream, "LOOKUP_TABLE default\n");
    FOR_EACH(t, 0, tsz) fprintf(stream, "%u\n", trgs[t]);
    goto exit;
  }

  /* Keep the segments involved in holes (not the vertices) */
  ERR(senc3d_scene_get_frontier_segments_count(stardis->senc3d_scn, &scount));
  if(scount) {
    /* Room to store frontier triangles */
    FOR_EACH(s, 0, scount) {
      unsigned vrtc[2], trid;
      ERR(senc3d_scene_get_frontier_segment(stardis->senc3d_scn, s, vrtc, &trid));
      trgs[trid] = 1;
    }
    logger_print(stardis->logger, LOG_WARNING, "Model contains hole(s).\n");
    fprintf(stream, "SCALARS Hole_frontiers unsigned_int 1\n");
    fprintf(stream, "LOOKUP_TABLE default\n");
    FOR_EACH(t, 0, tsz) fprintf(stream, "%u\n", trgs[t]);
  }

  /* Dump enclosure information */
  ERR(senc3d_scene_get_enclosure_count(stardis->senc3d_scn, &ecount));
  enc_status = MEM_CALLOC(stardis->allocator, ecount, sizeof(*enc_status));
  if(!enc_status) {
    res = RES_MEM_ERR;
    goto error;
  }
  ASSERT(stardis->undefined_medium_behind_boundary_id != SENC3D_UNSPECIFIED_MEDIUM);
  FOR_EACH(e, 0, ecount) {
    struct senc3d_enclosure_header header;
    unsigned tid, med, enc_fst_med = SENC3D_UNSPECIFIED_MEDIUM;
    int is_fst_med = 1, is_err_cs = 0;

    enc_status[e] = NO_ENCLOSURE_ERROR;
    ERR(senc3d_scene_get_enclosure(stardis->senc3d_scn, e, &enc));
    ERR(senc3d_enclosure_get_header(enc, &header));

    FOR_EACH(t, 0, header.unique_primitives_count) {
      unsigned prop[SG3D_PROP_TYPES_COUNT__];
      enum senc3d_side side;
      size_t j;
      ERR(senc3d_enclosure_get_triangle_id(enc, t, &tid, &side));
      FOR_EACH(j, 0, darray_uint_size_get(&stardis->compute_surface.err_triangles))
      {
        unsigned prim
          = darray_uint_cdata_get(&stardis->compute_surface.err_triangles)[j];
        if(prim == tid) {
          is_err_cs = 1;
          break;
        }
      }
      if(is_err_cs)
        /* Don't flag an enclosure invalid because of a triangle that is
         * considered not member of it */
        continue;

      ERR(sg3d_geometry_get_unique_triangle_properties(stardis->geometry.sg3d,
        tid, prop));

      if(prop[side] != SG3D_UNSPECIFIED_PROPERTY) {
        ASSERT(prop[side] < darray_descriptions_size_get(&stardis->descriptions));
        description_get_medium_id(descriptions + prop[side], &med);
      } else  {
        /* If unspecified behind a boundary, use a specific ID to avoid to flag
         * using different boundaries for a given enclosure as invalid */
        int properties_conflict_status;
        validate_properties(t, prop, stardis, &properties_conflict_status);
        if(properties_conflict_status == NO_PROPERTY_CONFLICT)
          med = stardis->undefined_medium_behind_boundary_id;
        else {
          enc_status[e] |= ENCLOSURE_WITH_UNDEF_MEDIUM;
          continue; /* Don't flag N_MEDIA at the same time */
        }
      }
      if(is_fst_med) {
        is_fst_med = 0;
        enc_fst_med = med;
      } else {
        if(enc_fst_med != med)
          enc_status[e] |= ENCLOSURE_WITH_N_MEDIA;
      }
    }
    /* The external (infinite) enclosure is always valid */
    if(enc_status[e] != NO_ENCLOSURE_ERROR && !header.is_infinite)
      invalid_enclosures_count++;
    ERR(senc3d_enclosure_ref_put(enc));
  }
  if(invalid_enclosures_count) {
    logger_print(stardis->logger, LOG_WARNING,
      "Found %d invalid enclosure(s).\n", invalid_enclosures_count);
  }
  fprintf(stream, "FIELD EnclosuresData 2\n");
  fprintf(stream, "Enclosures %d %d unsigned_char\n", ecount, tsz);
  FOR_EACH(t, 0, tsz) {
    unsigned encs[2], is_err_cs = 0;
    size_t j;
    FOR_EACH(j, 0, darray_uint_size_get(&stardis->compute_surface.err_triangles))
    {
      unsigned prim
        = darray_uint_cdata_get(&stardis->compute_surface.err_triangles)[j];
      if(prim == t) {
        is_err_cs = 1;
        break;
      }
    }
    if(is_err_cs) {
      /* Triangles in compute surface with error are considered member of no
       * enclosure */
      FOR_EACH(e, 1, ecount) fprintf(stream, "0 ");
      fprintf(stream, "0\n");
      continue;
    }
    ERR(senc3d_scene_get_triangle_enclosures(stardis->senc3d_scn, t, encs));
    FOR_EACH(e, 0, ecount) {
      unsigned c = (e == encs[SENC3D_FRONT] || e == encs[SENC3D_BACK])
        ? (unsigned char)enc_status[e] : 0;
      if(e == ecount - 1)
        fprintf(stream, "%u\n", c);
      else fprintf(stream, "%u ", c);
    }
  }

#define ENC_NOT_MEMBER SENC3D_UNSPECIFIED_MEDIUM
#define ENC_MEMBER_2_DISTINT_MEDIA (ENC_NOT_MEMBER - 1)
#define ENC_MEMBER_NO_MEDIUM (ENC_NOT_MEMBER - 2)

  fprintf(stream, "Enclosures_internal_media %d %d unsigned_int\n", ecount, tsz);
  FOR_EACH(t, 0, tsz) {
    unsigned descr[SG3D_PROP_TYPES_COUNT__];
    unsigned encs[2];
    unsigned is_err_cs = 0;
    size_t j;
    ERR(senc3d_scene_get_triangle_enclosures(stardis->senc3d_scn, t, encs));
    ERR(sg3d_geometry_get_unique_triangle_properties(stardis->geometry.sg3d, t,
      descr));

    /* Special value for triangles in compute surface with error */
    FOR_EACH(j, 0, darray_uint_size_get(&stardis->compute_surface.err_triangles))
    {
      unsigned prim
        = darray_uint_cdata_get(&stardis->compute_surface.err_triangles)[j];
      if(prim == t) {
        is_err_cs = 1;
        break;
      }
    }
    if(is_err_cs) {
      /* Triangles in compute surface with error are considered member of no
       * enclosure */
      FOR_EACH(e, 1, ecount) fprintf(stream, "%u ", ENC_NOT_MEMBER);
      fprintf(stream, "%u\n", ENC_NOT_MEMBER);
      continue;
    }
    FOR_EACH(e, 0, ecount) {
      unsigned mid;
      if(e == encs[SENC3D_FRONT] && e == encs[SENC3D_BACK]) {
        /* Both sides of this triangle are in enclosure #e */
        unsigned fmid, bmid;
        if(descr[SG3D_FRONT] != SG3D_UNSPECIFIED_PROPERTY)
          description_get_medium_id(descriptions + descr[SG3D_FRONT], &fmid);
        else if(descr[SG3D_INTFACE] != SG3D_UNSPECIFIED_PROPERTY
          && DESC_IS_BOUNDARY(descriptions+descr[SG3D_INTFACE]))
        {
          description_get_medium_id(descriptions + descr[SG3D_INTFACE], &fmid);
        }
        else fmid = ENC_MEMBER_NO_MEDIUM;
        if(descr[SENC3D_BACK] != SG3D_UNSPECIFIED_PROPERTY)
          description_get_medium_id(descriptions + descr[SENC3D_BACK], &bmid);
        else if(descr[SG3D_INTFACE] != SG3D_UNSPECIFIED_PROPERTY
          && DESC_IS_BOUNDARY(descriptions+descr[SG3D_INTFACE]))
        {
          description_get_medium_id(descriptions + descr[SG3D_INTFACE], &bmid);
        }
        else bmid = ENC_MEMBER_NO_MEDIUM;
        mid = (fmid == bmid) ? fmid : ENC_MEMBER_2_DISTINT_MEDIA;
      }
      else if(e == encs[SENC3D_FRONT]) {
        /* Member of enclosure #e (front side only) */
        if(descr[SG3D_FRONT] != SG3D_UNSPECIFIED_PROPERTY)
          description_get_medium_id(descriptions + descr[SG3D_FRONT], &mid);
        else if(descr[SG3D_INTFACE] != SG3D_UNSPECIFIED_PROPERTY
          && DESC_IS_BOUNDARY(descriptions+descr[SG3D_INTFACE]))
        {
          description_get_medium_id(descriptions + descr[SG3D_INTFACE], &mid);
        }
        else mid = ENC_MEMBER_NO_MEDIUM;
      }
      else if(e == encs[SENC3D_BACK]) {
        /* Member of enclosure #e  (back side only) */
        if(descr[SENC3D_BACK] != SG3D_UNSPECIFIED_PROPERTY)
          description_get_medium_id(descriptions + descr[SENC3D_BACK], &mid);
        else if(descr[SG3D_INTFACE] != SG3D_UNSPECIFIED_PROPERTY
          && DESC_IS_BOUNDARY(descriptions+descr[SG3D_INTFACE]))
        {
          description_get_medium_id(descriptions + descr[SG3D_INTFACE], &mid);
        }
        else mid = ENC_MEMBER_NO_MEDIUM;
      } else {
        /* Not member of enclosure #e */
        mid = ENC_NOT_MEMBER;
      }
      if(e == ecount - 1)
        fprintf(stream, "%u\n", mid);
      else fprintf(stream, "%u ", mid);
    }
  }

#undef ENC_NOT_MEMBER
#undef ENC_ERR_COMPUTE_SURFACE
#undef ENC_MEMBER_2_DISTINT_MEDIA
#undef ENC_MEMBER_NO_MEDIUM

exit:
  MEM_RM(stardis->allocator, trgs);
  MEM_RM(stardis->allocator, enc_status);
  return res;
error:
  goto exit;
}

res_T
print_computation_time
  (struct sdis_estimator* estimator,
   struct stardis* stardis,
   struct time* start,
   struct time* compute_start,
   struct time* compute_end,
   struct time* output_end)
{
  res_T res = RES_OK;
  struct sdis_mc time;
  struct time tmp;
  char buf[128];
  const int flag = TIME_MSEC | TIME_SEC | TIME_MIN | TIME_HOUR | TIME_DAY;

  ASSERT(stardis && start && compute_start && compute_end);

  time_sub(&tmp, compute_start, start);
  time_dump(&tmp, flag, NULL, buf, sizeof(buf));
  logger_print(stardis->logger, LOG_OUTPUT,
    "Initialisation time = %s\n", buf);
  time_sub(&tmp, compute_end, compute_start);
  time_dump(&tmp, flag, NULL, buf, sizeof(buf));
  logger_print(stardis->logger, LOG_OUTPUT,
    "Computation time = %s\n", buf);
  if(output_end) {
    time_sub(&tmp, output_end, compute_end);
    time_dump(&tmp, flag, NULL, buf, sizeof(buf));
    logger_print(stardis->logger, LOG_OUTPUT,
      "Result output time = %s\n", buf);
  }

  if(estimator) {
    ERR(sdis_estimator_get_realisation_time(estimator, &time));
    logger_print(stardis->logger, LOG_OUTPUT,
      "Time per realisation (in usec) = %g +/- %g\n", time.E, time.SE);
  }

exit:
  return res;
error:
  goto exit;
}

res_T
print_single_MC_result
  (struct sdis_estimator* estimator,
   struct stardis* stardis,
   FILE* stream)
{
  res_T res = RES_OK;
  struct sdis_mc result;
  size_t nfailures_;
  unsigned long nfailures, nsamples;

  ASSERT(estimator && stardis && stream);

  /* Fetch the estimation data */
  ERR(sdis_estimator_get_temperature(estimator, &result));
  ERR(sdis_estimator_get_failure_count(estimator, &nfailures_));
  ASSERT(nfailures_ <= ULONG_MAX && stardis->samples <= ULONG_MAX);
  nfailures = (unsigned long)nfailures_;
  nsamples = (unsigned long)stardis->samples;
  if(nfailures == nsamples) {
    logger_print(stardis->logger, LOG_ERROR,
      "All the %lu samples failed. No result to display.\n", nsamples);
    res = RES_BAD_OP;
    goto error;
  }

  /* Print the results */
  switch (stardis->mode & COMPUTE_MODES) {
  case MODE_PROBE_COMPUTE:
    if(stardis->mode & MODE_EXTENDED_RESULTS) {
      if(stardis->time_range[0] == stardis->time_range[1])
        fprintf(stream, "Temperature at [%g, %g, %g] at t=%g = %g K +/- %g\n",
          SPLIT3(stardis->probe), stardis->time_range[0],
          result.E, /* Expected value */
          result.SE); /* Standard error */
      else
        fprintf(stream,
          "Temperature at [%g, %g, %g] with t in [%g %g] = %g K +/- %g\n",
          SPLIT3(stardis->probe), SPLIT2(stardis->time_range),
          result.E, /* Expected value */
          result.SE); /* Standard error */
    }
    else fprintf(stream, "%g %g %lu %lu\n",
      result.E, result.SE, nfailures, nsamples);
    break;
  case MODE_PROBE_COMPUTE_ON_INTERFACE:
    if(stardis->mode & MODE_EXTENDED_RESULTS) {
      if(stardis->time_range[0] == stardis->time_range[1])
        fprintf(stream,
          "Boundary temperature at [%g, %g, %g] at t=%g = %g K +/- %g\n",
          SPLIT3(stardis->probe), stardis->time_range[0],
          result.E, /* Expected value */
          result.SE); /* Standard error */
      else
        fprintf(stream,
          "Boundary temperature at [%g, %g, %g] with t in [%g %g] = %g K +/- %g\n",
          SPLIT3(stardis->probe), SPLIT2(stardis->time_range),
          result.E, /* Expected value */
          result.SE); /* Standard error */
    }
    else fprintf(stream, "%g %g %lu %lu\n",
      result.E, result.SE, nfailures, nsamples);
    break;
  case MODE_MEDIUM_COMPUTE:
    if(stardis->mode & MODE_EXTENDED_RESULTS) {
      if(stardis->time_range[0] == stardis->time_range[1])
        fprintf(stream, "Temperature in medium '%s' at t=%g = %g K +/- %g\n",
          str_cget(&stardis->solve_name), stardis->time_range[0],
          result.E, /* Expected value */
          result.SE); /* Standard error */
      else
        fprintf(stream,
          "Temperature in medium '%s' with t in [%g %g] = %g K +/- %g\n",
          str_cget(&stardis->solve_name), SPLIT2(stardis->time_range),
          result.E, /* Expected value */
          result.SE); /* Standard error */
    }
    else fprintf(stream, "%g %g %lu %lu\n",
      result.E, result.SE, nfailures, nsamples);
    break;
  case MODE_BOUNDARY_COMPUTE:
    if(stardis->mode & MODE_EXTENDED_RESULTS) {
      if(stardis->time_range[0] == stardis->time_range[1])
        fprintf(stream, "Temperature at boundary '%s' at t=%g = %g K +/- %g\n",
          str_cget(&stardis->solve_name), stardis->time_range[0],
          result.E, /* Expected value */
          result.SE); /* Standard error */
      else
        fprintf(stream,
          "Temperature at boundary '%s' with t in [%g %g] = %g K +/- %g\n",
          str_cget(&stardis->solve_name), SPLIT2(stardis->time_range),
          result.E, /* Expected value */
          result.SE); /* Standard error */
    }
    else fprintf(stream, "%g %g %lu %lu\n",
      result.E, result.SE, nfailures, nsamples);
    break;
  case MODE_FLUX_BOUNDARY_COMPUTE: {
    enum sdis_estimator_type type;
    ERR(sdis_estimator_get_type(estimator, &type));
    ASSERT(type == SDIS_ESTIMATOR_FLUX);

    if(stardis->mode & MODE_EXTENDED_RESULTS) {
      if(stardis->time_range[0] == stardis->time_range[1])
        fprintf(stream, "Temperature at boundary '%s' at t=%g = %g K +/- %g\n",
          str_cget(&stardis->solve_name), stardis->time_range[0],
          result.E, /* Expected value */
          result.SE); /* Standard error */
      else
        fprintf(stream,
          "Temperature at boundary '%s' with t in [%g %g] = %g K +/- %g\n",
          str_cget(&stardis->solve_name), SPLIT2(stardis->time_range),
          result.E, /* Expected value */
          result.SE); /* Standard error */
      ERR(sdis_estimator_get_convective_flux(estimator, &result));
      if(stardis->time_range[0] == stardis->time_range[1])
        fprintf(stream, "Convective flux at boundary '%s' at t=%g = %g W +/- %g\n",
          str_cget(&stardis->solve_name), stardis->time_range[0],
          stardis->compute_surface.area * result.E, /* Expected value */
          stardis->compute_surface.area * result.SE); /* Standard error */
      else
        fprintf(stream,
          "Convective flux at boundary '%s' with t in [%g %g] = %g W +/- %g\n",
          str_cget(&stardis->solve_name), SPLIT2(stardis->time_range),
          stardis->compute_surface.area * result.E, /* Expected value */
          stardis->compute_surface.area * result.SE); /* Standard error */
      ERR(sdis_estimator_get_radiative_flux(estimator, &result));
      if(stardis->time_range[0] == stardis->time_range[1])
        fprintf(stream, "Radiative flux at boundary '%s' at t=%g = %g W +/- %g\n",
          str_cget(&stardis->solve_name), stardis->time_range[0],
          stardis->compute_surface.area * result.E, /* Expected value */
          stardis->compute_surface.area * result.SE); /* Standard error */
      else
        fprintf(stream,
          "Radiative flux at boundary '%s' with t in [%g %g] = %g W +/- %g\n",
          str_cget(&stardis->solve_name), SPLIT2(stardis->time_range),
          stardis->compute_surface.area * result.E, /* Expected value */
          stardis->compute_surface.area * result.SE); /* Standard error */
      ERR(sdis_estimator_get_imposed_flux(estimator, &result));
      if(stardis->time_range[0] == stardis->time_range[1])
        fprintf(stream, "Imposed flux at boundary '%s' at t=%g = %g W +/- %g\n",
          str_cget(&stardis->solve_name), stardis->time_range[0],
          stardis->compute_surface.area * result.E, /* Expected value */
          stardis->compute_surface.area * result.SE); /* Standard error */
      else
        fprintf(stream,
          "Imposed flux at boundary '%s' with t in [%g %g] = %g W +/- %g\n",
          str_cget(&stardis->solve_name), SPLIT2(stardis->time_range),
          stardis->compute_surface.area * result.E, /* Expected value */
          stardis->compute_surface.area * result.SE); /* Standard error */
      ERR(sdis_estimator_get_total_flux(estimator, &result));
      if(stardis->time_range[0] == stardis->time_range[1])
        fprintf(stream, "Total flux Flux at boundary '%s' at t=%g = %g W +/- %g\n",
          str_cget(&stardis->solve_name), stardis->time_range[0],
          stardis->compute_surface.area * result.E, /* Expected value */
          stardis->compute_surface.area * result.SE); /* Standard error */
      else
        fprintf(stream,
          "Total flux Flux at boundary '%s' with t in [%g %g] = %g W +/- %g\n",
          str_cget(&stardis->solve_name), SPLIT2(stardis->time_range),
          stardis->compute_surface.area * result.E, /* Expected value */
          stardis->compute_surface.area * result.SE); /* Standard error */
    } else {
      fprintf(stream, "%g %g ", result.E, result.SE); /* Temperature */
      ERR(sdis_estimator_get_convective_flux(estimator, &result));
      fprintf(stream, "%g %g ",
        stardis->compute_surface.area * result.E,
        stardis->compute_surface.area * result.SE);
      ERR(sdis_estimator_get_radiative_flux(estimator, &result));
      fprintf(stream, "%g %g ",
        stardis->compute_surface.area * result.E,
        stardis->compute_surface.area * result.SE);
      ERR(sdis_estimator_get_imposed_flux(estimator, &result));
      fprintf(stream, "%g %g ",
        stardis->compute_surface.area * result.E,
        stardis->compute_surface.area * result.SE);
      ERR(sdis_estimator_get_total_flux(estimator, &result));
      fprintf(stream, "%g %g ",
        stardis->compute_surface.area * result.E,
        stardis->compute_surface.area * result.SE);
      fprintf(stream, "%lu %lu\n", nfailures, nsamples);
    }
    break;
  }
  default: FATAL("Invalid mode.");
  }
  if(stardis->mode & MODE_EXTENDED_RESULTS)
    fprintf(stream, "#failures: %lu/%lu\n", nfailures, nsamples);
  if(nfailures)
    logger_print(stardis->logger, LOG_ERROR,
      "#failures: %lu/%lu\n", nfailures, nsamples);

end:
  return res;
error:
  goto end;
}

void
dump_map
  (const struct stardis* stardis,
   const struct darray_estimators* estimators,
   FILE* stream)
{
  unsigned i, vcount, tcount, last_v = 0;
  const size_t* idx;
  size_t sz;
  unsigned szp;
  struct sdis_estimator* const* est;

  ASSERT(stardis && estimators && stream);

  est = darray_estimators_cdata_get(estimators);
  idx = darray_size_t_cdata_get(&stardis->compute_surface.primitives);
  sz = darray_size_t_size_get(&stardis->compute_surface.primitives);
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
  fprintf(stream, "# vtk DataFile Version 2.0\n");
  fprintf(stream, "Temperature Map\n");
  fprintf(stream, "ASCII\n");
  fprintf(stream, "DATASET POLYDATA\n");
  fprintf(stream, "POINTS %u float\n\n", last_v + 1);
  for(i = 0; i <= last_v; ++i) {
    double coord[3];
    SG3D(geometry_get_unique_vertex(stardis->geometry.sg3d, i, coord));
    fprintf(stream, "%f %f %f\n", SPLIT3(coord));
  }
  /* Dump only primitives in boundary */
  fprintf(stream, "\nPOLYGONS %u %u\n", szp, 4 * szp);
  for(i = 0; i < szp; ++i) {
    unsigned t;
    unsigned indices[3];
    ASSERT(idx[i] <= UINT_MAX);
    t = (unsigned)idx[i];
    SG3D(geometry_get_unique_triangle_vertices(stardis->geometry.sg3d, t,
      indices));
    fprintf(stream, "3 %u %u %u\n", SPLIT3(indices));
  }
  fprintf(stream, "\nCELL_DATA %u\n", szp);
  fprintf(stream, "SCALARS temperature_estimate float 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  for(i = 0; i < szp; ++i) {
    struct sdis_mc T;
    SDIS(estimator_get_temperature(est[i], &T));
    fprintf(stream, "%f\n", T.E);
  }
  fprintf(stream, "SCALARS temperature_std_dev float 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  for(i = 0; i < szp; ++i) {
    struct sdis_mc T;
    SDIS(estimator_get_temperature(est[i], &T));
    fprintf(stream, "%f\n", T.SE);
  }
  fprintf(stream, "SCALARS failures_count unsigned_long_long 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  for(i = 0; i < szp; ++i) {
    size_t nfails;
    SDIS(estimator_get_failure_count(est[i], &nfails));
    ASSERT(nfails <= UINT_MAX);
    fprintf(stream, "%u\n", (unsigned)nfails);
  }
  fprintf(stream, "SCALARS computation_time_estimate float 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  for(i = 0; i < szp; ++i) {
    struct sdis_mc time;
    SDIS(estimator_get_realisation_time(est[i], &time));
    fprintf(stream, "%f\n", time.E);
  }
  fprintf(stream, "SCALARS computation_time_std_dev float 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  for(i = 0; i < szp; ++i) {
    struct sdis_mc time;
    SDIS(estimator_get_realisation_time(est[i], &time));
    fprintf(stream, "%f\n", time.SE);
  }
}

res_T
dump_compute_region_at_the_end_of_vtk
  (struct stardis* stardis,
   FILE* stream)
{
  res_T res = RES_OK;
  unsigned char* v = NULL;
  unsigned tsz, i;
  size_t j, psz;
  ASSERT(stardis && stream);
  psz = darray_size_t_size_get(&stardis->compute_surface.primitives);
  ASSERT(psz == darray_sides_size_get(&stardis->compute_surface.sides));

  ERR(sg3d_geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tsz));
  /* For triangles not in compute region v==0 */
  v = MEM_CALLOC(stardis->allocator, tsz, sizeof(*v));
  if(!v) {
    res = RES_MEM_ERR;
    goto error;
  }

  if(stardis->mode & SURFACE_COMPUTE_MODES) {
    /* For triangles in compute surface, v==1 if FRONT or v==2 for BACK */
    FOR_EACH(j, 0, psz) {
      size_t prim
        = darray_size_t_cdata_get(&stardis->compute_surface.primitives)[j];
      enum sdis_side side
        = darray_sides_cdata_get(&stardis->compute_surface.sides)[j];
      ASSERT(prim <= tsz);
      v[(unsigned)prim] =
        (unsigned char)(v[(unsigned)prim] | (side == SDIS_FRONT ? 1 : 2));
    }

    /* For triangles in compute surface with error v==MAX */
    FOR_EACH(j, 0, darray_uint_size_get(&stardis->compute_surface.err_triangles))
    {
      unsigned prim
        = darray_uint_cdata_get(&stardis->compute_surface.err_triangles)[j];
      ASSERT(prim <= tsz);
      v[(unsigned)prim] = UCHAR_MAX;
    }
  } else {
    unsigned descr[SG3D_PROP_TYPES_COUNT__];
    struct sdis_medium* medium;
    const struct description* descriptions;
    unsigned medium_id;
    ASSERT(stardis->mode & MODE_MEDIUM_COMPUTE);
    medium = find_medium_by_name(stardis, &stardis->solve_name, &medium_id);
    ASSERT(medium != NULL); (void)medium;
    descriptions = darray_descriptions_cdata_get(&stardis->descriptions);
    FOR_EACH(i, 0, tsz) {
      unsigned f_mid, b_mid;
      /* Get the description IDs for this triangle */
      ERR(sg3d_geometry_get_unique_triangle_properties(stardis->geometry.sg3d,
        i, descr));
      /* For triangles in compute volume,
       * v==1 if FRONT, v==2 for BACK */
      if(descr[SG3D_FRONT] == SG3D_UNSPECIFIED_PROPERTY)
        f_mid = UINT_MAX;
      else description_get_medium_id(descriptions + descr[SG3D_FRONT], &f_mid);
      if(descr[SG3D_BACK] == SG3D_UNSPECIFIED_PROPERTY)
        b_mid = UINT_MAX;
      else description_get_medium_id(descriptions + descr[SG3D_BACK], &b_mid);
      if(f_mid == medium_id && b_mid == medium_id)
        ; /* Keep v==0, not really a boundary */
      else if(f_mid == medium_id)
        v[i] = 1;
      else if(b_mid == medium_id)
        v[i] = 2;
    }
  }

  fprintf(stream, "SCALARS Compute_region unsigned_int 1\n");
  fprintf(stream, "LOOKUP_TABLE default\n");
  FOR_EACH(i, 0, tsz)
    fprintf(stream, "%u\n", v[i] == UCHAR_MAX ? UINT_MAX : v[i]);

exit:
  MEM_RM(stardis->allocator, v);
  return res;
error:
  goto exit;
}

res_T
dump_model_as_c_chunks
  (struct stardis* stardis,
   FILE* stream)
{
  res_T res = RES_OK;
  const char* prefix;
  unsigned n, vcount, tcount;

  ASSERT(stardis && stream);

  prefix = str_cget(&stardis->chunks_prefix);
  ERR(sg3d_geometry_get_unique_vertices_count(stardis->geometry.sg3d, &vcount));
  ERR(sg3d_geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tcount));

  fprintf(stream, "#define %s_UNSPECIFIED_PROPERTY %u\n\n",
    prefix, SG3D_UNSPECIFIED_PROPERTY);

  fprintf(stream, "static const unsigned\n");
  fprintf(stream, "%s_vertices_count = %u;\n\n", prefix, vcount);

  fprintf(stream, "static const unsigned\n");
  fprintf(stream, "%s_triangles_count = %u;\n\n", prefix, tcount);

  fprintf(stream, "static const double\n");
  fprintf(stream, "%s_vertices[%u][3] = {\n", prefix, vcount);
  for(n = 0; n < vcount; n++) {
    double vertex[3];
    ERR(sg3d_geometry_get_unique_vertex(stardis->geometry.sg3d, n,
      vertex));
    fprintf(stream, "   { %g, %g, %g }%c\n",
      SPLIT3(vertex), (n == vcount - 1 ? ' ' : ','));
  }
  fprintf(stream, "};\n\n");

  fprintf(stream, "static const unsigned\n");
  fprintf(stream, "%s_triangles[%u][3] = {\n", prefix, tcount);
  for(n = 0; n < tcount; n++) {
    unsigned triangle[3];
    ERR(sg3d_geometry_get_unique_triangle_vertices(stardis->geometry.sg3d, n,
      triangle));
    fprintf(stream, "   { %u, %u, %u }%c\n",
      SPLIT3(triangle), (n == tcount - 1 ? ' ' : ','));
  }
  fprintf(stream, "};\n\n");

  fprintf(stream, "static const unsigned\n");
  fprintf(stream, "%s_properties[%u][3] = {\n", prefix, tcount);
  for(n = 0; n < tcount; n++) {
    unsigned properties[SG3D_PROP_TYPES_COUNT__];
    ERR(sg3d_geometry_get_unique_triangle_properties(stardis->geometry.sg3d, n,
      properties));
    if(properties[0] == SG3D_UNSPECIFIED_PROPERTY)
      fprintf(stream, "   { %s_UNSPECIFIED_PROPERTY, ", prefix);
    else fprintf(stream, "   { %u, ", properties[0]);
    if(properties[1] == SG3D_UNSPECIFIED_PROPERTY)
      fprintf(stream, "%s_UNSPECIFIED_PROPERTY, ", prefix);
    else fprintf(stream, "%u, ", properties[1]);
    if(properties[2] == SG3D_UNSPECIFIED_PROPERTY)
      fprintf(stream, "%s_UNSPECIFIED_PROPERTY }", prefix);
    else fprintf(stream, "%u }", properties[2]);
    if(n == tcount - 1) fprintf(stream, "\n"); else fprintf(stream, ",\n");
  }
  fprintf(stream, "};\n\n");

exit:
  return res;
error:
  goto exit;
}

res_T
write_random_generator_state
  (struct sdis_estimator* estimator,
   FILE* stream)
{
  res_T res;
  struct ssp_rng* state;
  res = sdis_estimator_get_rng_state(estimator, &state);
  if(res != RES_OK) return res;
  return ssp_rng_write(state, stream);
}

res_T
read_random_generator_state
  (struct ssp_rng* state,
   FILE* stream)
{
  return ssp_rng_read(state, stream);
}
