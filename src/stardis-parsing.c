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

#define _POSIX_C_SOURCE 200809L /* strdup */
#include "stardis-parsing.h"
#include "stardis-app.h"
#include "stardis-default.h"
#include "stardis-version.h"

#include <rsys/cstr.h>
#include <rsys/double2.h>
#include <rsys/double3.h>
#include <sdis_version.h>
#include <rsys/logger.h>

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef COMPILER_GCC
#include <strings.h> /* strcasecmp */
#else
#define strcasecmp(s1, s2) _stricmp((s1), (s2))
#define strdup(tok) _strdup(tok)
#define strtok_r(str, delim, save) strtok_s((str), (delim), (save))
#endif

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

void
add_geom_ctx_indices
  (const unsigned itri,
   unsigned ids[3],
   void* context)
{
  const struct add_geom_ctx* ctx = context;
  const unsigned* trg;
  int i;
  ASSERT(ids && ctx);
  ASSERT(itri < ctx->stl_desc.triangles_count);
  trg = ctx->stl_desc.indices + 3 * itri;
  for(i = 0; i < 3; i++) ids[i] = trg[i];
}

void
add_geom_ctx_properties
  (const unsigned itri,
   unsigned prop[3],
   void* context)
{
  const struct add_geom_ctx* ctx = context;
  int i;
  ASSERT(prop && ctx); (void)itri;
  ASSERT(itri < ctx->stl_desc.triangles_count);
  /* Same media for the whole add_geometry set of triangles */
  for(i = 0; i < SG3D_PROP_TYPES_COUNT__; i++) prop[i] = ctx->properties[i];
}

void
add_geom_ctx_position
  (const unsigned ivert,
   double pos[3],
   void* context)
{
  const struct add_geom_ctx* ctx = context;
  const float* v;
  ASSERT(pos && ctx);
  ASSERT(ivert < ctx->stl_desc.vertices_count);
  v = ctx->stl_desc.vertices + 3 * ivert;
  d3_set_f3(pos, v);
}

static res_T
add_geom_keep_degenerated
  (const unsigned itri,
   void* context,
   int* abort)
{
  const struct add_geom_ctx* ctx = context;
  struct darray_uint* degenerated;

  ASSERT(abort && ctx && ctx->custom); (void)abort;
  ASSERT(itri < ctx->stl_desc.triangles_count);
  degenerated = ctx->custom;
  return darray_uint_push_back(degenerated, &itri);
}

static res_T
read_sides_and_files
  (struct stardis* stardis,
   const int descr_is_intface, /* if 1, don't read side */
   const unsigned description_id,
   char** tok_ctx)
{
  char* tk = NULL;
  int file_count = 0;
  struct sstl* sstl = NULL;
  struct add_geom_ctx add_geom_ctx;
  unsigned current_merge_errors;
  struct sg3d_geometry_add_callbacks callbacks = SG3D_ADD_CALLBACKS_NULL__;
  struct darray_uint degenerated;
  struct str str;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  darray_uint_init(stardis->allocator, &degenerated);
  str_init(stardis->allocator, &str);
  callbacks.get_indices = add_geom_ctx_indices;
  callbacks.get_properties = add_geom_ctx_properties;
  callbacks.get_position = add_geom_ctx_position;
  callbacks.degenerated_triangle = add_geom_keep_degenerated;
  add_geom_ctx.custom = &degenerated;

  ERR(sg3d_geometry_get_unique_triangles_with_merge_conflict_count(
    stardis->geometry.sg3d, &current_merge_errors));

  /* At least one side+name, no side without name */
  ERR(sstl_create(stardis->logger, stardis->allocator, 1, &sstl));
  for(;;) {
    unsigned merge_errors;
    if(descr_is_intface) {
      add_geom_ctx.properties[SG3D_FRONT] = SG3D_UNSPECIFIED_PROPERTY;
      add_geom_ctx.properties[SG3D_BACK] = SG3D_UNSPECIFIED_PROPERTY;
      add_geom_ctx.properties[SG3D_INTFACE] = description_id;
    } else {
      tk = strtok_r(NULL, " \t", tok_ctx);
      if(!tk) {
        if(file_count == 0) {
          /* At least 1 side */
          logger_print(stardis->logger, LOG_ERROR,
            "Invalid data (missing token 'side')\n");
          res = RES_BAD_ARG;
          goto error;
        }
        else break;
      }
      add_geom_ctx.properties[SG3D_INTFACE] = SG3D_UNSPECIFIED_PROPERTY;
      if(0 == strcasecmp(tk, "FRONT")) {
        add_geom_ctx.properties[SG3D_FRONT] = description_id;
        add_geom_ctx.properties[SG3D_BACK] = SG3D_UNSPECIFIED_PROPERTY;
      }
      else if(0 == strcasecmp(tk, "BACK")) {
        add_geom_ctx.properties[SG3D_FRONT] = SG3D_UNSPECIFIED_PROPERTY;
        add_geom_ctx.properties[SG3D_BACK] = description_id;
      }
      else if(0 == strcasecmp(tk, "BOTH")) {
        add_geom_ctx.properties[SG3D_FRONT] = description_id;
        add_geom_ctx.properties[SG3D_BACK] = description_id;
      }
      else {
        logger_print(stardis->logger, LOG_ERROR,
          "Invalid side specifier: %s\n", tk);
        res = RES_BAD_ARG;
        goto error;
      }
    }
    tk = strtok_r(NULL, " \t", tok_ctx);
    if(!tk) {
      if(!descr_is_intface /* Has read a side */
        || !file_count) /* Need at least 1 file */
      {
        logger_print(stardis->logger, LOG_ERROR,
          "Invalid data (missing token 'file name')\n");
        res = RES_BAD_ARG;
        goto error;
      }
      else break;
    }
    file_count++;
    res = sstl_load(sstl, tk);
    if(res != RES_OK) {
      logger_print(stardis->logger, LOG_ERROR,
        "Cannot read STL file: '%s'\n", tk);
      goto error;
    }
    ERR(sstl_get_desc(sstl, &add_geom_ctx.stl_desc));
    ASSERT(add_geom_ctx.stl_desc.vertices_count <= UINT_MAX
      && add_geom_ctx.stl_desc.triangles_count <= UINT_MAX);

    res = sg3d_geometry_add(
      stardis->geometry.sg3d,
      (unsigned)add_geom_ctx.stl_desc.vertices_count,
      (unsigned)add_geom_ctx.stl_desc.triangles_count,
      &callbacks,
      &add_geom_ctx);
    if(darray_uint_size_get(&degenerated)) {
      size_t c, n;
      const unsigned* ids = darray_uint_cdata_get(&degenerated);
      c = darray_uint_size_get(&degenerated);
      ASSERT(c <= ULONG_MAX);
      logger_print(stardis->logger, LOG_WARNING,
        "File '%s' included %lu degenerated triangles (removed)\n",
        tk, (unsigned long)c);
      ERR(str_printf(&str, "Degenerated triangles IDs: %u", ids[0]));
      FOR_EACH(n, 1, c) { STR_APPEND_PRINTF(&str, ", %u", ARG1( ids[n] ) ); }
      logger_print(stardis->logger, LOG_OUTPUT, "%s\n", str_cget(&str));
      darray_uint_clear(&degenerated);
    }

    if(res != RES_OK) {
      logger_print(stardis->logger, LOG_ERROR,
        "Cannot add file content: '%s'\n", tk);
      goto error;
    }
    /* Check conflicts */
    ERR(sg3d_geometry_get_unique_triangles_with_merge_conflict_count(
      stardis->geometry.sg3d, &merge_errors));
    if(current_merge_errors != merge_errors) {
      int is_for_compute =
        (stardis->mode & COMPUTE_MODES) && !(stardis->mode & MODE_DUMP_VTK);
      logger_print(stardis->logger, (is_for_compute ? LOG_ERROR : LOG_WARNING),
        "Merge conflicts found reading file '%s' (%u triangles).\n",
        tk, merge_errors - current_merge_errors);
      if(is_for_compute) {
        res = RES_BAD_ARG;
        goto error;
      }
    }
    current_merge_errors = merge_errors;
  }

end:
  darray_uint_release(&degenerated);
  str_release(&str);
  if(sstl) SSTL(ref_put(sstl));
  return res;
error:
  goto end;
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

static res_T
description_set_name
  (struct stardis* stardis,
   struct str* name,
   const char* tk)
{
  res_T res = RES_OK;
  double foo;
  const char* keywords[] = {
    "AUTO", "BACK", "BOTH", "FLUID", "FRONT", "F_BOUNDARY_FOR_SOLID",
    "H_BOUNDARY_FOR_FLUID", "H_BOUNDARY_FOR_SOLID", "SCALE", "SOLID",
    "SOLID_FLUID_CONNECTION", "SOLID_SOLID_CONNECTION", "TRAD",
    "T_BOUNDARY_FOR_SOLID", "UNKNOWN" };
  int i;
  ASSERT(name && tk);

  /* Use name before uppercasing it */
  ERR(str_set(name, tk));

  if(RES_OK == cstr_to_double(tk, &foo)) {
    /* A number is not a sensible choice for a name! */
    res = RES_BAD_ARG;
    goto error;
  }
  FOR_EACH(i, 0, sizeof(keywords) / sizeof(*keywords)) {
    if(0 == strcasecmp(tk, keywords[i])) {
      /* A keyword is not a sensible choice for a name! */
      res = RES_BAD_ARG;
      goto error;
    }
  }
  /* Name is OK */

end:
  return res;
error:
  logger_print(stardis->logger, LOG_ERROR, "Invalid name: %s\n", tk);
  goto end;
}

static struct description*
find_description_by_name
  (struct stardis* stardis,
   const struct str* name,
   const struct description* self)
{
  size_t i;
  ASSERT(stardis && name && self);

  FOR_EACH(i, 0, darray_descriptions_size_get(&stardis->descriptions)) {
    struct description* desc
      = darray_descriptions_data_get(&stardis->descriptions) + i;
    if(self == desc) continue;
    if(str_eq(name, get_description_name(desc))) {
      return desc;
    }
  }
  return NULL;
}

/* H_BOUNDARY_FOR_SOLID Name ref_temperature emissivity specular_fraction hc T_env STL_filenames
 * H_BOUNDARY_FOR_FLUID Name ref_temperature emissivity specular_fraction hc T_env STL_filenames */
static res_T
process_h
  (struct stardis* stardis,
   const enum description_type type,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  struct h_boundary* h_boundary;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  stardis->counts.hbound_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz+1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_h(stardis->allocator, &desc->d.h_boundary));
  h_boundary = desc->d.h_boundary;
  desc->type = type;

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "h boundary name");
  ERR(description_set_name(stardis, &h_boundary->name, tk));
  if(find_description_by_name(stardis, &h_boundary->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "ref_temperature");
  res = cstr_to_double(tk, &h_boundary->ref_temperature);
  if(res != RES_OK
    || h_boundary->ref_temperature < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid reference temperature: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], h_boundary->ref_temperature);
  stardis->t_range[1] = MMAX(stardis->t_range[1], h_boundary->ref_temperature);
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "emissivity");
  res = cstr_to_double(tk, &h_boundary->emissivity);
  if(res != RES_OK
    || h_boundary->emissivity < 0
    || h_boundary->emissivity > 1)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid emissivity: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "specular fraction");
  res = cstr_to_double(tk, &h_boundary->specular_fraction);
  if(res != RES_OK
    || h_boundary->specular_fraction < 0
    || h_boundary->specular_fraction > 1)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid specular fraction: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "hc");
  res = cstr_to_double(tk, &h_boundary->hc);
  if(res != RES_OK
    || h_boundary->hc < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid hc: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "temperature");
  res = cstr_to_double(tk, &h_boundary->imposed_temperature);
  if(res != RES_OK
    || h_boundary->imposed_temperature < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid temperature: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], h_boundary->imposed_temperature);
  stardis->t_range[1] = MMAX(stardis->t_range[1], h_boundary->imposed_temperature);

  if(type == DESC_BOUND_H_FOR_FLUID)
    ERR(get_dummy_solid_id(stardis, &h_boundary->mat_id));
  else {
    struct fluid* fluid = NULL;
    ERR(init_fluid(stardis->allocator, &fluid));
    fluid->fluid_id = allocate_stardis_medium_id(stardis);
    h_boundary->mat_id = fluid->fluid_id;
    h_boundary->possible_external_fluid = fluid;
    ASSERT(sz <= UINT_MAX);
    fluid->desc_id = (unsigned)sz;
    fluid->imposed_temperature
      = h_boundary->imposed_temperature;
    fluid->is_outside = 1;
    fluid->is_green = stardis->mode & (MODE_BIN_GREEN | MODE_GREEN);
    ERR(create_solver_fluid(stardis, fluid));
    logger_print(stardis->logger, LOG_OUTPUT,
      "External fluid created: T=%g (it is medium %u)\n",
      fluid->imposed_temperature,
      fluid->fluid_id);
  }

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* T_BOUNDARY_FOR_SOLID Name T STL_filenames */
static res_T
process_t
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  struct t_boundary* t_boundary;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  stardis->counts.tbound_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_t(stardis->allocator, &desc->d.t_boundary));
  t_boundary = desc->d.t_boundary;
  desc->type = DESC_BOUND_T_FOR_SOLID;

  ERR(get_dummy_fluid_id(stardis, &t_boundary->mat_id));

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "temperature boundary name");
  ERR(description_set_name(stardis, &t_boundary->name, tk));
  if(find_description_by_name(stardis, &t_boundary->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "temperature");
  res = cstr_to_double(tk, &t_boundary->imposed_temperature);
  if(res != RES_OK
    || t_boundary->imposed_temperature < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid temperature: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], t_boundary->imposed_temperature);
  stardis->t_range[1] = MMAX(stardis->t_range[1], t_boundary->imposed_temperature);

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* F_BOUNDARY_FOR_SOLID Name F STL_filenames */
static res_T
process_flx
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  struct f_boundary* f_boundary;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  stardis->counts.fbound_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_f(stardis->allocator, &desc->d.f_boundary));
  f_boundary = desc->d.f_boundary;
  desc->type = DESC_BOUND_F_FOR_SOLID;

  ERR(get_dummy_fluid_id(stardis, &f_boundary->mat_id));

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "flux boundary name");
  ERR(description_set_name(stardis, &f_boundary->name, tk));
  if(find_description_by_name(stardis, &f_boundary->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "flux");
  res = cstr_to_double(tk, &f_boundary->imposed_flux);
  if(res != RES_OK
    || f_boundary->imposed_flux == SDIS_FLUX_NONE) {
    /* Flux can be < 0 but not undefined */
    if(res == RES_OK) res = RES_BAD_ARG;
    logger_print(stardis->logger, LOG_ERROR, "Invalid flux: %s\n", tk);
    goto end;
  }

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* SOLID_FLUID_CONNECTION Name ref_temperature emissivity specular_fraction hc STL_filenames */
static res_T
process_sfc
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  struct solid_fluid_connect* sf_connect;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  stardis->counts.sfconnect_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_sf(stardis->allocator, &desc->d.sf_connect));
  sf_connect = desc->d.sf_connect;
  desc->type = DESC_SOLID_FLUID_CONNECT;

  /* Use a medium ID even if there is no medium here
   * As other cases use media IDs as unique IDs for read_sides_and_files calls
   * we continue the trend to ensure connection ID is OK */
  sf_connect->connection_id = allocate_stardis_medium_id(stardis);

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "solid fluid connection name");
  ERR(description_set_name(stardis, &sf_connect->name, tk));
  if(find_description_by_name(stardis, &sf_connect->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "ref_temperature");
  res = cstr_to_double(tk, &sf_connect->ref_temperature);
  if(res != RES_OK
    || sf_connect->ref_temperature < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid reference temperature: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], sf_connect->ref_temperature);
  stardis->t_range[1] = MMAX(stardis->t_range[1], sf_connect->ref_temperature);
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "emissivity");
  res = cstr_to_double(tk, &sf_connect->emissivity);
  if(res != RES_OK
    || sf_connect->emissivity < 0
    || sf_connect->emissivity > 1)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid emissivity: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "specular fraction");
  res = cstr_to_double(tk, &sf_connect->specular_fraction);
  if(res != RES_OK
    || sf_connect->specular_fraction < 0
    || sf_connect->specular_fraction > 1)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid specular fraction: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "hc");
  res = cstr_to_double(tk, &sf_connect->hc);
  if(res != RES_OK
    || sf_connect->hc < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid hc: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* SOLID_SOLID_CONNECTION Name contact-resitance STL_filenames */
static res_T
process_ssc
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  struct solid_solid_connect* ss_connect;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  stardis->counts.ssconnect_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_ss(stardis->allocator, &desc->d.ss_connect));
  ss_connect = desc->d.ss_connect;
  desc->type = DESC_SOLID_SOLID_CONNECT;

  /* Use a medium ID even if there is no medium here
   * As other cases use media IDs as unique IDs for read_sides_and_files calls
   * we continue the trend to ensure connection ID is OK */
  ss_connect->connection_id = allocate_stardis_medium_id(stardis);

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "solid solid connection name");
  ERR(description_set_name(stardis, &ss_connect->name, tk));
  if(find_description_by_name(stardis, &ss_connect->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "contact resistance");
  res = cstr_to_double(tk, &ss_connect->tcr);
  if(res != RES_OK
    || ss_connect->tcr < 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid contact resistance: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  else if(ss_connect->tcr == 0) {
    logger_print(stardis->logger, LOG_WARNING,
      "Solid-solid connection %s: defining a contact resistance to 0 has "
      "no effect\n", str_cget(&ss_connect->name));
  }

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, tok_ctx));

end:
  return res;
error:
  goto end;
}

static res_T
read_imposed_temperature
  (struct stardis* stardis,
   double* imposed_temperature,
   char** tok_ctx)
{
  char* tk = NULL;
  struct str keep;
  res_T res = RES_OK;
  ASSERT(stardis && imposed_temperature && tok_ctx);

  str_init(stardis->allocator, &keep);
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "imposed temperature");
  ERR(str_set(&keep, tk));
  if(RES_OK == cstr_to_double(tk, imposed_temperature)) {
    /* Was a number */
    if(*imposed_temperature < 0) {
      res = RES_BAD_ARG;
      goto error;
    }
  } else {
    /* Could be 'unknown' */
    if(0 == strcasecmp(tk, "UNKNOWN")) {
      *imposed_temperature = UNKNOWN_MEDIUM_TEMPERATURE;
    } else {
      res = RES_BAD_ARG;
      goto error;
    }
  }
end:
  str_release(&keep);
  return res;
error:
  logger_print(stardis->logger, LOG_ERROR, "Invalid imposed temperature: %s\n",
    str_cget(&keep));
  goto end;
}

static res_T
read_delta
  (struct stardis* stardis,
   double* delta,
   char** tok_ctx)
{
  char* tk = NULL;
  struct str keep;
  res_T res = RES_OK;
  ASSERT(stardis && delta && tok_ctx);

  str_init(stardis->allocator, &keep);
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "delta");
  ERR(str_set(&keep, tk));
  if(RES_OK == cstr_to_double(tk, delta)) {
    /* Was a number */
    if(*delta <= 0) {
      res = RES_BAD_ARG;
      goto error;
    }
  } else {
    /* Could be 'auto' */ 
    if(0 == strcasecmp(tk, "AUTO")) {
      /* Set to DELTA_AUTO until actual value is substituted */
      *delta = DELTA_AUTO;
    } else {
      res = RES_BAD_ARG;
      goto error;
    }
  }
end:
  str_release(&keep);
  return res;
error:
  logger_print(stardis->logger, LOG_ERROR, "Invalid delta: %s\n",
    str_cget(&keep));
  goto end;
}

/* SOLID Name lambda rho cp delta Tinit Timposed volumic_power STL_filenames */
static res_T
process_solid
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  struct solid* solid;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  stardis->counts.smed_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_solid(stardis->allocator, &desc->d.solid));
  solid = desc->d.solid;
  desc->type = DESC_MAT_SOLID;

  solid->solid_id = allocate_stardis_medium_id(stardis);
  solid->is_green = stardis->mode & (MODE_BIN_GREEN | MODE_GREEN);
  solid->is_outside = 0;
  ASSERT(sz <= UINT_MAX);
  solid->desc_id = (unsigned)sz;
  
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "solid name");
  ERR(description_set_name(stardis, &solid->name, tk));
  if(find_description_by_name(stardis, &solid->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "lambda");
  res = cstr_to_double(tk, &solid->lambda);
  if(res != RES_OK
    || solid->lambda <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid lambda: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "rho");
  res = cstr_to_double(tk, &solid->rho);
  if(res != RES_OK
    || solid->rho <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid rho: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "cp");
  res = cstr_to_double(tk, &solid->cp);
  if(res != RES_OK
    || solid->cp <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid cp: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  ERR(read_delta(stardis, &solid->delta, tok_ctx));
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "Tinit");
  res = cstr_to_double(tk, &solid->tinit);
  if(res != RES_OK
    || solid->tinit < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid Tinit: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], solid->tinit);
  stardis->t_range[1] = MMAX(stardis->t_range[1], solid->tinit);
  ERR(read_imposed_temperature(stardis, &solid->imposed_temperature,
    tok_ctx));
  if(solid->imposed_temperature >= 0
    && solid->imposed_temperature != solid->tinit)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Imposed temperature, if defined, must match initial temperature "
      "(initial: %g; imposed: %g)\n",
      solid->tinit, solid->imposed_temperature);
    res = RES_BAD_ARG;
    goto end;
  }
  if(solid->imposed_temperature >= 0)
    stardis->t_range[0] = MMIN(stardis->t_range[0], solid->imposed_temperature);
  stardis->t_range[1] = MMAX(stardis->t_range[1], solid->imposed_temperature);
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "volumic power");
  res = cstr_to_double(tk, &solid->vpower);
  if(res != RES_OK) {
    /* VPower can be < 0 */
    logger_print(stardis->logger, LOG_ERROR, "Invalid volumic power: %s\n", tk);
    goto end;
  }

  /* Actual solid creation is defered until geometry is read to allow
   * enclosure shape VS delta analysis (and auto delta computation) */

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 0, (unsigned)sz, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* FLUID Name rho cp Tinit Timposed STL_filenames */
static res_T
process_fluid
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  struct fluid* fluid;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  stardis->counts.fmed_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_fluid(stardis->allocator, &desc->d.fluid));
  fluid = desc->d.fluid;
  desc->type = DESC_MAT_FLUID;

  fluid->fluid_id = allocate_stardis_medium_id(stardis);
  fluid->is_outside = 0;
  fluid->is_green = stardis->mode & (MODE_BIN_GREEN | MODE_GREEN);
  ASSERT(sz <= UINT_MAX);
  fluid->desc_id = (unsigned)sz;
  
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "fluid name");
  ERR(description_set_name(stardis, &fluid->name, tk));
  if(find_description_by_name(stardis, &fluid->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "rho");
  res = cstr_to_double(tk, &fluid->rho);
  if(res != RES_OK
    || fluid->rho <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid rho: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "cp");
  res = cstr_to_double(tk, &fluid->cp);
  if(res != RES_OK
    || fluid->cp <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid cp: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "Tinit");
  res = cstr_to_double(tk, &fluid->tinit);
  if(res != RES_OK
    || fluid->tinit < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid Tinit: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], fluid->tinit);
  stardis->t_range[1] = MMAX(stardis->t_range[1], fluid->tinit);
  ERR(read_imposed_temperature(stardis, &fluid->imposed_temperature,
    tok_ctx));
  if(fluid->imposed_temperature >= 0
    && fluid->imposed_temperature != fluid->tinit)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Imposed temperature, if defined, must match initial temperature "
      "(initial: %g; imposed: %g)\n",
      fluid->tinit, fluid->imposed_temperature);
    res = RES_BAD_ARG;
    goto end;
  }
  if(fluid->imposed_temperature >= 0)
    stardis->t_range[0] = MMIN(stardis->t_range[0], fluid->imposed_temperature);
  stardis->t_range[1] = MMAX(stardis->t_range[1], fluid->imposed_temperature);

  ERR(create_solver_fluid(stardis, fluid));

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 0, (unsigned)sz, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* SCALE scale_factor */
static res_T
process_scale
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  if(stardis->scale_factor > 0) {
    logger_print(stardis->logger, LOG_ERROR,
      "SCALE cannot be specified twice\n");
    res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "scale factor");
  res = cstr_to_double(tk, &stardis->scale_factor);
  if(res != RES_OK
    || stardis->scale_factor <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid scale factor: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

end:
  return res;
error:
  goto end;
}

/* TRAD Trad Trad_ref */
static res_T
process_radiative
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  if(stardis->trad_def) {
    logger_print(stardis->logger, LOG_ERROR,
      "TRAD cannot be specified twice\n");
    res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "Trad");
  res = cstr_to_double(tk, &stardis->trad);
  if(res != RES_OK
    || stardis->trad < 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid Trad: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "Trad reference");
  res = cstr_to_double(tk, &stardis->trad_ref);
  if(res != RES_OK
    || stardis->trad_ref < 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid Trad reference: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->trad_def = 1;

end:
  return res;
error:
  goto end;
}

/* Read medium or boundary line; should be one of:
 * SOLID Name lambda rho cp delta Tinit Timposed volumic_power STL_filenames
 * FLUID Name rho cp Tinit Timposed STL_filenames
 * H_BOUNDARY_FOR_SOLID Name ref_temperature emissivity specular_fraction hc T_env STL_filenames
 * H_BOUNDARY_FOR_FLUID Name ref_temperature emissivity specular_fraction hc T_env STL_filenames
 * T_BOUNDARY_FOR_SOLID Name T STL_filenames
 * F_BOUNDARY_FOR_SOLID Name F STL_filenames
 * SOLID_FLUID_CONNECTION Name ref_temperature emissivity specular_fraction hc STL_filenames
 * SCALE scale_factor
 * TRAD Trad Trad_ref
 *
 * STL_filenames = { { FRONT | BACK | BOTH } STL_filename }+
 */
res_T
process_model_line
  (const char* file_name,
   char* line,
   struct stardis* stardis)
{
  res_T res = RES_OK;
  struct str keep;
  char* tk = NULL, * tok_ctx = NULL;

  ASSERT(file_name && line && stardis);

  str_init(stardis->allocator, &keep);
  ERR(str_set(&keep, line));
  CHK_TOK(strtok_r(line, " \t", &tok_ctx), "model line type");
  
  if(0 == strcasecmp(tk, "H_BOUNDARY_FOR_SOLID"))
    ERR(process_h(stardis, DESC_BOUND_H_FOR_SOLID, &tok_ctx));
  else if(0 == strcasecmp(tk, "H_BOUNDARY_FOR_FLUID"))
    ERR(process_h(stardis, DESC_BOUND_H_FOR_FLUID, &tok_ctx));
  else if(0 == strcasecmp(tk, "T_BOUNDARY_FOR_SOLID"))
    ERR(process_t(stardis, &tok_ctx));
  else if(0 == strcasecmp(tk, "F_BOUNDARY_FOR_SOLID"))
    ERR(process_flx(stardis, &tok_ctx));
  else if(0 == strcasecmp(tk, "SOLID_FLUID_CONNECTION"))
    ERR(process_sfc(stardis, &tok_ctx));
  else if(0 == strcasecmp(tk, "SOLID_SOLID_CONNECTION"))
    ERR(process_ssc(stardis, &tok_ctx));
  else if(0 == strcasecmp(tk, "SOLID"))
    ERR(process_solid(stardis, &tok_ctx));
  else if(0 == strcasecmp(tk, "FLUID"))
    ERR(process_fluid(stardis, &tok_ctx));
  else if(0 == strcasecmp(tk, "SCALE"))
    ERR(process_scale(stardis, &tok_ctx));
  else if(0 == strcasecmp(tk, "TRAD"))
    ERR(process_radiative(stardis, &tok_ctx));
  else {
    logger_print(stardis->logger, LOG_ERROR,
      "Unknown description type: %s\n", tk);
    res = RES_BAD_ARG;
    goto error;
  }

end:
  str_release(&keep);
  return res;
error:
  logger_print(stardis->logger, LOG_ERROR,
    "Invalid description line in model file '%s':\n", file_name);
  logger_print(stardis->logger, LOG_ERROR, "%s\n", str_cget(&keep));
  goto end;
}

res_T
get_dummy_solid_id
  (struct stardis* stardis,
   unsigned* id)
{
  res_T res = RES_OK;
  struct solid* dummy = NULL;
  struct dummies* dummies;
  ASSERT(stardis && id);
  dummies = &stardis->dummies;
  if(dummies->dummy_solid) {
    *id = dummies->dummy_solid_id;
    goto end;
  }
  ERR(init_solid(stardis->allocator, &dummy));
  dummies->stardis_solid = dummy;
  dummies->dummy_solid_id = allocate_stardis_medium_id(stardis);
  dummy->solid_id = dummies->dummy_solid_id;
  dummy->is_outside = 1;
  dummy->is_green = stardis->mode & (MODE_BIN_GREEN | MODE_GREEN);
  create_solver_solid(stardis, dummy);
  dummies->dummy_solid
    = darray_media_ptr_data_get(&stardis->media)[dummies->dummy_solid_id];
  logger_print(stardis->logger, LOG_OUTPUT,
    "Dummy solid created: (it is medium %u)\n",
    dummies->dummy_solid_id);
  *id = dummies->dummy_solid_id;
end:
  return res;
error:
  goto end;
}

res_T
get_dummy_fluid_id
  (struct stardis* stardis,
   unsigned* id)
{
  res_T res = RES_OK;
  struct fluid* dummy = NULL;
  struct dummies* dummies;
  ASSERT(stardis && id);
  dummies = &stardis->dummies;
  if(dummies->dummy_fluid) {
    *id = dummies->dummy_fluid_id;
    goto end;
  }
  ERR(init_fluid(stardis->allocator, &dummy));
  dummies->stardis_fluid = dummy;
  dummies->dummy_fluid_id = allocate_stardis_medium_id(stardis);
  dummy->fluid_id = dummies->dummy_fluid_id;
  dummy->is_outside = 1;
  dummy->is_green = stardis->mode & (MODE_BIN_GREEN | MODE_GREEN);
  create_solver_fluid(stardis, dummy);
  dummies->dummy_fluid
    = darray_media_ptr_data_get(&stardis->media)[dummies->dummy_fluid_id];
  logger_print(stardis->logger, LOG_OUTPUT,
    "Dummy fluid created: (it is medium %u)\n",
    dummies->dummy_fluid_id);
  *id = dummies->dummy_fluid_id;
end:
  return res;
error:
  goto end;
}
