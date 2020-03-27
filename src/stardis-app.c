/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com) */

#include "stardis-app.h"
#include "stardis-output.h"
#include "stardis-compute.h"
#include "stardis-intface.h"
#include "stardis-fluid.h"
#include "stardis-solid.h"

#include <star/sg3d_sdisXd_helper.h>

#include <rsys/text_reader.h>
#include <rsys/logger.h>

#include <string.h>

static const struct dummies DUMMIES_NULL = DUMMIES_NULL__;

static const struct counts COUNTS_NULL = COUNTS_NULL__;

/*******************************************************************************
 * Local Type; for documentation purpose
 * These values are used in dumps
 ******************************************************************************/
enum properties_conflict_t {
  NO_CONFLICT,
  BOUND_H_FOR_FLUID_BETWEEN_2_DEFS,
  BOUND_H_FOR_FLUID_BETWEEN_2_UNDEFS,
  BOUND_H_FOR_FLUID_ENCLOSING_SOLID,
  BOUND_H_FOR_SOLID_BETWEEN_2_DEFS,
  BOUND_H_FOR_SOLID_BETWEEN_2_UNDEFS,
  BOUND_H_FOR_SOLID_ENCLOSING_FLUID,
  BOUND_T_FOR_FLUID_BETWEEN_2_DEFS,
  BOUND_T_FOR_FLUID_BETWEEN_2_UNDEFS,
  BOUND_T_FOR_FLUID_ENCLOSING_SOLID,
  BOUND_T_FOR_SOLID_BETWEEN_2_DEFS,
  BOUND_T_FOR_SOLID_BETWEEN_2_UNDEFS,
  BOUND_T_FOR_SOLID_ENCLOSING_FLUID,
  BOUND_F_FOR_SOLID_BETWEEN_2_DEFS,
  BOUND_F_FOR_SOLID_BETWEEN_2_UNDEFS,
  BOUND_F_FOR_SOLID_ENCLOSING_FLUID,
  SFCONNECT_BETWEEN_2_SOLIDS,
  SFCONNECT_BETWEEN_2_FLUIDS,
  SFCONNECT_USED_AS_BOUNDARY,
  NO_BOUND_BETWEEN_FLUIDS,
  TRG_WITH_NO_PROPERTY,
  NO_BOUND_BETWEEN_FLUID_AND_UNDEF,
  NO_BOUND_BETWEEN_SOLID_AND_UNDEF,
  PROPERTIES_CONFLICT_COUNT__
};

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

static res_T
read_model
  (const struct darray_str* model_files,
   struct stardis* stardis)
{
  res_T res = RES_OK;
  const struct str* files = NULL;
  size_t i;
  FILE* f = NULL;
  struct txtrdr* txtrdr = NULL;

  ASSERT(model_files && stardis);
  files = darray_str_cdata_get(model_files);
  FOR_EACH(i, 0, darray_str_size_get(model_files)) {
    const char* name = str_cget(files + i);
    f = fopen(name, "r");
    if(!f) {
      logger_print(stardis->logger, LOG_ERROR,
        "Cannot open model file '%s'\n",
        name);
      res = RES_IO_ERR;
      goto error;
    }
    txtrdr_stream(stardis->allocator, f, name, '#', &txtrdr);
    for(;;) {
      char* line;
      ERR(txtrdr_read_line(txtrdr));
      line = txtrdr_get_line(txtrdr);
      if(!line) break;
      ERR(process_model_line(name, line, stardis));
    }
    txtrdr_ref_put(txtrdr);
    txtrdr = NULL;
    fclose(f);
    f = NULL;
  }

  ASSERT(!f && !txtrdr);
exit:
  return res;
error:
  if(f) fclose(f);
  if(txtrdr) txtrdr_ref_put(txtrdr);
  goto exit;
}

#define COUNT_SIDE(Rank) {\
  if(properties[(Rank)] == SG3D_UNSPECIFIED_PROPERTY) undef_count++;\
  else {\
    ASSERT(properties[(Rank)] < darray_descriptions_size_get(&stardis->descriptions));\
    ASSERT(descs[properties[(Rank)]].type == DESC_MAT_SOLID\
      || descs[properties[(Rank)]].type == DESC_MAT_FLUID);\
    if(descs[properties[(Rank)]].type == DESC_MAT_SOLID) solid_count++;\
    else fluid_count++;\
  }\
}

#define PRT_INCOHERENT_TRG() {\
  struct str str;\
  str_init(stardis->allocator, &str);\
  ERR(logger_print(stardis->logger, LOG_WARNING,\
    "Incoherent triangle description (%u)\n", itri));\
  if(!front_defined)\
    ERR(logger_print(stardis->logger, LOG_OUTPUT, "Front: undefined\n"));\
  else {\
    ERR(str_printf(&str, "Front: "));\
    ERR(print_description(&str, descs + properties[SG3D_FRONT]));\
    ERR(str_printf(&str, "\n"));\
    ERR(logger_print(stardis->logger, LOG_OUTPUT, "%s", str_cget(&str))); \
  }\
  str_clear(&str);\
  if(!back_defined)\
    ERR(logger_print(stardis->logger, LOG_OUTPUT, "Back: undefined\n"));\
  else {\
    ERR(str_printf(&str, "Back: "));\
    ERR(print_description(&str, descs + properties[SG3D_BACK]));\
    ERR(str_printf(&str, "\n"));\
    ERR(logger_print(stardis->logger, LOG_OUTPUT, "%s", str_cget(&str))); \
  }\
  str_clear(&str);\
  if(!connect_defined)\
    ERR(logger_print(stardis->logger, LOG_OUTPUT, "Connection: undefined\n"));\
  else {\
    ERR(str_printf(&str, "Connection: "));\
    ERR(print_description(&str, descs + properties[SG3D_INTFACE]));\
    ERR(str_printf(&str, "\n"));\
    ERR(logger_print(stardis->logger, LOG_OUTPUT, "%s", str_cget(&str))); \
  }\
  str_release(&str);\
}

static res_T
validate_properties
  (const unsigned itri,
   const unsigned properties[SG3D_PROP_TYPES_COUNT__],
   void* context,
   int* properties_conflict_status)
{
  res_T res = RES_OK;
  struct stardis* stardis = context;
  unsigned undef_count, solid_count, fluid_count, intface_count;
  const struct description* descs;
  const struct description* intface = NULL;
  int front_defined, back_defined, connect_defined;

  (void)itri;
  ASSERT(stardis && properties_conflict_status);
  descs = darray_descriptions_cdata_get(&stardis->descriptions);
  undef_count = solid_count = fluid_count = intface_count = 0;

  COUNT_SIDE(SG3D_FRONT);
  COUNT_SIDE(SG3D_BACK);
  if(properties[SG3D_INTFACE] == SG3D_UNSPECIFIED_PROPERTY)
    undef_count++;
  else intface_count++;

  front_defined = (properties[SG3D_FRONT] != SG3D_UNSPECIFIED_PROPERTY);
  back_defined = (properties[SG3D_BACK] != SG3D_UNSPECIFIED_PROPERTY);
  connect_defined = (properties[SG3D_INTFACE] != SG3D_UNSPECIFIED_PROPERTY);

  ASSERT(solid_count <= 2 && fluid_count <= 2 && intface_count <= 1);
  ASSERT(undef_count + solid_count + fluid_count + intface_count == 3);
  if(intface_count) {
    ASSERT(properties[SG3D_INTFACE] < darray_descriptions_size_get(&stardis->descriptions));
    intface = descs + properties[SG3D_INTFACE];
    switch (intface->type) {
    case DESC_BOUND_H_FOR_FLUID:
      if(!(solid_count == 0 && fluid_count == 1)) {
        PRT_INCOHERENT_TRG();
        logger_print(stardis->logger, LOG_ERROR,
          "Can only define a DESC_BOUND_H_FOR_FLUID boundary for a fluid region\n");
        if(solid_count + fluid_count == 2)
          *properties_conflict_status = BOUND_H_FOR_FLUID_BETWEEN_2_DEFS;
        else if(solid_count + fluid_count == 0)
          *properties_conflict_status = BOUND_H_FOR_FLUID_BETWEEN_2_UNDEFS;
        else if(solid_count == 1)
          *properties_conflict_status = BOUND_H_FOR_FLUID_ENCLOSING_SOLID;
        else FATAL("error:" STR(__FILE__) ":" STR(__LINE__)"\n");
        goto end;
      }
      break;
    case DESC_BOUND_H_FOR_SOLID:
      if(!(solid_count == 1 && fluid_count == 0)) {
        PRT_INCOHERENT_TRG();
        logger_print(stardis->logger, LOG_ERROR,
          "Can only define a DESC_BOUND_H_FOR_SOLID boundary for a solid region\n");
        if(solid_count + fluid_count == 2)
          *properties_conflict_status = BOUND_H_FOR_SOLID_BETWEEN_2_DEFS;
        else if(solid_count + fluid_count == 0)
          *properties_conflict_status = BOUND_H_FOR_SOLID_BETWEEN_2_UNDEFS;
        else if(fluid_count == 1)
          *properties_conflict_status = BOUND_H_FOR_SOLID_ENCLOSING_FLUID;
        else FATAL("error:" STR(__FILE__) ":" STR(__LINE__)"\n");
        goto end;
      }
      break;
    case DESC_BOUND_T_FOR_FLUID:
      if(!(solid_count == 0 && fluid_count == 1)) {
        PRT_INCOHERENT_TRG();
        logger_print(stardis->logger, LOG_ERROR,
          "Can only define a DESC_BOUND_T_FOR_FLUID boundary for a fluid region\n");
        if(solid_count + fluid_count == 2)
          *properties_conflict_status = BOUND_T_FOR_FLUID_BETWEEN_2_DEFS;
        else if(solid_count + fluid_count == 0)
          *properties_conflict_status = BOUND_T_FOR_FLUID_BETWEEN_2_UNDEFS;
        else if(solid_count == 1)
          *properties_conflict_status = BOUND_T_FOR_FLUID_ENCLOSING_SOLID;
        else FATAL("error:" STR(__FILE__) ":" STR(__LINE__)"\n");
        goto end;
      }
      break;
    case DESC_BOUND_T_FOR_SOLID:
      if(!(solid_count == 1 && fluid_count == 0)) {
        PRT_INCOHERENT_TRG();
        logger_print(stardis->logger, LOG_ERROR,
          "Can only define a DESC_BOUND_T_FOR_SOLID boundary for a solid region\n");
        if(solid_count + fluid_count == 2)
          *properties_conflict_status = BOUND_T_FOR_SOLID_BETWEEN_2_DEFS;
        else if(solid_count + fluid_count == 0)
          *properties_conflict_status = BOUND_T_FOR_SOLID_BETWEEN_2_UNDEFS;
        else if(fluid_count == 1)
          *properties_conflict_status = BOUND_T_FOR_SOLID_ENCLOSING_FLUID;
        else FATAL("error:" STR(__FILE__) ":" STR(__LINE__)"\n");
        goto end;
      }
      break;
    case DESC_BOUND_F_FOR_SOLID:
      if(!(solid_count == 1 && fluid_count == 0)) {
        PRT_INCOHERENT_TRG();
        logger_print(stardis->logger, LOG_ERROR,
          "Can only define a DESC_BOUND_F_FOR_SOLID boundary for a solid region\n");
        if(solid_count + fluid_count == 2)
          *properties_conflict_status = BOUND_F_FOR_SOLID_BETWEEN_2_DEFS;
        else if(solid_count + fluid_count == 0)
          *properties_conflict_status = BOUND_F_FOR_SOLID_BETWEEN_2_UNDEFS;
        else if(fluid_count == 1)
          *properties_conflict_status = BOUND_F_FOR_SOLID_ENCLOSING_FLUID;
        else FATAL("error:" STR(__FILE__) ":" STR(__LINE__)"\n");
        goto end;
      }
      break;
    case DESC_SOLID_FLUID_CONNECT:
      if(solid_count != 1 || fluid_count != 1) {
        PRT_INCOHERENT_TRG();
        logger_print(stardis->logger, LOG_ERROR,
          "Can only define a DESC_SOLID_FLUID_CONNECT between a fluid and a solid\n");
        if(solid_count == 2)
          *properties_conflict_status = SFCONNECT_BETWEEN_2_SOLIDS;
        else if(fluid_count == 2)
          *properties_conflict_status = SFCONNECT_BETWEEN_2_FLUIDS;
        else if(solid_count + fluid_count < 2)
          *properties_conflict_status = SFCONNECT_USED_AS_BOUNDARY;
        else FATAL("error:" STR(__FILE__) ":" STR(__LINE__)"\n");
        goto end;
      }
      break;
    default:
      FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
    }
  } else {
    /* No interface defined */
    ASSERT(intface_count == 0 && undef_count >= 1);
    if(fluid_count == 2) {
      *properties_conflict_status = NO_BOUND_BETWEEN_FLUIDS;
      goto end;
    }
    if(undef_count == 3) {
      *properties_conflict_status = TRG_WITH_NO_PROPERTY;
      goto end;
    }
    if(undef_count == 2) {
      ASSERT(fluid_count + solid_count == 1);
      if(fluid_count)
        *properties_conflict_status = NO_BOUND_BETWEEN_FLUID_AND_UNDEF;
      else *properties_conflict_status = NO_BOUND_BETWEEN_SOLID_AND_UNDEF;
      goto end;
    }
    /* Undef interface between solids is OK */
    if(undef_count == 1) CHK(solid_count == 2);
  }
  
end:
  return res;
error:
  goto end;
}

#undef COUNT_SIDE
#undef PRT_INCOHERENT_TRG

static struct sdis_interface*
geometry_get_interface
  (const size_t itri, void* ctx)
{
  struct darray_interface_ptrs* app_interface_data = ctx;
  ASSERT(app_interface_data
    && itri < darray_interface_ptrs_size_get(app_interface_data));
  return darray_interface_ptrs_cdata_get(app_interface_data)[itri];
}

static res_T
create_holder
  (struct stardis* stardis,
   struct dummies* dummies,
   const unsigned idx,
   const int is_green)
{
  res_T res = RES_OK;
  struct description* description;

  ASSERT(stardis && dummies
    && idx < darray_descriptions_size_get(&stardis->descriptions));

  description = darray_descriptions_data_get(&stardis->descriptions) + idx;
  switch (description->type) {
  case DESC_BOUND_H_FOR_SOLID:
    stardis->counts.hbound_count++;
    /* Create an external fluid with imposed temperature */
    ERR(create_stardis_fluid(stardis, NULL, 1, 1, is_green, 1, -1,
      description->d.h_boundary.imposed_temperature,
      &description->d.h_boundary.mat_id));
    logger_print(stardis->logger, LOG_OUTPUT,
      "External fluid (id=%u): rho=1 cp=1 T=%g\n",
      description->d.h_boundary.mat_id,
      description->d.h_boundary.imposed_temperature);
    break;
  case DESC_BOUND_H_FOR_FLUID:
    stardis->counts.hbound_count++;
    if(dummies->dummy_solid) {
      /* Reuse external dummy solid */
      description->d.t_boundary.mat_id = dummies->dummy_solid_id;
    } else {
      /* Create dummy solid */
      size_t sz = darray_media_ptr_size_get(&stardis->media);
      ERR(create_stardis_solid(stardis, NULL, 1, 1, 1, 1, is_green, 1, -1,
        -1, 0, &description->d.t_boundary.mat_id));
      dummies->dummy_solid = darray_media_ptr_data_get(&stardis->media)[sz];
      dummies->dummy_solid_id = description->d.t_boundary.mat_id;
      logger_print(stardis->logger, LOG_OUTPUT,
        "Dummy solid (id=%u): lambda=1 rho=1 cp=1\n",
        dummies->dummy_solid_id);
    }
    break;
  case DESC_BOUND_T_FOR_SOLID:
    stardis->counts.tbound_count++;
    if(dummies->dummy_fluid) {
      /* Reuse external dummy fluid */
      description->d.t_boundary.mat_id = dummies->dummy_fluid_id;
    } else {
      /* Create dummy fluid */
      size_t sz = darray_media_ptr_size_get(&stardis->media);
      ERR(create_stardis_fluid(stardis, NULL, 1, 1, is_green, 1, -1,
        -1, &description->d.t_boundary.mat_id));
      dummies->dummy_fluid = darray_media_ptr_data_get(&stardis->media)[sz];
      dummies->dummy_fluid_id = description->d.t_boundary.mat_id;
      logger_print(stardis->logger, LOG_OUTPUT,
        "Dummy fluid (id=%u): rho=1 cp=1\n", dummies->dummy_fluid_id);
    }
    break;
  case DESC_BOUND_T_FOR_FLUID:
    stardis->counts.tbound_count++;
    if(dummies->dummy_solid) {
      /* Reuse external dummy solid */
      description->d.t_boundary.mat_id = dummies->dummy_solid_id;
    } else {
      /* Create dummy solid */
      size_t sz = darray_media_ptr_size_get(&stardis->media);
      ERR(create_stardis_solid(stardis, NULL, 1, 1, 1, 1, is_green, 1, -1,
        -1, 0, &description->d.t_boundary.mat_id));
      dummies->dummy_solid = darray_media_ptr_data_get(&stardis->media)[sz];
      dummies->dummy_solid_id = description->d.t_boundary.mat_id;
      logger_print(stardis->logger, LOG_OUTPUT,
        "Dummy solid (id=%u): lambda=1 rho=1 cp=1\n",
        dummies->dummy_solid_id);
    }
    break;
  case DESC_BOUND_F_FOR_SOLID:
    stardis->counts.fbound_count++;
    if(dummies->dummy_fluid) {
      /* Reuse external dummy fluid */
      description->d.f_boundary.mat_id = dummies->dummy_fluid_id;
    } else {
      /* Create dummy fluid */
      size_t sz = darray_media_ptr_size_get(&stardis->media);
      ERR(create_stardis_fluid(stardis, NULL, 1, 1, is_green, 1, -1, -1,
        &description->d.f_boundary.mat_id));
      dummies->dummy_fluid = darray_media_ptr_data_get(&stardis->media)[sz];
      dummies->dummy_fluid_id = description->d.f_boundary.mat_id;
      logger_print(stardis->logger, LOG_OUTPUT,
        "Dummy fluid (id=%u): rho=1 cp=1\n", dummies->dummy_fluid_id);
    }
    break;
  case DESC_SOLID_FLUID_CONNECT:
    stardis->counts.sfconnect_count++;
    ASSERT(description->d.sf_connect.specular_fraction >= 0
      && description->d.sf_connect.specular_fraction <= 1);
    ASSERT(description->d.sf_connect.emissivity >= 0);
    ASSERT(description->d.sf_connect.hc >= 0);
    break;
  case DESC_MAT_SOLID:
    stardis->counts.smed_count++;
    ERR(create_stardis_solid(stardis,
      &description->d.solid.name,
      description->d.solid.lambda,
      description->d.solid.rho,
      description->d.solid.cp,
      description->d.solid.delta,
      is_green,
      0,
      description->d.solid.tinit,
      description->d.solid.imposed_temperature,
      description->d.solid.vpower,
      &description->d.solid.solid_id));
    break;
  case DESC_MAT_FLUID:
    stardis->counts.fmed_count++;
    ERR(create_stardis_fluid(stardis,
      &description->d.fluid.name,
      description->d.fluid.rho,
      description->d.fluid.cp,
      is_green,
      0,
      description->d.fluid.tinit,
      description->d.fluid.imposed_temperature,
      &description->d.fluid.fluid_id));
    break;
  default:
    FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
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
stardis_init
  (const struct args* args,
   struct logger* logger,
   struct mem_allocator* allocator,
   struct stardis* stardis)
{
  res_T res = RES_OK;
  struct sg3d_sdisXd_scene_create_context create_context;
  struct dummies dummies = DUMMIES_NULL;
  struct htable_intface htable_interfaces;
  struct str str;
  unsigned i, vcount, tcount, count;
  int is_for_compute;

  ASSERT(args && logger && allocator && stardis);

  /* Init everithing that cannot fail */
  stardis->logger = logger;
  stardis->allocator = allocator;
  htable_intface_init(stardis->allocator, &htable_interfaces);
  darray_descriptions_init(stardis->allocator, &stardis->descriptions);
  stardis->probe[0] = args->probe[0];
  stardis->probe[1] = args->probe[1];
  stardis->probe[2] = args->probe[2];
  stardis->probe[3] = args->probe[3];
  stardis->dev = NULL;
  stardis->sdis_scn = NULL;
  stardis->mode = args->mode;
  stardis->counts = COUNTS_NULL;
  init_camera(&stardis->camera);
  str_init(stardis->allocator, &stardis->solve_name);
  darray_size_t_init(stardis->allocator, &stardis->compute_surface.primitives);
  darray_sides_init(stardis->allocator, &stardis->compute_surface.sides);
  darray_uint_init(stardis->allocator, &stardis->compute_surface.err_triangles);
  stardis->samples = args->samples;
  stardis->nthreads = args->nthreads;
  stardis->scale_factor = args->scale_factor;
  stardis->radiative_temp[0] = args->radiative_temp[0];
  stardis->radiative_temp[1] = args->radiative_temp[1];
  stardis->dump_paths = SDIS_HEAT_PATH_NONE;
  if(args->dump_paths & DUMP_ERROR)
    stardis->dump_paths |= SDIS_HEAT_PATH_FAILURE;
  if(args->dump_paths & DUMP_SUCCESS)
    stardis->dump_paths |= SDIS_HEAT_PATH_SUCCESS;
  stardis->verbose = args->verbose;
  darray_media_ptr_init(stardis->allocator, &stardis->media);
  
  is_for_compute = (stardis->mode & COMPUTE_MODES);
  ERR(init_geometry(stardis->logger, stardis->allocator, stardis->verbose,
    &stardis->geometry));

  if(args->mode & IR_COMPUTE) {
    ERR(parse_camera(stardis->logger, args->camera, &stardis->camera));
  }
  else if(args->mode & MEDIUM_COMPUTE) {
    ERR(str_set(&stardis->solve_name, args->medium_name));
  }
  else if(args->mode & SURFACE_COMPUTE_MODES) {
    ERR(str_set(&stardis->solve_name, args->solve_filename));
  }
  ERR(read_model(&args->model_files, stardis));

  ERR(sdis_device_create(stardis->logger, stardis->allocator, args->nthreads,
    args->verbose, &stardis->dev));

  /* Create media and property holders for those found in descriptions */
  str_init(stardis->allocator, &str);
  for(i = 0; i < darray_descriptions_size_get(&stardis->descriptions); i++) {
    ERR(create_holder(stardis, &dummies, i, stardis->mode & GREEN_MODE));
    str_clear(&str);
    str_printf(&str, "Description %d: ", i);
    ERR(print_description(&str,
      darray_descriptions_cdata_get(&stardis->descriptions) + i));
    ERR(str_printf(&str, "\n"));
    logger_print(stardis->logger, LOG_OUTPUT, "%s", str_cget(&str));
  }
  str_release(&str);

  create_context.geometry = stardis->geometry.sg3d;
  create_context.app_interface_getter = geometry_get_interface;
  create_context.app_interface_data = &stardis->geometry.interf_bytrg;
  ERR(sg3d_geometry_get_unique_vertices_count(stardis->geometry.sg3d, &vcount));
  ERR(sg3d_geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tcount));

  ERR(sg3d_geometry_validate_properties(stardis->geometry.sg3d,
     validate_properties, stardis));
  ERR(sg3d_geometry_get_unique_triangles_with_properties_conflict_count(
    stardis->geometry.sg3d, &count));
  if(count) {
    logger_print(stardis->logger, (is_for_compute ? LOG_ERROR : LOG_WARNING),
      "Properties conflicts found in the the model (%u triangles).\n", count);
    if(is_for_compute) {
      res = RES_BAD_ARG;
      goto error;
    }
  }

  for(i = 0; i < tcount; ++i) {
    ERR(create_intface(stardis, i, &htable_interfaces));
  }

  /* If computation is on a compute surface, read it */
  if(args->mode & SURFACE_COMPUTE_MODES) {
    ASSERT(!str_is_empty(&stardis->solve_name));
    ERR(read_compute_surface(stardis));
    /* Check compute surface  */
    ERR(sg3d_geometry_validate_properties(stardis->geometry.sg3d,
      validate_properties, stardis));
    ERR(sg3d_geometry_get_unique_triangles_with_properties_conflict_count(
      stardis->geometry.sg3d, &count));
    if(count) {
      logger_print(stardis->logger, (is_for_compute ? LOG_ERROR : LOG_WARNING),
        "Invalid compute region defined by file '%s'.\n",
        str_cget(&stardis->solve_name));
      logger_print(stardis->logger, (is_for_compute ? LOG_ERROR : LOG_WARNING),
        "The file contains %u triangles not in the model.\n", count);
      if(is_for_compute) {
        res = RES_BAD_ARG;
        goto error;
      }
    }
  }
  /* If computation is on a volume, check medium is known */
  if(args->mode & MEDIUM_COMPUTE) {
    if(!find_medium_by_name(stardis, &stardis->solve_name, NULL)) {
      logger_print(stardis->logger, (is_for_compute ? LOG_ERROR : LOG_WARNING),
        "Cannot find medium '%s'\n", str_cget(&stardis->solve_name));
      res = RES_BAD_ARG;
      goto error;
    }
  }

  /* Check conflicts */
  ERR(sg3d_geometry_get_unique_triangles_with_merge_conflict_count(
    stardis->geometry.sg3d, &count));
  if(count) {
    logger_print(stardis->logger, (is_for_compute ? LOG_ERROR : LOG_WARNING),
      "Merge conflicts found in the the model (%u triangles).\n", count);
    if(is_for_compute) {
      res = RES_BAD_ARG;
      goto error;
    }
  }

  if(is_for_compute) {
    ASSERT(darray_interface_ptrs_size_get(&stardis->geometry.interf_bytrg)
      == tcount);
    res = sdis_scene_create(stardis->dev, tcount,
      sg3d_sdisXd_geometry_get_indices,
      sg3d_sdisXd_geometry_get_interface,
      vcount, sg3d_sdisXd_geometry_get_position,
      &create_context, &stardis->sdis_scn);
    if(res != RES_OK) {
      logger_print(stardis->logger, LOG_ERROR,
        "Cannot create the stardis solver scene, no computation possible.\n");
      goto error;
    }
  }
  
exit:
  htable_intface_release(&htable_interfaces);
  return res;
error:
  stardis_release(stardis);
  goto exit;
}

void
stardis_release(struct stardis* stardis)
{
  size_t i;

  ASSERT(stardis);

  if(stardis->dev) SDIS(device_ref_put(stardis->dev));
  if(stardis->sdis_scn) SDIS(scene_ref_put(stardis->sdis_scn));
  str_release(&stardis->solve_name);
  darray_descriptions_release(&stardis->descriptions);
  release_geometry(&stardis->geometry);
  darray_size_t_release(&stardis->compute_surface.primitives);
  darray_sides_release(&stardis->compute_surface.sides);
  darray_uint_release(&stardis->compute_surface.err_triangles);
  FOR_EACH(i, 0, darray_media_ptr_size_get(&stardis->media))
    SDIS(medium_ref_put(darray_media_ptr_data_get(&stardis->media)[i]));
  darray_media_ptr_release(&stardis->media);
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

  if (len * sizeof(char) <= str->allocated)
    return RES_OK;

  mod = len % alloc_granularity;
  new_len = !mod ? len : len - mod + alloc_granularity;
  new_size = new_len * sizeof(char);
  buf = MEM_ALLOC(str->allocator, new_size);
  if (!buf)
    return RES_MEM_ERR;

  if (keep_old) {
    strncpy(buf, str->cstr, new_len - 1);
    buf[new_len - 1] = '\0';
  }

  str->allocated = new_len * sizeof(char);
  if (str->cstr != str->buffer)
    MEM_RM(str->allocator, str->cstr);

  str->cstr = buf;
  return RES_OK;
}

res_T
str_printf
(struct str* str,
  const char* fmt, ...)
{
  res_T res = RES_OK;
  size_t initial_len, left;
  va_list args_cp;
  size_t needed = 0;
  va_list args;
  ASSERT(str && fmt);
  va_start(args, fmt);

  initial_len = str_len(str);
  ASSERT(str->allocated >= initial_len);
  left = str->allocated - initial_len;
  if (left) {
    VA_COPY(args_cp, args);
    needed = (size_t)vsnprintf(str->cstr + initial_len, left, fmt, args_cp);
  }
  if (needed >= left) {
    ERR(ensure_allocated(str, initial_len + needed + 1, 1));
    ASSERT(str->allocated >= initial_len);
    left = str->allocated - initial_len;
    needed = (size_t)vsnprintf(str->cstr + initial_len, left, fmt, args);
    CHK(needed < left);
  }
  va_end(args);
end:
  return res;
error:
  goto end;
}
