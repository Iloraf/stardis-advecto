/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com) */

#include "stardis-app.h"
#include "stardis-output.h"
#include "stardis-compute.h"

#include <rsys/text_reader.h>

#include <string.h>

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
      fprintf(stderr, "Cannot open model file '%s'\n", name);
      res = RES_IO_ERR;
      goto error;
    }
    txtrdr_stream(stardis->allocator, f, name, '#', &txtrdr);
    for(;;) {
      char* line;
      ERR(txtrdr_read_line(txtrdr));
      line = txtrdr_get_line(txtrdr);
      if(!line) break;
      ERR(process_model_line(line, stardis));
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
  if(properties[(Rank)] == SG3D_UNSPECIFIED_PROPERTY) u_count++;\
  else {\
    ASSERT(properties[(Rank)] < darray_descriptions_size_get(descriptions));\
    ASSERT(descs[properties[(Rank)]].type == DESC_MAT_SOLID\
      || descs[properties[(Rank)]].type == DESC_MAT_FLUID);\
    if(descs[properties[(Rank)]].type == DESC_MAT_SOLID) s_count++;\
    else f_count++;\
  }\
}

static res_T
validate_properties
  (const unsigned itri,
   const unsigned properties[SG3D_PROP_TYPES_COUNT__],
   void* context,
   int* properties_conflict_status)
{
  struct darray_descriptions* descriptions = context;
  unsigned u_count, s_count, f_count, i_count;
  const struct description* descs, * d = NULL;

  (void)itri;
  ASSERT(descriptions && properties_conflict_status);
  descs = darray_descriptions_cdata_get(descriptions);
  u_count = s_count = f_count = i_count = 0;

  COUNT_SIDE(SG3D_FRONT);
  COUNT_SIDE(SG3D_BACK);
  if(properties[SG3D_INTFACE] == SG3D_UNSPECIFIED_PROPERTY)
    u_count++;
  else i_count++;

  ASSERT(u_count + s_count + f_count + i_count == 3);
  if(i_count) {
    ASSERT(properties[SG3D_INTFACE] < darray_descriptions_size_get(descriptions));
    d = descs + properties[SG3D_INTFACE];
  }
  if(f_count > 1) {
    /* Cannot have 2 fluids */
    *properties_conflict_status = 1;
  }
  else if(u_count > 1) {
    /* Cannot have undefined interface if 1 side is undefined */
    *properties_conflict_status = 1;
  }
  else if(u_count == 1) {
    if(!(s_count == 2 && !d)
      && !(s_count == 1 && d
        && (d->type == DESC_BOUND_H_FOR_SOLID || d->type == DESC_BOUND_H_FOR_SOLID)))
    {
      /* The only valid configurations are Solid / Undef / Solid
       * and Solid / Solid_bound / Undef */
      *properties_conflict_status = 1;
    }
  } else {
    ASSERT(u_count == 0 && d);
    if(s_count != 1 || f_count != 1 || d->type != DESC_SOLID_FLUID_CONNECT) {
      /* The only valid configuration is Solid / Solid_Fluid_bound / Fluid */
      *properties_conflict_status = 1;
    }
  }

  /* No conflict found */
  *properties_conflict_status = 0;
  return RES_OK;
}

#undef COUNT_SIDE

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
stardis_init
  (const struct args* args,
   struct stardis* stardis)
{
  res_T res = RES_OK;
  int geom_initialized = 0;
  unsigned count;
  int run = 1;

  ASSERT(args && stardis && args->allocator);

  stardis->allocator = args->allocator;
  darray_descriptions_init(stardis->allocator, &stardis->descriptions);
  stardis->probe[0] = args->probe[0];
  stardis->probe[1] = args->probe[1];
  stardis->probe[2] = args->probe[2];
  stardis->probe[3] = args->probe[3];
  init_camera(&stardis->camera);
  str_init(stardis->allocator, &stardis->solve_name);
  darray_size_t_init(stardis->allocator, &stardis->compute_region.primitives);
  darray_sides_init(stardis->allocator, &stardis->compute_region.sides);
  darray_uint_init(stardis->allocator, &stardis->compute_region.err_triangles);
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
  ERR(init_geometry(stardis->allocator, stardis->verbose, &stardis->geometry));
  geom_initialized = 1;

  if(args->mode & IR_COMPUTE)
    ERR(parse_camera(args->camera, &stardis->camera));
  else if(args->mode & MEDIUM_COMPUTE)
    ERR(str_set(&stardis->solve_name, args->medium_name));
  else if(args->mode & (BOUNDARY_COMPUTE | MAP_COMPUTE))
    ERR(str_set(&stardis->solve_name, args->solve_boundary_filename));

  ERR(read_model(&args->model_files, stardis));

  /* If computation is on a compute region, read it */
  if(args->mode & REGION_COMPUTE_MODES) {
    ASSERT(!str_is_empty(&stardis->solve_name));
    ERR(read_compute_region(stardis));
  }

  /* Check conflicts */
  ERR(sg3d_geometry_get_unique_triangles_with_merge_conflict_count(
    stardis->geometry.sg3d, &count));
  if(count) {
    fprintf(stderr,
      "Merge conflicts found in the the model (%u triangles).\n", count);
    run = 0;
    res = RES_BAD_ARG;
    /* Don't goto error to allow further checking */
  }
  ERR(sg3d_geometry_validate_properties(stardis->geometry.sg3d,
    validate_properties, &stardis->descriptions));
  ERR(sg3d_geometry_get_unique_triangles_with_properties_conflict_count(
    stardis->geometry.sg3d, &count));
  if(count) {
    fprintf(stderr,
      "Properties conflicts found in the the model (%u triangles).\n", count);
    run = 0;
    res = RES_BAD_ARG;
    /* Don't goto error to allow further checking */
  }
  if(args->mode & REGION_COMPUTE_MODES) {
    ERR(sg3d_geometry_validate_properties(stardis->geometry.sg3d,
      validate_properties, &stardis->descriptions));
    ERR(sg3d_geometry_get_unique_triangles_with_properties_conflict_count(
      stardis->geometry.sg3d, &count));
    if(count) {
      fprintf(stderr, "Invalid compute region defined by file '%s'.\n",
        str_cget(&stardis->solve_name));
      fprintf(stderr,
        "The file contains %u triangles not in the model.\n", count);
      run = 0;
      res = RES_BAD_ARG;
    }
  }
  if(!run) {
    fprintf(stderr, "Cannot run the solver.\n");
    goto error;
  }

exit:
  return res;
error:
  if(geom_initialized) release_geometry(&stardis->geometry);
  darray_descriptions_release(&stardis->descriptions);
  str_release(&stardis->solve_name);
  darray_size_t_release(&stardis->compute_region.primitives);
  darray_sides_release(&stardis->compute_region.sides);
  darray_uint_release(&stardis->compute_region.err_triangles);
  goto exit;
}

void
stardis_release(struct stardis* stardis)
{
  struct sdis_interface** itf;
  struct description* dsc;

  if(!stardis) return;

  itf = darray_interface_ptrs_data_get(&stardis->geometry.interfaces);
  dsc = darray_descriptions_data_get(&stardis->descriptions);
  str_release(&stardis->solve_name);
  darray_descriptions_release(&stardis->descriptions);
  release_geometry(&stardis->geometry);
  darray_size_t_release(&stardis->compute_region.primitives);
  darray_sides_release(&stardis->compute_region.sides);
  darray_uint_release(&stardis->compute_region.err_triangles);
}
