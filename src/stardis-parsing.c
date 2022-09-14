/* Copyright (C) 2018-2022 |Meso|Star> (contact@meso-star.com)
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

#include "stardis-parsing.h"
#include "stardis-app.h"
#include "stardis-prog-properties.h"
#include "stardis-args.h"
#include "stardis-description.h"
#include "stardis-hbound.h"
#include "stardis-hbound-prog.h"
#include "stardis-tbound.h"
#include "stardis-tbound-prog.h"
#include "stardis-fbound.h"
#include "stardis-fbound-prog.h"
#include "stardis-sfconnect.h"
#include "stardis-sfconnect-prog.h"
#include "stardis-ssconnect.h"
#include "stardis-ssconnect-prog.h"
#include "stardis-fluid-prog.h"
#include "stardis-fluid.h"
#include "stardis-solid-prog.h"
#include "stardis-solid.h"
#include "stardis-program.h"
#include "stardis-default.h"
#include "stardis-green-types.h"

#include <rsys/rsys.h>
#include <rsys/cstr.h>
#include <rsys/double2.h>
#include <rsys/double3.h>
#include <rsys/logger.h>
#include <rsys/text_reader.h>
#include <rsys/library.h>

#include <wordexp.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef COMPILER_GCC
#include <strings.h> /* strcasecmp */
#else
#define strcasecmp(s1, s2) _stricmp((s1), (s2))
#endif

/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static void
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
   wordexp_t* pwordexp,
   size_t* idx)
{
  char* arg = NULL;
  int file_count = 0;
  struct sstl* sstl = NULL;
  struct add_geom_ctx add_geom_ctx;
  unsigned current_merge_errors;
  struct sg3d_geometry_add_callbacks callbacks = SG3D_ADD_CALLBACKS_NULL__;
  struct darray_uint degenerated;
  struct str str;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp && idx);

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
      if(pwordexp->we_wordc <= *idx
          || 0 == strcasecmp((arg = pwordexp->we_wordv[(*idx)++]), "PROG_PARAMS"))
      {
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
      if(0 == strcasecmp(arg, "FRONT")) {
        add_geom_ctx.properties[SG3D_FRONT] = description_id;
        add_geom_ctx.properties[SG3D_BACK] = SG3D_UNSPECIFIED_PROPERTY;
      }
      else if(0 == strcasecmp(arg, "BACK")) {
        add_geom_ctx.properties[SG3D_FRONT] = SG3D_UNSPECIFIED_PROPERTY;
        add_geom_ctx.properties[SG3D_BACK] = description_id;
      }
      else if(0 == strcasecmp(arg, "BOTH")) {
        add_geom_ctx.properties[SG3D_FRONT] = description_id;
        add_geom_ctx.properties[SG3D_BACK] = description_id;
      }
      else {
        logger_print(stardis->logger, LOG_ERROR,
          "Invalid side specifier: %s\n", arg);
        res = RES_BAD_ARG;
        goto error;
      }
    }
    if(pwordexp->we_wordc <= *idx
        || 0 == strcasecmp((arg = pwordexp->we_wordv[(*idx)++]), "PROG_PARAMS"))
    {
      if(!descr_is_intface /* Has read a side specifier */
        || !file_count) /* Need at least 1 file name */
      {
        logger_print(stardis->logger, LOG_ERROR,
          "Invalid data (missing token 'file name')\n");
        res = RES_BAD_ARG;
        goto error;
      }
      else break;
    }
    file_count++;
    res = sstl_load(sstl, arg);
    if(res == RES_OK) {
      ERR(sstl_get_desc(sstl, &add_geom_ctx.stl_desc));
      ASSERT(add_geom_ctx.stl_desc.vertices_count <= UINT_MAX
        && add_geom_ctx.stl_desc.triangles_count <= UINT_MAX);
      logger_print(stardis->logger, LOG_OUTPUT,
        "Read file '%s': %u triangles found.\n",
        arg, (unsigned)add_geom_ctx.stl_desc.triangles_count);
    } else {
      logger_print(stardis->logger, LOG_ERROR,
        "Cannot read STL file: '%s'\n", arg);
      goto error;
    }

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
        arg, (unsigned long)c);
      ERR(str_printf(&str, "Degenerated triangles IDs: %u", ids[0]));
      FOR_EACH(n, 1, c) { ERR(str_append_printf(&str, ", %u", ids[n])); }
      logger_print(stardis->logger, LOG_OUTPUT, "%s\n", str_cget(&str));
      darray_uint_clear(&degenerated);
    }

    if(res != RES_OK) {
      logger_print(stardis->logger, LOG_ERROR,
        "Cannot add file content: '%s'\n", arg);
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
        arg, merge_errors - current_merge_errors);
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

static res_T
description_set_name
  (struct stardis* stardis,
   struct str* name,
   const char* arg)
{
  res_T res = RES_OK;
  double foo;
  const char* keywords[] = {
    "AUTO", "BACK", "BOTH", "FLUID", "FLUID_PROG", "FRONT", "F_BOUNDARY_FOR_SOLID",
    "F_BOUNDARY_FOR_SOLID_PROG", "H_BOUNDARY_FOR_FLUID", "H_BOUNDARY_FOR_FLUID_PROG",
    "H_BOUNDARY_FOR_SOLID", "H_BOUNDARY_FOR_SOLID_PROG", "PROGRAM", "PROG_PARAMS",
    "SCALE", "SOLID", "SOLID_PROG", "SOLID_FLUID_CONNECTION",
    "SOLID_FLUID_CONNECTION_PROG", "SOLID_SOLID_CONNECTION",
    "SOLID_SOLID_CONNECTION_PROG", "TRAD", "T_BOUNDARY_FOR_SOLID",
    "T_BOUNDARY_FOR_SOLID_PROG", "UNKNOWN" };
  const char* reason = NULL;
  size_t i;
  ASSERT(name && arg);

  /* Use name before uppercasing it */
  ERR(str_set(name, arg));

  if(RES_OK == cstr_to_double(arg, &foo)) {
    /* A number is not a sensible choice for a name! */
    res = RES_BAD_ARG;
    reason = "number";
    goto error;
  }
  FOR_EACH(i, 0, sizeof(keywords) / sizeof(*keywords)) {
    if(0 == strcasecmp(arg, keywords[i])) {
      /* A keyword is not a sensible choice for a name! */
      res = RES_BAD_ARG;
      reason = "reserved keyword";
      goto error;
    }
  }
  if(str_len(name) > DESC_NAME_MAX_LEN) {
    /* Due to Green export limitations, names are limited in length */
    res = RES_BAD_ARG;
    reason = "too long";
    goto error;
  }
  /* Name is OK */

end:
  return res;
error:
  ASSERT(reason != NULL);
  logger_print(stardis->logger, LOG_ERROR, "Invalid name (%s): %s\n",
    reason, arg);
  goto end;
}

static struct description*
find_description_by_name
  (struct stardis* stardis,
   const struct str* name,
   const struct description* self)
{
  size_t i;
  ASSERT(stardis && name);

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
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  size_t sz;
  struct h_boundary* h_boundary;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.hbound_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz+1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_h_boundary(stardis->allocator, &desc->d.h_boundary));
  h_boundary = desc->d.h_boundary;
  desc->type = type;

  CHK_ARG(idx, "h boundary name");
  ERR(description_set_name(stardis, &h_boundary->name, arg));
  if(find_description_by_name(stardis, &h_boundary->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_ARG(idx, "ref_temperature");
  res = cstr_to_double(arg, &h_boundary->ref_temperature);
  if(res != RES_OK
    || h_boundary->ref_temperature < 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
        "Invalid reference temperature: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], h_boundary->ref_temperature);
  stardis->t_range[1] = MMAX(stardis->t_range[1], h_boundary->ref_temperature);
  CHK_ARG(idx, "emissivity");
  res = cstr_to_double(arg, &h_boundary->emissivity);
  if(res != RES_OK
    || h_boundary->emissivity < 0
    || h_boundary->emissivity > 1)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid emissivity: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "specular fraction");
  res = cstr_to_double(arg, &h_boundary->specular_fraction);
  if(res != RES_OK
    || h_boundary->specular_fraction < 0
    || h_boundary->specular_fraction > 1)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid specular fraction: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "Convection coefficient");
  res = cstr_to_double(arg, &h_boundary->hc);
  if(res != RES_OK
    || h_boundary->hc < 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid Convection coefficient: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "temperature");
  res = cstr_to_double(arg, &h_boundary->imposed_temperature);
  if(res != RES_OK
    || h_boundary->imposed_temperature < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid temperature: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], h_boundary->imposed_temperature);
  stardis->t_range[1] = MMAX(stardis->t_range[1], h_boundary->imposed_temperature);

  if(type == DESC_BOUND_H_FOR_FLUID)
    ERR(get_dummy_solid_id(stardis, &h_boundary->mat_id));
  else {
    struct fluid* fluid = NULL;
    ASSERT(type == DESC_BOUND_H_FOR_SOLID);
    ERR(init_fluid(stardis->allocator, &fluid));
    fluid->fluid_id = allocate_stardis_medium_id(stardis);
    h_boundary->mat_id = fluid->fluid_id;
    h_boundary->possible_external_fluid = fluid;
    ASSERT(sz <= UINT_MAX);
    fluid->desc_id = (unsigned)sz;
    fluid->imposed_temperature = h_boundary->imposed_temperature;
    fluid->is_outside = 1;
    fluid->is_green = stardis->mode & (MODE_BIN_GREEN | MODE_GREEN);
    ERR(create_solver_fluid(stardis, fluid));
    logger_print(stardis->logger, LOG_OUTPUT,
      "External fluid created: T=%g (it is medium %u)\n",
      fluid->imposed_temperature,
      fluid->fluid_id);
  }

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, pwordexp, &idx));

end:
  return res;
error:
  goto end;
}

static res_T
set_argc_argv
  (struct mem_allocator* allocator,
   size_t* argc,
   char** argv[],
   const wordexp_t* pwordexp,
   size_t idx)
{
  size_t i, n;
  res_T res = RES_OK;

  ASSERT(argc && argv && pwordexp);
  *argv = NULL;

  if(pwordexp->we_wordc < idx) {
    res = RES_BAD_ARG;
    goto error;
  }
  *argc = pwordexp->we_wordc - idx;
  *argv = MEM_CALLOC(allocator, 1 + *argc, sizeof(char *));
  if(*argv == NULL) {
    res = RES_MEM_ERR;
    goto error;
  }
  for(i = idx, n = 0; i < pwordexp->we_wordc; i++, n++) {
    (*argv)[n] = MEM_CALLOC(allocator, 1, 1 + strlen(pwordexp->we_wordv[i]));
    if((*argv)[n] == NULL) {
      res = RES_MEM_ERR;
      goto error;
    }
    strcpy((*argv)[n], pwordexp->we_wordv[i]); /* size is adequate */
  }
end:
  return res;
error:
  if(*argv) {
    for(i = 0; i < *argc; i++)
      if((*argv)[i]) MEM_RM(allocator, (*argv)[i]);
    MEM_RM(allocator, *argv);
  }
  goto end;
}

/* utility macros */
#define GET_LIB_SYMBOL_BASE(DestField, LibHandle, FunName, Optional) \
  *(void**)DestField = library_get_symbol((LibHandle), #FunName ); \
  if(!*DestField && !(Optional)) { \
    logger_print(stardis->logger, LOG_ERROR, \
      "Cannot find function '" #FunName "()' in lib %s\n", lib_name); \
    res = RES_BAD_ARG; \
    goto error; \
  }

#define GET_LIB_SYMBOL(Dest, Field, FunName) \
  GET_LIB_SYMBOL_BASE(&((Dest)->Field), (Dest)->program->lib_handle, FunName, 0)

#define CREATE_DESC_DATA_BASE(Desc, CreateArgs) \
  (Desc)->prog_data = (Desc)->create(&ctx, CreateArgs); \
  if(!(Desc)->prog_data) { \
    logger_print(stardis->logger, LOG_ERROR, \
      "Cannot create data for description %s\n", str_cget(&(Desc)->name)); \
    res = RES_BAD_ARG; \
    goto error; \
  }

#define CREATE_DESC_DATA(Desc) \
  CREATE_DESC_DATA_BASE(Desc, \
      LIST_ARG3((Desc)->program->prog_data, (Desc)->argc, (Desc)->argv))

/* The returned program is NULL if no stardis_create_program function is
 * defined in the library. */
static res_T
get_prog_common
  (const char* lib_name,
   struct stardis* stardis,
   struct program** program,
   void* (**create)
     (const struct stardis_description_create_context*, void*, size_t, char**),
   void (**release)(void*))
{
  res_T res = RES_OK;
  struct description* desc;
  struct str tmp;

  ASSERT(lib_name && program && create && release && stardis);

  /* get the library handler */
  str_init(stardis->allocator, &tmp);
  ERR(str_set(&tmp, lib_name));
  desc = find_description_by_name(stardis, &tmp, NULL);
  if(!desc) {
    logger_print(stardis->logger, LOG_ERROR,
      "Undefined PROGRAM: %s\n", lib_name);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  else if(desc->type != DESC_PROGRAM) {
    logger_print(stardis->logger, LOG_ERROR,
      "Is not a PROGRAM: %s\n", lib_name);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  *program = desc->d.program;
  /* get the mandatory user-defined functions from the library */
  GET_LIB_SYMBOL_BASE(create, (*program)->lib_handle, stardis_create_data, 0);
  GET_LIB_SYMBOL_BASE(release, (*program)->lib_handle, stardis_release_data, 0);

end:
  str_release(&tmp);
  return res;
error:
  goto end;
}

/* PROGRAM Name library_path [...] */
static res_T
process_program
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  size_t sz;
  struct program* program;
  const char* lib_name;
  const char* lic;
  const char* _c_;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_program(stardis->allocator, &desc->d.program));
  program = desc->d.program;
  desc->type = DESC_PROGRAM;
  
  CHK_ARG(idx, "program name");
  ERR(description_set_name(stardis, &program->name, arg));
  if(find_description_by_name(stardis, &program->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  lib_name = arg;

  CHK_ARG(idx, "library path");
  ERR(str_set(&program->lib_path, arg));

  /* get the library handler */
  program->lib_handle = library_open(arg);
  if(!program->lib_handle) {
    logger_print(stardis->logger, LOG_ERROR,
      "Cannot open library: %s (%s)\n", lib_name, arg);
    res = RES_BAD_ARG;
    goto error;
  }

  /* get the mandatory user-defined functions from the library */
  GET_LIB_SYMBOL_BASE(&program->get_copyright_notice, program->lib_handle,
    get_copyright_notice, 0);
  GET_LIB_SYMBOL_BASE(&program->get_license_short, program->lib_handle,
    get_license_short, 0);
  GET_LIB_SYMBOL_BASE(&program->get_license_text, program->lib_handle,
    get_license_text, 0);
  /* get the optional user-defined functions from the library */
  GET_LIB_SYMBOL_BASE(&program->create,
    program->lib_handle,  stardis_create_library_data, 1);
  GET_LIB_SYMBOL_BASE(&program->release,
    program->lib_handle,  stardis_release_library_data, 1);
  GET_LIB_SYMBOL_BASE(&program->finalize,
    program->lib_handle,  stardis_finalize_library_data, 1);
  if(!(program->create && program->release && program->finalize)
    && !(!program->create && !program->release && !program->finalize))
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Inconsistent library data management for library '%s'.\n",
      lib_name);
    logger_print(stardis->logger, LOG_ERROR,
      "Please define all or none of stardis_create_library_data, "
      "stardis_finalize_library_data and stardis_release_library_data funcions.\n");
    res = RES_BAD_ARG;
    goto error;
  }

  /* store the end of line as args for custom init */
  ERR(set_argc_argv(stardis->allocator, &program->argc, &program->argv,
    pwordexp, idx));
  if(program->create) {
    /* create and init custom data */
    struct stardis_program_context ctx;
    ctx.name = lib_name;
    switch(stardis->verbose) {
      case 0: ctx.verbosity_level = STARDIS_VERBOSE_NONE; break;
      case 1: ctx.verbosity_level = STARDIS_VERBOSE_ERROR; break;
      case 2: ctx.verbosity_level = STARDIS_VERBOSE_WARNING; break;
      case 3: ctx.verbosity_level = STARDIS_VERBOSE_INFO; break;
      default:
        FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
    }
    CREATE_DESC_DATA_BASE(program, LIST_ARG2(program->argc, program->argv));
  } else if(program->argc != 0) {
    logger_print(stardis->logger, LOG_ERROR,
      "Library '%s' has no custom data management functions but has arguments:\n",
      lib_name);
    for( ; idx < pwordexp->we_wordc; idx++) {
      logger_print(stardis->logger, LOG_ERROR, "%s\n", pwordexp->we_wordv[idx]);
    }
    res = RES_BAD_ARG;
    goto error;
  }

  lic = program->get_license_short(program->prog_data);
  _c_ = program->get_copyright_notice(program->prog_data);
  if(!lic) {
    res = RES_BAD_ARG;
    goto error;
  }
  if(!_c_) {
    res = RES_BAD_ARG;
    goto error;
  }
  logger_print(stardis->logger, LOG_OUTPUT,
    "Loading external library '%s': \"%s\"\n",
    str_cget(&program->name), str_cget(&program->lib_path));
  logger_print(stardis->logger, LOG_OUTPUT, "  %s\n", _c_);
  logger_print(stardis->logger, LOG_OUTPUT, "  %s\n", lic);

end:
  return res;
error:
  goto end;
}

/* H_BOUNDARY_FOR_SOLID_PROG Name ProgName STL_filenames [PROG_PARAMS ...]
 * H_BOUNDARY_FOR_FLUID_PROG Name ProgName STL_filenames [PROG_PARAMS ...] */
static res_T
process_h_prog
  (struct stardis* stardis,
   const enum description_type type,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  const char *lib_name, *desc_name;
  size_t sz;
  struct h_boundary_prog* h_boundary_prog;
  struct stardis_description_create_context ctx;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.fmed_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_h_boundary_prog(stardis->allocator, &desc->d.h_boundary_prog));
  h_boundary_prog = desc->d.h_boundary_prog;
  desc->type = type;
  
  CHK_ARG(idx, "programmed h boundary name");
  ERR(description_set_name(stardis, &h_boundary_prog->name, arg));
  if(find_description_by_name(stardis, &h_boundary_prog->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  desc_name = arg;

  CHK_ARG(idx, "program name");
  ERR(str_set(&h_boundary_prog->prog_name, arg));
  lib_name = arg;

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, pwordexp, &idx));

  /* store the end of line as args for custom init */
  ERR(set_argc_argv(stardis->allocator, &h_boundary_prog->argc,
    &h_boundary_prog->argv, pwordexp, idx));
  /* get the user-defined functions from the library */
  ERR(get_prog_common(lib_name, stardis,  &h_boundary_prog->program,
    &h_boundary_prog->create, &h_boundary_prog->release));
  GET_LIB_SYMBOL(h_boundary_prog, ref_temp, stardis_reference_temperature);
  GET_LIB_SYMBOL(h_boundary_prog, emissivity, stardis_emissivity);
  GET_LIB_SYMBOL(h_boundary_prog, alpha, stardis_specular_fraction);
  GET_LIB_SYMBOL(h_boundary_prog, hc, stardis_convection_coefficient);
  GET_LIB_SYMBOL(h_boundary_prog, hmax, stardis_max_convection_coefficient);
  GET_LIB_SYMBOL(h_boundary_prog, t_range, stardis_t_range);
  if(type == DESC_BOUND_H_FOR_FLUID_PROG) {
    GET_LIB_SYMBOL(h_boundary_prog, boundary_temp, stardis_boundary_temperature);
  } else {
    GET_LIB_SYMBOL(h_boundary_prog, fluid_temp, stardis_medium_temperature);
  }
  /* create and init custom data */
  ctx.name = desc_name;
  CREATE_DESC_DATA(h_boundary_prog);

  h_boundary_prog->t_range(h_boundary_prog->prog_data, stardis->t_range);

  /* create the media behind the interface */
  if(type == DESC_BOUND_H_FOR_FLUID_PROG) {
    ERR(get_dummy_solid_id(stardis, &h_boundary_prog->mat_id));
  } else {
    struct fluid_prog* fluid_prog = NULL;
    ASSERT(type == DESC_BOUND_H_FOR_SOLID_PROG);
    ERR(init_fluid_prog(stardis->allocator, &fluid_prog));
    fluid_prog->fluid_id = allocate_stardis_medium_id(stardis);
    h_boundary_prog->mat_id = fluid_prog->fluid_id;
    h_boundary_prog->possible_external_fluid = fluid_prog;
    fluid_prog->desc_id = (unsigned)sz;
    fluid_prog->temp = h_boundary_prog->fluid_temp;
    fluid_prog->is_outside = 1;
    fluid_prog->prog_data = h_boundary_prog->prog_data;
    /* fluid_prog->release is NULL to avoid deleting shared prog_data */
    ERR(create_solver_external_fluid_prog(stardis, fluid_prog));
    logger_print(stardis->logger, LOG_OUTPUT,
      "External programmed fluid created (it is medium %u)\n",
      fluid_prog->fluid_id);
  }

end:
  return res;
error:
  goto end;
}

/* T_BOUNDARY_FOR_SOLID Name T STL_filenames */
static res_T
process_t
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  size_t sz;
  struct t_boundary* t_boundary;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.tbound_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_t_boundary(stardis->allocator, &desc->d.t_boundary));
  t_boundary = desc->d.t_boundary;
  desc->type = DESC_BOUND_T_FOR_SOLID;

  ERR(get_dummy_fluid_id(stardis, &t_boundary->mat_id));

  
  CHK_ARG(idx, "temperature boundary name");
  ERR(description_set_name(stardis, &t_boundary->name, arg));
  if(find_description_by_name(stardis, &t_boundary->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_ARG(idx, "temperature");
  res = cstr_to_double(arg, &t_boundary->imposed_temperature);
  if(res != RES_OK
    || t_boundary->imposed_temperature < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid temperature: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], t_boundary->imposed_temperature);
  stardis->t_range[1] = MMAX(stardis->t_range[1], t_boundary->imposed_temperature);

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, pwordexp, &idx));

end:
  return res;
error:
  goto end;
}

/* T_BOUNDARY_FOR_SOLID_PROG Name ProgName STL_filenames [PROG_PARAMS ...] */
static res_T
process_t_prog
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  const char *lib_name, *desc_name;
  size_t sz;
  struct t_boundary_prog* t_boundary_prog;
  struct stardis_description_create_context ctx;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.fmed_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_t_boundary_prog(stardis->allocator, &desc->d.t_boundary_prog));
  t_boundary_prog = desc->d.t_boundary_prog;
  desc->type = DESC_BOUND_T_FOR_SOLID_PROG;

  ERR(get_dummy_fluid_id(stardis, &t_boundary_prog->mat_id));
  
  CHK_ARG(idx, "programmed t boundary name");
  ERR(description_set_name(stardis, &t_boundary_prog->name, arg));
  if(find_description_by_name(stardis, &t_boundary_prog->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  desc_name = arg;

  CHK_ARG(idx, "program name");
  ERR(str_set(&t_boundary_prog->prog_name, arg));
  lib_name = arg;

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, pwordexp, &idx));

  /* store the end of line as args for custom init */
  ERR(set_argc_argv(stardis->allocator, &t_boundary_prog->argc,
    &t_boundary_prog->argv, pwordexp, idx));
  /* get the user-defined functions from the library */
  ERR(get_prog_common(lib_name, stardis, &t_boundary_prog->program,
    &t_boundary_prog->create, &t_boundary_prog->release));
  GET_LIB_SYMBOL(t_boundary_prog, temperature, stardis_boundary_temperature);
  GET_LIB_SYMBOL(t_boundary_prog, t_range, stardis_t_range);
  /* create and init custom data */
  ctx.name = desc_name;
  CREATE_DESC_DATA(t_boundary_prog);

  t_boundary_prog->t_range(t_boundary_prog->prog_data, stardis->t_range);

end:
  return res;
error:
  goto end;
}

/* F_BOUNDARY_FOR_SOLID Name F STL_filenames */
static res_T
process_flx
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  size_t sz;
  struct f_boundary* f_boundary;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.fbound_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_f_boundary(stardis->allocator, &desc->d.f_boundary));
  f_boundary = desc->d.f_boundary;
  desc->type = DESC_BOUND_F_FOR_SOLID;

  ERR(get_dummy_fluid_id(stardis, &f_boundary->mat_id));

  CHK_ARG(idx, "flux boundary name");
  ERR(description_set_name(stardis, &f_boundary->name, arg));
  if(find_description_by_name(stardis, &f_boundary->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_ARG(idx, "flux");
  res = cstr_to_double(arg, &f_boundary->imposed_flux);
  if(res != RES_OK
    || f_boundary->imposed_flux == SDIS_FLUX_NONE) {
    /* Flux can be < 0 but not undefined */
    if(res == RES_OK) res = RES_BAD_ARG;
    logger_print(stardis->logger, LOG_ERROR, "Invalid flux: %s\n", arg);
    goto end;
  }
  if(f_boundary->imposed_flux != 0 && stardis->picard_order > 1) {
    logger_print(stardis->logger, LOG_ERROR,
      "Cannot have a flux defined at a boundary (here %f) if Picard order "
      "is not 1 (here order is %u)\n",
      f_boundary->imposed_flux, stardis->picard_order);
    res = RES_BAD_ARG;
    goto end;
  }

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, pwordexp, &idx));

end:
  return res;
error:
  goto end;
}

/* F_BOUNDARY_FOR_SOLID_PROG Name ProgName STL_filenames [PROG_PARAMS ...] */
static res_T
process_flx_prog
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  const char *lib_name, *desc_name;
  size_t sz;
  struct f_boundary_prog* f_boundary_prog;
  struct stardis_description_create_context ctx;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.fmed_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_f_boundary_prog(stardis->allocator, &desc->d.f_boundary_prog));
  f_boundary_prog = desc->d.f_boundary_prog;
  desc->type = DESC_BOUND_F_FOR_SOLID_PROG;

  ERR(get_dummy_fluid_id(stardis, &f_boundary_prog->mat_id));
  
  CHK_ARG(idx, "programmed t boundary name");
  ERR(description_set_name(stardis, &f_boundary_prog->name, arg));
  if(find_description_by_name(stardis, &f_boundary_prog->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_ARG(idx, "program name");
  ERR(str_set(&f_boundary_prog->prog_name, arg));
  desc_name = arg;
  desc_name = arg;
  lib_name = arg;

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, pwordexp, &idx));

  /* store the end of line as args for custom init */
  ERR(set_argc_argv(stardis->allocator, &f_boundary_prog->argc,
    &f_boundary_prog->argv, pwordexp, idx));
  /* get the user-defined functions from the library */
  ERR(get_prog_common(lib_name, stardis, &f_boundary_prog->program,
    &f_boundary_prog->create, &f_boundary_prog->release));
  GET_LIB_SYMBOL(f_boundary_prog, flux, stardis_boundary_flux);
  /* create and init custom data */
  ctx.name = desc_name;
  CREATE_DESC_DATA(f_boundary_prog);

end:
  return res;
error:
  goto end;
}

/* SOLID_FLUID_CONNECTION Name ref_temperature emissivity specular_fraction hc STL_filenames */
static res_T
process_sfc
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  size_t sz;
  struct solid_fluid_connect* sf_connect;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.sfconnect_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_sf_connect(stardis->allocator, &desc->d.sf_connect));
  sf_connect = desc->d.sf_connect;
  desc->type = DESC_SOLID_FLUID_CONNECT;

  /* Use a medium ID even if there is no medium here
   * As other cases use media IDs as unique IDs for read_sides_and_files calls
   * we continue the trend to ensure connection ID is OK */
  sf_connect->connection_id = allocate_stardis_medium_id(stardis);

  CHK_ARG(idx, "solid-fluid connection name");
  ERR(description_set_name(stardis, &sf_connect->name, arg));
  if(find_description_by_name(stardis, &sf_connect->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_ARG(idx, "ref_temperature");
  res = cstr_to_double(arg, &sf_connect->ref_temperature);
  if(res != RES_OK
    || sf_connect->ref_temperature < 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
        "Invalid reference temperature: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], sf_connect->ref_temperature);
  stardis->t_range[1] = MMAX(stardis->t_range[1], sf_connect->ref_temperature);
  CHK_ARG(idx, "emissivity");
  res = cstr_to_double(arg, &sf_connect->emissivity);
  if(res != RES_OK
    || sf_connect->emissivity < 0
    || sf_connect->emissivity > 1)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid emissivity: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "specular fraction");
  res = cstr_to_double(arg, &sf_connect->specular_fraction);
  if(res != RES_OK
    || sf_connect->specular_fraction < 0
    || sf_connect->specular_fraction > 1)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid specular fraction: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "Convection coefficient");
  res = cstr_to_double(arg, &sf_connect->hc);
  if(res != RES_OK
    || sf_connect->hc < 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid Convection coefficient: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, pwordexp, &idx));

end:
  return res;
error:
  goto end;
}

/* SOLID_FLUID_CONNECTION_PROG Name ProgName STL_filenames [PROG_PARAMS ...] */
static res_T
process_sfc_prog
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  const char *lib_name, *desc_name;
  size_t sz;
  struct solid_fluid_connect_prog* sf_connect_prog;
  struct stardis_description_create_context ctx;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.sfconnect_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_sf_connect_prog(stardis->allocator, &desc->d.sf_connect_prog));
  sf_connect_prog = desc->d.sf_connect_prog;
  desc->type = DESC_SOLID_FLUID_CONNECT_PROG;

  CHK_ARG(idx, "programmed solid-fluid connection name");
  ERR(description_set_name(stardis, &sf_connect_prog->name, arg));
  if(find_description_by_name(stardis, &sf_connect_prog->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  desc_name = arg;

  CHK_ARG(idx, "program name");
  ERR(str_set(&sf_connect_prog->prog_name, arg));
  lib_name = arg;

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, pwordexp, &idx));

  /* store the end of line as args for custom init */
  ERR(set_argc_argv(stardis->allocator, &sf_connect_prog->argc,
    &sf_connect_prog->argv, pwordexp, idx));
  /* get the user-defined functions from the library */
  ERR(get_prog_common(lib_name, stardis, &sf_connect_prog->program,
    &sf_connect_prog->create, &sf_connect_prog->release));
  GET_LIB_SYMBOL(sf_connect_prog, ref_temp, stardis_reference_temperature);
  GET_LIB_SYMBOL(sf_connect_prog, emissivity, stardis_emissivity);
  GET_LIB_SYMBOL(sf_connect_prog, alpha, stardis_specular_fraction);
  GET_LIB_SYMBOL(sf_connect_prog, hc, stardis_convection_coefficient);
  GET_LIB_SYMBOL(sf_connect_prog, hmax, stardis_max_convection_coefficient);
  GET_LIB_SYMBOL(sf_connect_prog, t_range, stardis_t_range);
  /* create and init custom data */
  ctx.name = desc_name;
  CREATE_DESC_DATA(sf_connect_prog);

  sf_connect_prog->t_range(sf_connect_prog->prog_data, stardis->t_range);

end:
  return res;
error:
  goto end;
}

/* SOLID_SOLID_CONNECTION Name contact-resitance STL_filenames */
static res_T
process_ssc
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  size_t sz;
  struct solid_solid_connect* ss_connect;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.ssconnect_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_ss_connect(stardis->allocator, &desc->d.ss_connect));
  ss_connect = desc->d.ss_connect;
  desc->type = DESC_SOLID_SOLID_CONNECT;

  /* Use a medium ID even if there is no medium here
   * As other cases use media IDs as unique IDs for read_sides_and_files calls
   * we continue the trend to ensure connection ID is OK */
  ss_connect->connection_id = allocate_stardis_medium_id(stardis);

  CHK_ARG(idx, "solid-solid connection name");
  ERR(description_set_name(stardis, &ss_connect->name, arg));
  if(find_description_by_name(stardis, &ss_connect->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_ARG(idx, "contact resistance");
  res = cstr_to_double(arg, &ss_connect->tcr);
  if(res != RES_OK
    || ss_connect->tcr < 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid contact resistance: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  else if(ss_connect->tcr == 0) {
    logger_print(stardis->logger, LOG_WARNING,
      "Solid-solid connection %s: defining a contact resistance to 0 has "
      "no effect\n", str_cget(&ss_connect->name));
  }

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, pwordexp, &idx));

end:
  return res;
error:
  goto end;
}

/* SOLID_SOLID_CONNECTION_PROG Name ProgName STL_filenames [PROG_PARAMS ...] */
static res_T
process_ssc_prog
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  const char *lib_name, *desc_name;
  size_t sz;
  struct solid_solid_connect_prog* ss_connect_prog;
  struct stardis_description_create_context ctx;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.sfconnect_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_ss_connect_prog(stardis->allocator, &desc->d.ss_connect_prog));
  ss_connect_prog = desc->d.ss_connect_prog;
  desc->type = DESC_SOLID_SOLID_CONNECT_PROG;

  CHK_ARG(idx, "programmed solid-solid connection name");
  ERR(description_set_name(stardis, &ss_connect_prog->name, arg));
  if(find_description_by_name(stardis, &ss_connect_prog->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  desc_name = arg;

  CHK_ARG(idx, "program name");
  ERR(str_set(&ss_connect_prog->prog_name, arg));
  lib_name = arg;

  ASSERT(sz <= UINT_MAX);
  ERR(read_sides_and_files(stardis, 1, (unsigned)sz, pwordexp, &idx));

  /* store the end of line as args for custom init */
  ERR(set_argc_argv(stardis->allocator, &ss_connect_prog->argc,
    &ss_connect_prog->argv, pwordexp, idx));
  /* get the user-defined functions from the library */
  ERR(get_prog_common(lib_name, stardis, &ss_connect_prog->program,
    &ss_connect_prog->create, &ss_connect_prog->release));
  GET_LIB_SYMBOL(ss_connect_prog, tcr, stardis_thermal_contact_resistance);
  if(!ss_connect_prog->tcr) {
    logger_print(stardis->logger, LOG_ERROR,
      "Cannot find function 'stardis_thermal_contact_resistance()' in lib %s\n",
      lib_name);
    res = RES_BAD_ARG;
    goto error;
  }
  /* create and init custom data */
  ctx.name = desc_name;
  CREATE_DESC_DATA(ss_connect_prog);

end:
  return res;
error:
  goto end;
}

static res_T
read_imposed_temperature
  (struct stardis* stardis,
   double* imposed_temperature,
   wordexp_t* pwordexp,
   size_t* idx)
{
  char* arg = NULL;
  struct str keep;
  res_T res = RES_OK;
  ASSERT(stardis && imposed_temperature && pwordexp);

  str_init(stardis->allocator, &keep);
  CHK_ARG((*idx), "imposed temperature");
  ERR(str_set(&keep, arg));
  if(RES_OK == cstr_to_double(arg, imposed_temperature)) {
    /* Was a number */
    if(*imposed_temperature < 0) {
      res = RES_BAD_ARG;
      goto error;
    }
  } else {
    /* Could be 'unknown' */
    if(0 == strcasecmp(arg, "UNKNOWN")) {
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
   wordexp_t* pwordexp,
   size_t* idx)
{
  char* arg = NULL;
  res_T res = RES_OK;
  ASSERT(stardis && delta && pwordexp && idx);

  CHK_ARG((*idx), "delta");
  if(RES_OK == cstr_to_double(arg, delta)) {
    /* Was a number */
    if(*delta <= 0) {
      res = RES_BAD_ARG;
      goto error;
    }
  } else {
    /* Could be 'auto' */ 
    if(0 == strcasecmp(arg, "AUTO")) {
      /* Set to DELTA_AUTO until actual value is substituted */
      *delta = DELTA_AUTO;
    } else {
      res = RES_BAD_ARG;
      goto error;
    }
  }
end:
  return res;
error:
  logger_print(stardis->logger, LOG_ERROR, "Invalid delta: %s\n", arg);
  goto end;
}

/* SOLID Name lambda rho cp delta Tinit Timposed volumic_power STL_filenames */
static res_T
process_solid
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  size_t sz;
  struct solid* solid;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

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
  
  CHK_ARG(idx, "solid name");
  ERR(description_set_name(stardis, &solid->name, arg));
  if(find_description_by_name(stardis, &solid->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_ARG(idx, "lambda");
  res = cstr_to_double(arg, &solid->lambda);
  if(res != RES_OK
    || solid->lambda <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid lambda: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "rho");
  res = cstr_to_double(arg, &solid->rho);
  if(res != RES_OK
    || solid->rho <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid rho: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "cp");
  res = cstr_to_double(arg, &solid->cp);
  if(res != RES_OK
    || solid->cp <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid cp: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  ERR(read_delta(stardis, &solid->delta, pwordexp, &idx));
  CHK_ARG(idx, "Tinit");
  res = cstr_to_double(arg, &solid->tinit);
  if(res != RES_OK
    || solid->tinit < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid Tinit: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], solid->tinit);
  stardis->t_range[1] = MMAX(stardis->t_range[1], solid->tinit);
  ERR(read_imposed_temperature(stardis, &solid->imposed_temperature,
    pwordexp, &idx));
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
  CHK_ARG(idx, "volumic power");
  res = cstr_to_double(arg, &solid->vpower);
  if(res != RES_OK) {
    /* VPower can be < 0 */
    logger_print(stardis->logger, LOG_ERROR, "Invalid volumic power: %s\n", arg);
    goto end;
  }
  if(solid->vpower != 0 && stardis->picard_order > 1) {
    logger_print(stardis->logger, LOG_ERROR,
      "Cannot have volumic power (here %f) if Picard order is not 1 "
      "(here order is %u)\n",
      solid->vpower, stardis->picard_order);
    res = RES_BAD_ARG;
    goto end;
  }

  /* Actual solid creation is defered until geometry is read to allow
   * enclosure shape VS delta analysis (and auto delta computation) */

  ERR(read_sides_and_files(stardis, 0, (unsigned)sz, pwordexp, &idx));

end:
  return res;
error:
  goto end;
}

/* SOLID_PROG Name ProgName STL_filenames [PROG_PARAMS ...] */
static res_T
process_solid_prog
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  const char *lib_name, *desc_name;
  size_t sz;
  struct solid_prog* solid_prog;
  struct stardis_description_create_context ctx;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.fmed_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_solid_prog(stardis->allocator, &desc->d.solid_prog));
  solid_prog = desc->d.solid_prog;
  desc->type = DESC_MAT_SOLID_PROG;
  solid_prog->solid_id = allocate_stardis_medium_id(stardis);
  ASSERT(sz <= UINT_MAX);
  solid_prog->desc_id = (unsigned)sz;
  
  CHK_ARG(idx, "programmed solid name");
  ERR(description_set_name(stardis, &solid_prog->name, arg));
  if(find_description_by_name(stardis, &solid_prog->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  desc_name = arg;

  CHK_ARG(idx, "program name");
  ERR(str_set(&solid_prog->prog_name, arg));
  lib_name = arg;

  ERR(read_sides_and_files(stardis, 0, (unsigned)sz, pwordexp, &idx));

  /* store the end of line as args for custom init */
  ERR(set_argc_argv(stardis->allocator, &solid_prog->argc, &solid_prog->argv,
    pwordexp, idx));
  /* get the user-defined functions from the library */
  ERR(get_prog_common(lib_name, stardis, &solid_prog->program,
    &solid_prog->create, &solid_prog->release));
  GET_LIB_SYMBOL(solid_prog, lambda, stardis_conductivity);
  GET_LIB_SYMBOL(solid_prog, rho, stardis_volumic_mass);
  GET_LIB_SYMBOL(solid_prog, cp, stardis_calorific_capacity);
  GET_LIB_SYMBOL(solid_prog, delta, stardis_delta_solid);
  GET_LIB_SYMBOL(solid_prog, temp, stardis_medium_temperature);
  GET_LIB_SYMBOL(solid_prog, vpower, stardis_volumic_power);
  GET_LIB_SYMBOL(solid_prog, t_range, stardis_t_range);
  /* create and init custom data */
  ctx.name = desc_name;
  CREATE_DESC_DATA(solid_prog);

  solid_prog->t_range(solid_prog->prog_data, stardis->t_range);

  ERR(create_solver_solid_prog(stardis, solid_prog));

end:
  return res;
error:
  goto end;
}

/* FLUID Name rho cp Tinit Timposed STL_filenames */
static res_T
process_fluid
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  size_t sz;
  struct fluid* fluid;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.fmed_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_fluid(stardis->allocator, &desc->d.fluid));
  fluid = desc->d.fluid;
  desc->type = DESC_MAT_FLUID;
  fluid->fluid_id = allocate_stardis_medium_id(stardis);
  fluid->is_green = stardis->mode & (MODE_BIN_GREEN | MODE_GREEN);
  ASSERT(sz <= UINT_MAX);
  fluid->desc_id = (unsigned)sz;
  
  CHK_ARG(idx, "fluid name");
  ERR(description_set_name(stardis, &fluid->name, arg));
  if(find_description_by_name(stardis, &fluid->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_ARG(idx, "rho");
  res = cstr_to_double(arg, &fluid->rho);
  if(res != RES_OK
    || fluid->rho <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid rho: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "cp");
  res = cstr_to_double(arg, &fluid->cp);
  if(res != RES_OK
    || fluid->cp <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid cp: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "Tinit");
  res = cstr_to_double(arg, &fluid->tinit);
  if(res != RES_OK
    || fluid->tinit < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid Tinit: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  stardis->t_range[0] = MMIN(stardis->t_range[0], fluid->tinit);
  stardis->t_range[1] = MMAX(stardis->t_range[1], fluid->tinit);
  ERR(read_imposed_temperature(stardis, &fluid->imposed_temperature,
    pwordexp, &idx));
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

  ERR(read_sides_and_files(stardis, 0, (unsigned)sz, pwordexp, &idx));

end:
  return res;
error:
  goto end;
}

/* FLUID_PROG Name ProgName STL_filenames [PROG_PARAMS ...] */
static res_T
process_fluid_prog
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  struct description* desc;
  const char *lib_name, *desc_name;
  size_t sz;
  struct fluid_prog* fluid_prog;
  struct stardis_description_create_context ctx;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  stardis->counts.fmed_count++;

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  ERR(init_fluid_prog(stardis->allocator, &desc->d.fluid_prog));
  fluid_prog = desc->d.fluid_prog;
  desc->type = DESC_MAT_FLUID_PROG;
  fluid_prog->fluid_id = allocate_stardis_medium_id(stardis);
  ASSERT(sz <= UINT_MAX);
  fluid_prog->desc_id = (unsigned)sz;
  
  CHK_ARG(idx, "programmed fluid name");
  ERR(description_set_name(stardis, &fluid_prog->name, arg));
  if(find_description_by_name(stardis, &fluid_prog->name, desc)) {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  desc_name = arg;

  CHK_ARG(idx, "program name");
  ERR(str_set(&fluid_prog->prog_name, arg));
  lib_name = arg;

  ERR(read_sides_and_files(stardis, 0, (unsigned)sz, pwordexp, &idx));

  /* store the end of line as args for custom init */
  ERR(set_argc_argv(stardis->allocator, &fluid_prog->argc, &fluid_prog->argv,
    pwordexp, idx));
  /* get the user-defined functions from the library */
  ERR(get_prog_common(lib_name, stardis, &fluid_prog->program,
    &fluid_prog->create, &fluid_prog->release));
  GET_LIB_SYMBOL(fluid_prog, rho, stardis_volumic_mass);
  GET_LIB_SYMBOL(fluid_prog, cp, stardis_calorific_capacity);
  GET_LIB_SYMBOL(fluid_prog, temp, stardis_medium_temperature);
  GET_LIB_SYMBOL(fluid_prog, t_range, stardis_t_range);
  /* create and init custom data */
  ctx.name = desc_name;
  CREATE_DESC_DATA(fluid_prog);

  fluid_prog->t_range(fluid_prog->prog_data, stardis->t_range);

  ERR(create_solver_fluid_prog(stardis, fluid_prog));

end:
  return res;
error:
  goto end;
}

/* SCALE scale_factor */
static res_T
process_scale
  (struct stardis* stardis,
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  if(stardis->scale_factor > 0) {
    logger_print(stardis->logger, LOG_ERROR,
      "SCALE cannot be specified twice\n");
    res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "scale factor");
  res = cstr_to_double(arg, &stardis->scale_factor);
  if(res != RES_OK
    || stardis->scale_factor <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid scale factor: %s\n", arg);
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
   wordexp_t* pwordexp)
{
  char* arg = NULL;
  size_t idx = 1;
  res_T res = RES_OK;

  ASSERT(stardis && pwordexp);

  if(stardis->trad_def) {
    logger_print(stardis->logger, LOG_ERROR,
      "TRAD cannot be specified twice\n");
    res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "Trad");
  res = cstr_to_double(arg, &stardis->trad);
  if(res != RES_OK
    || stardis->trad < 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid Trad: %s\n", arg);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_ARG(idx, "Trad reference");
  res = cstr_to_double(arg, &stardis->trad_ref);
  if(res != RES_OK
    || stardis->trad_ref < 0)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid Trad reference: %s\n", arg);
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
 * SOLID Name lambda rho cp delta Tinit Timposed volumic_power STL_sides_filenames
 * FLUID Name rho cp Tinit Timposed STL_filenames
 * H_BOUNDARY_FOR_SOLID Name ref_temperature emissivity specular_fraction hc T_env STL_sides_filenames
 * H_BOUNDARY_FOR_FLUID Name ref_temperature emissivity specular_fraction hc T_env STL_filenames
 * T_BOUNDARY_FOR_SOLID Name T STL_filenames
 * F_BOUNDARY_FOR_SOLID Name F STL_filenames
 * SOLID_FLUID_CONNECTION Name ref_temperature emissivity specular_fraction hc STL_filenames
 *
 * SOLID_PROG Name Libray STL_sides_filenames [ PROG_PARAMS ... ]
 * SOLID_PROG Name Libray STL_sides_filenames [ PROG_PARAMS ... ]
 * H_BOUNDARY_FOR_SOLID_PROG Name Libray STL_filenames [ PROG_PARAMS ... ]
 * H_BOUNDARY_FOR_FLUID_PROG Name Libray STL_filenames [ PROG_PARAMS ... ]
 * T_BOUNDARY_FOR_SOLID_PROG Name Libray STL_filenames [ PROG_PARAMS ... ]
 * F_BOUNDARY_FOR_SOLID_PROG Name Libray STL_filenames [ PROG_PARAMS ... ]
 * SOLID_FLUID_CONNECTION_PROG Name Libray STL_filenames [ PROG_PARAMS ... ]
 * SOLID_SOLID_CONNECTION_PROG Name Libray STL_filenames [ PROG_PARAMS ... ]
 *
 * SCALE scale_factor
 * TRAD Trad Trad_ref
 *
 * STL_sides_filenames = { { FRONT | BACK | BOTH } STL_filename }+
 * STL_filenames = { STL_filename }+
 */
static res_T
process_model_line
  (const char* file_name,
   const char* line,
   wordexp_t *pwordexp,
   struct stardis* stardis)
{
  res_T res = RES_OK;
  char* arg = NULL;
  size_t idx = 0;

  ASSERT(file_name && line && pwordexp && stardis);

  CHK_ARG(idx, "model line type");
  
  if(0 == strcasecmp(arg, "H_BOUNDARY_FOR_SOLID"))
    ERR(process_h(stardis, DESC_BOUND_H_FOR_SOLID, pwordexp));
  else if(0 == strcasecmp(arg, "H_BOUNDARY_FOR_SOLID_PROG"))
    ERR(process_h_prog(stardis, DESC_BOUND_H_FOR_SOLID_PROG, pwordexp));
  else if(0 == strcasecmp(arg, "H_BOUNDARY_FOR_FLUID"))
    ERR(process_h(stardis, DESC_BOUND_H_FOR_FLUID, pwordexp));
  else if(0 == strcasecmp(arg, "H_BOUNDARY_FOR_FLUID_PROG"))
    ERR(process_h_prog(stardis, DESC_BOUND_H_FOR_FLUID_PROG, pwordexp));
  else if(0 == strcasecmp(arg, "T_BOUNDARY_FOR_SOLID"))
    ERR(process_t(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "T_BOUNDARY_FOR_SOLID_PROG"))
    ERR(process_t_prog(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "F_BOUNDARY_FOR_SOLID"))
    ERR(process_flx(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "F_BOUNDARY_FOR_SOLID_PROG"))
    ERR(process_flx_prog(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "SOLID_FLUID_CONNECTION"))
    ERR(process_sfc(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "SOLID_FLUID_CONNECTION_PROG"))
    ERR(process_sfc_prog(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "SOLID_SOLID_CONNECTION"))
    ERR(process_ssc(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "SOLID_SOLID_CONNECTION_PROG"))
    ERR(process_ssc_prog(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "SOLID"))
    ERR(process_solid(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "SOLID_PROG"))
    ERR(process_solid_prog(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "FLUID"))
    ERR(process_fluid(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "FLUID_PROG"))
    ERR(process_fluid_prog(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "PROGRAM"))
    ERR(process_program(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "SCALE"))
    ERR(process_scale(stardis, pwordexp));
  else if(0 == strcasecmp(arg, "TRAD"))
    ERR(process_radiative(stardis, pwordexp));
  else {
    logger_print(stardis->logger, LOG_ERROR,
      "Unknown description type: %s\n", arg);
    res = RES_BAD_ARG;
    goto error;
  }

end:
  return res;
error:
  logger_print(stardis->logger, LOG_ERROR,
    "Invalid description line in model file '%s':\n", file_name);
  logger_print(stardis->logger, LOG_ERROR, "%s\n", line);
  goto end;
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

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

res_T
read_model
  (const struct darray_str* model_files,
   struct stardis* stardis)
{
  res_T res = RES_OK;
  const struct str* files = NULL;
  size_t i;
  FILE* f = NULL;
  struct txtrdr* txtrdr = NULL;
  wordexp_t pwordexp;
  int word_initialized = 0;

  ASSERT(model_files && stardis);
  files = darray_str_cdata_get(model_files);
  FOR_EACH(i, 0, darray_str_size_get(model_files)) {
    const char* name = str_cget(files + i);
    int fst = 1;
    f = fopen(name, "r");
    if(!f) {
      logger_print(stardis->logger, LOG_ERROR,
        "Cannot open model file '%s'\n", name);
      res = RES_IO_ERR;
      goto error;
    }
    txtrdr_stream(stardis->allocator, f, name, '#', &txtrdr);
    for(;;) {
      char* line;
      int flags = WRDE_NOCMD | WRDE_UNDEF;
      if(!fst) flags |= WRDE_REUSE;
      ERR(txtrdr_read_line(txtrdr));
      line = txtrdr_get_line(txtrdr);
      if(!line) break;
      word_initialized = 1;
      switch(wordexp(line, &pwordexp, flags)) {
        case 0: /* No error */
          break;
        case WRDE_NOSPACE: /* Ran out of memory.  */
          res = RES_MEM_ERR;
          goto error;
        case WRDE_BADCHAR: /* A metachar appears in the wrong place.  */
          logger_print(stardis->logger, LOG_ERROR,
            "%s: word expansion error: invalid character:\n", name);
          goto exp_error;
        case WRDE_BADVAL: /* Undefined var reference with WRDE_UNDEF.  */
          logger_print(stardis->logger, LOG_ERROR,
            "%s: word expansion error: undefined environment variable:\n", name);
          goto exp_error;
        case WRDE_CMDSUB: /* Command substitution with WRDE_NOCMD.  */
          logger_print(stardis->logger, LOG_ERROR,
            "%s: word expansion error: command substitution is not enabled:\n",
            name);
          goto exp_error;
        case WRDE_SYNTAX: /* Shell syntax error.  */
          logger_print(stardis->logger, LOG_ERROR,
            "%s: word expansion error: syntax error:\n", name);
          goto exp_error;
        default:
          FATAL("Unexpected return code.\n");
      }
      ERR(process_model_line(name, line, &pwordexp, stardis));
      fst = 0;
      continue;
exp_error:
      logger_print(stardis->logger, LOG_ERROR, "%s\n", line);
      res = RES_BAD_ARG;
      goto error;
    }
    txtrdr_ref_put(txtrdr);
    txtrdr = NULL;
    fclose(f);
    f = NULL;
  }
  if(stardis->scale_factor <= 0)
    stardis->scale_factor = STARDIS_DEFAULT_SCALE_FACTOR;
  logger_print(stardis->logger, LOG_OUTPUT,
    "Scaling factor is %g\n", stardis->scale_factor);
  logger_print(stardis->logger, LOG_OUTPUT,
    "Trad is %g, Trad reference is %g\n", stardis->trad, stardis->trad_ref);
  stardis->t_range[0] = MMIN(stardis->t_range[0], stardis->trad_ref);
  stardis->t_range[1] = MMAX(stardis->t_range[1], stardis->trad_ref);
  logger_print(stardis->logger, LOG_OUTPUT,
    "System T range is [%g %g]\n", SPLIT2(stardis->t_range));
  logger_print(stardis->logger, LOG_OUTPUT,
    "Picard order is %u\n", stardis->picard_order);

  ASSERT(!f && !txtrdr);
exit:
  if(word_initialized) wordfree(&pwordexp);
  return res;
error:
  if(f) fclose(f);
  if(txtrdr) txtrdr_ref_put(txtrdr);
  goto exit;
}

