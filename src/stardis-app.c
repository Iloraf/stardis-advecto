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

#include "stardis-app.h"
#include "stardis-output.h"
#include "stardis-compute.h"
#include "stardis-intface.h"
#include "stardis-fluid.h"
#include "stardis-solid.h"

#include <star/senc3d.h>
#include <star/sg3d_sencXd_helper.h>
#include <star/sg3d_sdisXd_helper.h>

#include <rsys/str.h>
#include <rsys/text_reader.h>
#include <rsys/logger.h>
#include <rsys/double2.h>
#include <rsys/double3.h>

#include <string.h>

static const struct dummies DUMMIES_NULL = DUMMIES_NULL__;

static const struct counts COUNTS_NULL = COUNTS_NULL__;

/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static res_T
read_model
  (const struct darray_str* model_files,
   struct stardis* stardis,
   struct dummies* dummies)
{
  res_T res = RES_OK;
  const struct str* files = NULL;
  size_t i;
  FILE* f = NULL;
  struct txtrdr* txtrdr = NULL;

  ASSERT(model_files && stardis && dummies);
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
      ERR(process_model_line(name, line, stardis, dummies));
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
  stardis->tmax = MMAX(stardis->tmax, stardis->trad);
  stardis->tmax = MMAX(stardis->tmax, stardis->trad_ref);
  logger_print(stardis->logger, LOG_OUTPUT,
    "System Tmax is %g\n", stardis->tmax);

  ASSERT(!f && !txtrdr);
exit:
  return res;
error:
  if(f) fclose(f);
  if(txtrdr) txtrdr_ref_put(txtrdr);
  goto exit;
}

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
check_delta_and_create_solid
  (struct stardis* stardis,
   struct description* description)
{
  res_T res = RES_OK;
  struct senc3d_enclosure* enc = NULL;
  double ratio, delta_range[2] = { DBL_MAX, -DBL_MAX };
  const double acceptance_ratio = 3;
  struct senc3d_enclosure_header header;

  ASSERT(stardis && description && description->type == DESC_MAT_SOLID);

  /* Create solid to have ID informed */
  /* Check if delta can fit possible multiple enclosures */
  if(stardis->senc3d_scn) {
    /* Due to previous errors, senc3d_scn can be unavailable */
    unsigned e, ecount = 0;
    const unsigned desc_id = description->d.solid.desc_id;

    /* The enclosures where created using description ids */
    ERR(senc3d_scene_get_enclosure_count_by_medium(stardis->senc3d_scn,
      desc_id, &ecount));
    if(ecount == 0) {
      unsigned ccount;
      ERR(sg3d_geometry_get_unique_triangles_with_properties_conflict_count(
        stardis->geometry.sg3d, &ccount));
      CHK(ccount == 0); /* This solid can only be unused if in conflict*/
    } else {
      int external = 0;
      FOR_EACH(e, 0, ecount) {
        ERR(senc3d_scene_get_enclosure_by_medium(stardis->senc3d_scn, desc_id,
          e, &enc));
        ERR(senc3d_enclosure_get_header(enc, &header));
        if(header.is_infinite) {
          /* External solid, volume is negative and no delta walk expected */
          external = 1;
        } else {
          double d = header.volume / (header.area * 6);
          ASSERT(d >= 0);
          delta_range[0] = MMIN(delta_range[0], d);
          delta_range[1] = MMAX(delta_range[1], d);
        }
        ERR(senc3d_enclosure_ref_put(enc));
        enc = NULL;
      }
      if(ecount > 1 || !external) {
        ASSERT(0 < delta_range[0] && delta_range[0] <= delta_range[1]);
        ratio = delta_range[1] / delta_range[0];
        if(ratio > acceptance_ratio)
          logger_print(stardis->logger, LOG_WARNING,
            "Solid '%s' is used in %u different enclosures that have different "
            "delta requirements.\n",
            str_cget(&description->d.solid.name), ecount);
        /* Delta needs to be substituted with actual value */
        if(description->d.solid.delta == DELTA_AUTO) {
          description->d.solid.delta = delta_range[0];
          logger_print(stardis->logger, LOG_OUTPUT,
            "Auto delta for solid '%s' set to %g\n",
            str_cget(&description->d.solid.name), description->d.solid.delta);
        } else {
          int too_small
            = (delta_range[0] > description->d.solid.delta * acceptance_ratio);
          int too_big
            = (delta_range[0] * acceptance_ratio < description->d.solid.delta);
          /* Check if user delta is OK */
          if(too_small || too_big) {
            logger_print(stardis->logger, LOG_WARNING,
              "User delta for solid '%s' seems too %s: %g; "
              "auto delta would have set it to %g.\n",
              str_cget(&description->d.solid.name), (too_big ? "big" : "small"),
              description->d.solid.delta, delta_range[0]);
          }
        }
      }
    }
  }
  ERR(create_solver_solid(stardis, &description->d.solid));

end:
  if(enc) SENC3D(enclosure_ref_put(enc));
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
  res_T tmp_res, res = RES_OK;
  struct sg3d_sdisXd_scene_create_context create_context;
  struct dummies dummies = DUMMIES_NULL;
  struct htable_intface htable_interfaces;
  struct str str;
  unsigned i, vcount, tcount, ocount, count;
  int is_for_compute;

  ASSERT(args && logger && allocator && stardis);

  str_init(allocator, &str);
  /* Init everithing that cannot fail */
  stardis->logger = logger;
  stardis->allocator = allocator;
  htable_intface_init(stardis->allocator, &htable_interfaces);
  darray_descriptions_init(stardis->allocator, &stardis->descriptions);
  d3_set(stardis->probe, args->pos_and_time);
  d2_set(stardis->time_range, args->pos_and_time + 3);
  stardis->dev = NULL;
  stardis->sdis_scn = NULL;
  stardis->senc3d_scn = NULL;
  stardis->mode = args->mode;
  stardis->counts = COUNTS_NULL;
  init_camera(stardis->allocator, &stardis->camera);
  str_init(stardis->allocator, &stardis->solve_name);
  str_init(stardis->allocator, &stardis->paths_filename);
  str_init(stardis->allocator, &stardis->bin_green_filename);
  str_init(stardis->allocator, &stardis->end_paths_filename);
  str_init(stardis->allocator, &stardis->rndgen_state_in_filename);
  str_init(stardis->allocator, &stardis->rndgen_state_out_filename);
  str_init(stardis->allocator, &stardis->chunks_prefix);
  darray_size_t_init(stardis->allocator, &stardis->compute_surface.primitives);
  darray_sides_init(stardis->allocator, &stardis->compute_surface.sides);
  darray_uint_init(stardis->allocator, &stardis->compute_surface.err_triangles);
  stardis->compute_surface.area = 0;
  stardis->samples = args->samples;
  stardis->nthreads = args->nthreads;
  stardis->scale_factor = -1; /* invalid value */
  stardis->trad = STARDIS_DEFAULT_TRAD;
  stardis->trad_ref = STARDIS_DEFAULT_TRAD_REFERENCE;
  stardis->trad_def = 0;
  stardis->dump_paths = SDIS_HEAT_PATH_NONE;
  if(args->dump_paths & DUMP_ERROR)
    stardis->dump_paths |= SDIS_HEAT_PATH_FAILURE;
  if(args->dump_paths & DUMP_SUCCESS)
    stardis->dump_paths |= SDIS_HEAT_PATH_SUCCESS;
  stardis->next_medium_id = 0;
  stardis->undefined_medium_behind_boundary_id = SENC3D_UNSPECIFIED_MEDIUM;
  stardis->verbose = args->verbose;
  darray_media_ptr_init(stardis->allocator, &stardis->media);

  /* If a dump is expected, we won't process any computation */
  is_for_compute =
    (stardis->mode & COMPUTE_MODES) && !(stardis->mode & MODE_DUMP_VTK);

  ERR(sdis_device_create(stardis->logger, stardis->allocator, stardis->nthreads,
    args->verbose, &stardis->dev));

  ERR(init_geometry(stardis->logger, stardis->allocator, stardis->verbose,
    &stardis->geometry));

  if(args->mode & MODE_IR_COMPUTE) {
    ERR(parse_camera(stardis->logger, args->camera, stardis));
  }
  else if(args->mode & MODE_MEDIUM_COMPUTE) {
    ERR(str_set(&stardis->solve_name, args->medium_name));
  }
  else if(args->mode & MODE_PROBE_COMPUTE_ON_INTERFACE && args->medium_name) {
    ERR(str_set(&stardis->solve_name, args->medium_name));
  }
  else if(args->mode & SURFACE_COMPUTE_MODES) {
    ERR(str_set(&stardis->solve_name, args->solve_filename));
  }
  else if(args->mode & MODE_DUMP_C_CHUNKS) {
    ERR(str_set(&stardis->chunks_prefix, args->chunks_prefix));
  }
  ERR(read_model(&args->model_files, stardis, &dummies));

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
      "Property conflicts found in the model (%u triangles).\n", count);
    if(is_for_compute) {
      res = RES_BAD_ARG;
      goto error;
    }
  }

  /* If computation is on a compute surface, read it */
  if(args->mode & SURFACE_COMPUTE_MODES) {
    unsigned save_count = count;
    ASSERT(!str_is_empty(&stardis->solve_name));
    ERR(read_compute_surface(stardis));
    /* Check compute surface  */
    ERR(sg3d_geometry_validate_properties(stardis->geometry.sg3d,
      validate_properties, stardis));
    ERR(sg3d_geometry_get_unique_triangles_with_properties_conflict_count(
      stardis->geometry.sg3d, &count));
    ASSERT(count >= save_count);
    if(save_count != count) {
      logger_print(stardis->logger, (is_for_compute ? LOG_ERROR : LOG_WARNING),
        "Invalid compute region defined by file '%s'.\n",
        str_cget(&stardis->solve_name));
      logger_print(stardis->logger, (is_for_compute ? LOG_ERROR : LOG_WARNING),
        "The file contains %u triangles not in the model.\n", count - save_count);
      if(is_for_compute) {
        res = RES_BAD_ARG;
        goto error;
      }
    }
    logger_print(stardis->logger, LOG_OUTPUT, "Compute surface area is %g m2\n",
      stardis->compute_surface.area);
  }

  /* Create enclosures */
  tmp_res = init_enclosures(stardis);
  if(tmp_res != RES_OK && is_for_compute) {
    res = tmp_res;
    goto error;
  }
  ERR(senc3d_scene_get_overlapping_triangles_count(stardis->senc3d_scn, &ocount));
  if(ocount) {
    logger_print(stardis->logger, (is_for_compute ? LOG_ERROR : LOG_WARNING),
      "Scene contains %u overlapping triangles.\n",
      ocount);
    if(is_for_compute) {
      res = RES_BAD_ARG;
      goto error;
    }
  }

  /* Create solids and log model information */
  for(i = 0; i < darray_descriptions_size_get(&stardis->descriptions); i++) {
    struct description* desc =
      darray_descriptions_data_get(&stardis->descriptions) + i;
    if(desc->type == DESC_MAT_SOLID) {
      tmp_res = check_delta_and_create_solid(stardis, desc);
      if(tmp_res != RES_OK && is_for_compute) {
        res = tmp_res;
        goto error;
      }
    }
    ERR(str_print_description(&str, i, desc));
    logger_print(stardis->logger, LOG_OUTPUT, "%s\n", str_cget(&str));
  }

  if(is_for_compute) {
    for(i = 0; i < tcount; ++i) {
      ERR(create_intface(stardis, i, &htable_interfaces));
    }
    if(args->paths_filename) {
      ERR(str_set(&stardis->paths_filename, args->paths_filename));
    }
    if(args->bin_green_filename) {
      ERR(str_set(&stardis->bin_green_filename, args->bin_green_filename));
    }
    if(args->end_paths_filename) {
      ERR(str_set(&stardis->end_paths_filename, args->end_paths_filename));
    }
    if(args->rndgen_state_in_filename) {
      ERR(str_set(&stardis->rndgen_state_in_filename,
        args->rndgen_state_in_filename));
    }
    if(args->rndgen_state_out_filename) {
      ERR(str_set(&stardis->rndgen_state_out_filename,
        args->rndgen_state_out_filename));
    }
  }

  /* If computation is on a volume, check medium is known */
  if(args->mode & MODE_MEDIUM_COMPUTE) {
    if(!find_medium_by_name(stardis, &stardis->solve_name, NULL)) {
      logger_print(stardis->logger, (is_for_compute ? LOG_ERROR : LOG_WARNING),
        "Cannot find medium '%s'\n", str_cget(&stardis->solve_name));
      res = RES_BAD_ARG;
      goto error;
    }
  }

  if(is_for_compute) {
    struct sdis_scene_create_args scn_args = SDIS_SCENE_CREATE_ARGS_DEFAULT;
    ASSERT(darray_interface_ptrs_size_get(&stardis->geometry.interf_bytrg)
      == tcount);
    scn_args.get_indices = sg3d_sdisXd_geometry_get_indices;
    scn_args.get_interface = sg3d_sdisXd_geometry_get_interface;
    scn_args.get_position = sg3d_sdisXd_geometry_get_position;
    scn_args.nprimitives = tcount;
    scn_args.nvertices = vcount;
    scn_args.fp_to_meter = stardis->scale_factor;
    scn_args.trad.temperature = stardis->trad;
    scn_args.trad.reference = stardis->trad_ref;
    scn_args.tmax = stardis->tmax;
    scn_args.context = &create_context;
    res = sdis_scene_create(stardis->dev, &scn_args, &stardis->sdis_scn);
    if(res != RES_OK) {
      logger_print(stardis->logger, LOG_ERROR,
        "Cannot create the stardis solver scene.\n");
      goto error;
    }
  }

exit:
  str_release(&str);
  htable_intface_release(&htable_interfaces);
  return res;
error:
  stardis_release(stardis);
  goto exit;
}

void
stardis_release
  (struct stardis* stardis)
{
  size_t i;

  ASSERT(stardis);

  if(stardis->dev) SDIS(device_ref_put(stardis->dev));
  if(stardis->sdis_scn) SDIS(scene_ref_put(stardis->sdis_scn));
  if(stardis->senc3d_scn) SENC3D(scene_ref_put(stardis->senc3d_scn));
  str_release(&stardis->solve_name);
  str_release(&stardis->paths_filename);
  str_release(&stardis->bin_green_filename);
  str_release(&stardis->end_paths_filename);
  str_release(&stardis->chunks_prefix);
  darray_descriptions_release(&stardis->descriptions);
  release_geometry(&stardis->geometry);
  darray_size_t_release(&stardis->compute_surface.primitives);
  darray_sides_release(&stardis->compute_surface.sides);
  darray_uint_release(&stardis->compute_surface.err_triangles);
  FOR_EACH(i, 0, darray_media_ptr_size_get(&stardis->media)) {
    if(darray_media_ptr_data_get(&stardis->media)[i])
      SDIS(medium_ref_put(darray_media_ptr_data_get(&stardis->media)[i]));
  }
  darray_media_ptr_release(&stardis->media);
  release_camera(&stardis->camera);
}

res_T
init_enclosures
  (struct stardis* stardis)
{
  res_T res = RES_OK;
  unsigned tsz, vsz;
  struct senc3d_device* senc_dev = NULL;

  ERR(sg3d_geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tsz));
  ERR(sg3d_geometry_get_unique_vertices_count(stardis->geometry.sg3d, &vsz));
  ERR(senc3d_device_create(stardis->logger, stardis->allocator,
    stardis->nthreads, stardis->verbose, &senc_dev));
  stardis->undefined_medium_behind_boundary_id
    = allocate_stardis_medium_id(stardis);
  ERR(senc3d_scene_create(senc_dev,
    SENC3D_CONVENTION_NORMAL_BACK | SENC3D_CONVENTION_NORMAL_OUTSIDE,
    tsz, sg3d_sencXd_geometry_get_indices, sg3d_sencXd_geometry_get_media,
    vsz, sg3d_sencXd_geometry_get_position, stardis->geometry.sg3d,
    &stardis->senc3d_scn));
exit:
  if(senc_dev) senc3d_device_ref_put(senc_dev);
  return res;
error:
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

res_T
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

  (void)itri;
  ASSERT(stardis && properties_conflict_status);
  descs = darray_descriptions_cdata_get(&stardis->descriptions);
  *properties_conflict_status = NO_PROPERTY_CONFLICT;
  undef_count = solid_count = fluid_count = intface_count = 0;

  COUNT_SIDE(SG3D_FRONT);
  COUNT_SIDE(SG3D_BACK);
  if(properties[SG3D_INTFACE] == SG3D_UNSPECIFIED_PROPERTY)
    undef_count++;
  else intface_count++;

  ASSERT(solid_count <= 2 && fluid_count <= 2 && intface_count <= 1);
  ASSERT(undef_count + solid_count + fluid_count + intface_count == 3);
  if(intface_count) {
    ASSERT(properties[SG3D_INTFACE]
      < darray_descriptions_size_get(&stardis->descriptions));
    intface = descs + properties[SG3D_INTFACE];
    switch (intface->type) {
    case DESC_BOUND_H_FOR_FLUID:
      if(!(solid_count == 0 && fluid_count == 1)) {
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
    case DESC_BOUND_T_FOR_SOLID:
      if(!(solid_count == 1 && fluid_count == 0)) {
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
        if(solid_count == 2)
          *properties_conflict_status = SFCONNECT_BETWEEN_2_SOLIDS;
        else if(fluid_count == 2)
          *properties_conflict_status = SFCONNECT_BETWEEN_2_FLUIDS;
        else if(solid_count + fluid_count == 1)
          *properties_conflict_status = SFCONNECT_USED_AS_BOUNDARY;
        else if(solid_count + fluid_count == 0)
          *properties_conflict_status = SFCONNECT_BETWEEN_2_UNDEFS;
        else FATAL("error:" STR(__FILE__) ":" STR(__LINE__)"\n");
        goto end;
      }
      break;
    case DESC_SOLID_SOLID_CONNECT:
      if(solid_count != 2) {
        /*if(soli_count == 1 && fluid_count == 1)*/
          /**properties_conflict_status = SSCONNECT_BETWEEN_SOLID_AND_FLUID;*/
        goto end;
      }
      break;
    default:
      FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
    }
  } else {
    /* No interface defined */
    ASSERT(intface_count == 0 && undef_count >= 1);
    if(undef_count == 3) {
      *properties_conflict_status = TRG_WITH_NO_PROPERTY;
      goto end;
    }
    if(fluid_count == 2) {
      *properties_conflict_status = NO_CONNECTION_BETWEEN_2_FLUIDS;
      goto end;
    }
    if(undef_count == 2) {
      ASSERT(fluid_count + solid_count == 1);
      if(fluid_count)
        *properties_conflict_status = NO_BOUND_BETWEEN_FLUID_AND_UNDEF;
      else *properties_conflict_status = NO_BOUND_BETWEEN_SOLID_AND_UNDEF;
      goto end;
    }
    if(undef_count == 1 && solid_count == 1 && fluid_count == 1) {
      *properties_conflict_status = NO_CONNECTION_BETWEEN_SOLID_AND_FLUID;
      goto end;
    }
    /* Undef interface between solids is OK */
    CHK(solid_count == 2);
  }

end:
  return res;
}

#undef COUNT_SIDE
