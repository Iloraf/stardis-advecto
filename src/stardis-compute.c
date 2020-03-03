/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#include "stardis-app.h"
#include "stardis-output.h"
#include "stardis-compute.h"
#include "stardis-fluid.h"
#include "stardis-solid.h"
#include "stardis-intface.h"

#include <sdis.h>
#include <sdis_version.h>

#include <star/sg3d_sdisXd_helper.h>

#include <rsys/double3.h>
#include <rsys/double2.h>
#include <rsys/image.h>

struct dummies {
  struct sdis_medium* dummy_fluid;
  unsigned dummy_fluid_id;
  struct sdis_medium* dummy_solid;
  unsigned dummy_solid_id;
};
#define DUMMIES_NULL__ {\
  NULL, UINT_MAX, NULL, UINT_MAX\
}
static const struct dummies DUMMIES_NULL = DUMMIES_NULL__;

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

/* Check probe position and move it to the closest point on an interface 
 * if on_interface is set */
static res_T
select_probe_type
  (const struct sdis_scene* scn, 
   const struct stardis* stardis,
   const int on_interface,
   double pos[3], 
   size_t* iprim, 
   double uv[2])
{
  res_T res = RES_OK;
  double min_d = DBL_MAX, min_delta = DBL_MAX, new_pos[3], tmp_uv[2], min_uv[2];
  const struct description *min_desc = NULL;
  size_t found = SIZE_MAX;
  enum description_type min_type = DESCRIPTION_TYPE_COUNT__;
  const struct description* descriptions;
  unsigned i, tcount;

  ASSERT(scn && stardis && pos && iprim && uv);
  descriptions = darray_descriptions_cdata_get(&stardis->descriptions);
  ERR(sg3d_geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tcount));
  for(i = 0; i < tcount; ++i) {
    unsigned indices[3], properties[3];
    double dp[3], d, projected_pos[3], nn[3], tmp[3];
    float v0[3], v1[3], v2[3], e1[3], e2[3], n[3];
    const struct description *desc = NULL;
    enum sdis_side s;

    ERR(sg3d_geometry_get_unique_triangle_vertices(stardis->geometry.sg3d, i,
      indices));

    ERR(sdis_scene_boundary_project_position(scn, i, pos, tmp_uv));
    ERR(sdis_scene_get_boundary_position(scn, i, tmp_uv, projected_pos));
    d3_sub(dp, projected_pos, pos);
    d = d3_len(dp);

    if(d == 0) {
      /* Best possible match */
      found = i;
      fprintf(stderr,
        "The probe is on the primitive %llu.\n", (long long int)*iprim);
      break;
    }
    if(d >= min_d) {
      /* No improvement */
      continue;
    }
    min_d = d;
    d2_set(min_uv, tmp_uv);

    /* Find side */
    ERR(sg3d_geometry_get_unique_vertex(stardis->geometry.sg3d, indices[0], tmp));
    f3_set_d3(v0, tmp);
    ERR(sg3d_geometry_get_unique_vertex(stardis->geometry.sg3d, indices[1], tmp));
    f3_set_d3(v1, tmp);
    ERR(sg3d_geometry_get_unique_vertex(stardis->geometry.sg3d, indices[2], tmp));
    f3_set_d3(v2, tmp);
    f3_sub(e1, v1, v0);
    f3_sub(e2, v2, v0);
    f3_cross(n, e1, e2);

    /* Stardis solver convention is triangle normal is back side */
    d3_set_f3(nn, n);
    s = (d3_dot(nn, dp) < 0) ? SDIS_BACK : SDIS_FRONT;

    ERR(sg3d_geometry_get_unique_triangle_properties(stardis->geometry.sg3d, i,
      properties));

    if(s == SDIS_FRONT && properties[SG3D_FRONT] != SG3D_UNSPECIFIED_PROPERTY)
      desc = descriptions + properties[SG3D_FRONT];
    if(s == SDIS_BACK && properties[SDIS_BACK] != SG3D_UNSPECIFIED_PROPERTY)
      desc = descriptions + properties[SG3D_BACK];

    min_desc = desc;
    if(!desc) continue;

    min_type = desc->type;
    if(desc->type == DESC_MAT_SOLID) {
      found = i;
      min_delta = desc->d.solid.delta;
      d3_set(new_pos, projected_pos);
    }
  }

  if(!min_desc) {
    /* Doesn't handle the case where pos is outside the model
     * but very close and could be considered on the boundary */
    fprintf(stderr, "The probe is outside the model.\n");
    return RES_BAD_ARG;
  }

  if(on_interface) {
    *iprim = found;
    if(min_type == DESC_MAT_FLUID)
      fprintf(stderr, "Probe was on a fluid.\n");
    fprintf(stderr, "The probe is moved to %g,%g,%g (primitive %llu)\n",
      SPLIT3(new_pos), (long long int)*iprim);
    if(min_type == DESC_MAT_SOLID && min_d > min_delta)
      fprintf(stderr, "WARNING: The probe move is %.1f delta long.\n", min_d / min_delta);
    d3_set(pos, new_pos);
    d2_set(uv, min_uv);
  } else {
    if(min_type == DESC_MAT_FLUID)
      fprintf(stderr,
        "The probe is in a fluid: computing fluid temperature, not using a specific position.\n");
    if(min_type == DESC_MAT_SOLID && min_d < min_delta) {
      fprintf(stderr,
        "WARNING: The probe is very close to the primitive %llu (%.1f delta).\n",
        (long long int)found, min_d / min_delta);
      fprintf(stderr,
        "To have this probe moved on the closest interface use -P instead of -p\n");
    }
  }

end:
  return res;
error:
  goto end;
}

static res_T
create_holder
  (struct mem_allocator* allocator, 
   struct counts* counts,
   struct dummies* dummies,
   struct description* description,
   struct sdis_device* dev,
   struct darray_media_ptr* media_ptr,
   const int is_green)
{
  res_T res = RES_OK;

  ASSERT(allocator && counts && dummies && description && dev && media_ptr);

  switch (description->type) {
  case DESC_BOUND_H_FOR_SOLID:
    counts->hbound_count++;
    /* Create an external fluid with bound temp as fluid temp */
    ERR(create_fluid(dev, allocator, NULL, 1, 1, is_green, 1, -1,
      description->d.h_boundary.imposed_temperature, media_ptr,
      &description->d.h_boundary.mat_id));
    break;
  case DESC_BOUND_H_FOR_FLUID:
    /* Create an external solid with bound temp as solid temp */
    counts->hbound_count++;
    ERR(create_solid(dev, allocator, NULL, INF, 1, 1, 1, is_green, 1, -1,
      description->d.h_boundary.imposed_temperature, 0, media_ptr,
      &description->d.h_boundary.mat_id));
    break;
  case DESC_BOUND_T_FOR_SOLID:
    counts->tbound_count++;
    if(dummies->dummy_fluid) {
      /* Reuse external dummy fluid */
      description->d.t_boundary.mat_id = dummies->dummy_fluid_id;
    } else {
      /* Create dummy fluid */
      size_t sz = darray_media_ptr_size_get(media_ptr);
      ERR(create_fluid(dev, allocator, NULL, 1, 1, is_green, 1, -1,
        -1, media_ptr, &description->d.t_boundary.mat_id));
      dummies->dummy_fluid = darray_media_ptr_data_get(media_ptr)[sz];
      dummies->dummy_fluid_id = description->d.t_boundary.mat_id;
    }
    break;
  case DESC_BOUND_T_FOR_FLUID:
    counts->tbound_count++;
    if(dummies->dummy_solid) {
      /* Reuse external dummy solid */
      description->d.t_boundary.mat_id = dummies->dummy_solid_id;
    } else {
      /* Create dummy solid */
      size_t sz = darray_media_ptr_size_get(media_ptr);
      ERR(create_solid(dev, allocator, NULL, 1, 1, 1, 1, is_green, 1, -1,
        -1, 0, media_ptr, &description->d.t_boundary.mat_id));
      dummies->dummy_solid = darray_media_ptr_data_get(media_ptr)[sz];
      dummies->dummy_solid_id = description->d.t_boundary.mat_id;
    }
    break;
  case DESC_BOUND_F_FOR_SOLID:
    counts->fbound_count++;
    if(dummies->dummy_fluid) {
      /* Reuse external dummy fluid */
      description->d.f_boundary.mat_id = dummies->dummy_fluid_id;
    } else {
      /* Create dummy fluid */
      size_t sz = darray_media_ptr_size_get(media_ptr);
      ERR(create_fluid(dev, allocator, NULL, 1, 1, is_green, 1, -1, -1,
        media_ptr, &description->d.f_boundary.mat_id));
      dummies->dummy_fluid = darray_media_ptr_data_get(media_ptr)[sz];
      dummies->dummy_fluid_id = description->d.f_boundary.mat_id;
    }
    break;
  case DESC_SOLID_FLUID_CONNECT:
    counts->sfconnect_count++;
    ASSERT(description->d.sf_connect.specular_fraction >= 0
      && description->d.sf_connect.specular_fraction <= 1);
    ASSERT(description->d.sf_connect.emissivity >= 0);
    ASSERT(description->d.sf_connect.hc >= 0);
    break;
  case DESC_MAT_SOLID:
    counts->smed_count++;
    ERR(create_solid(dev,
      allocator,
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
      media_ptr,
      &description->d.solid.solid_id));
    break;
  case DESC_MAT_FLUID:
    counts->fmed_count++;
    ERR(create_fluid(dev,
      allocator,
      &description->d.fluid.name,
      description->d.fluid.rho,
      description->d.fluid.cp,
      is_green,
      0,
      description->d.fluid.tinit,
      description->d.fluid.imposed_temperature,
      media_ptr,
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

static res_T
compute_probe
  (struct sdis_scene* scn,
   const struct counts* counts,
   struct stardis* stardis,
   enum stardis_mode mode)
{
  res_T res = RES_OK;
  double time[2], pos[3], uv[2] = { 0,0 };
  size_t iprim = SIZE_MAX;
  struct sdis_green_function* green = NULL;
  struct sdis_estimator* estimator = NULL;
  
  ASSERT(scn && counts && stardis && (mode & PROBE_COMPUTE));

  d3_set(pos, stardis->probe);
  ERR(select_probe_type(scn, stardis, 0, pos, &iprim, uv));

  if(mode & GREEN_MODE) {
    ASSERT(iprim == SIZE_MAX);
    ERR(sdis_solve_probe_green_function(scn,
      stardis->samples,
      pos,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      &green));
    ERR(dump_green(green, counts, &stardis->descriptions, stardis->radiative_temp));
  } else {
    ASSERT(iprim == SIZE_MAX);
    d2_splat(time, stardis->probe[3]);
    ERR(sdis_solve_probe(scn,
      stardis->samples,
      pos,
      time,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      stardis->dump_paths,
      &estimator));
    ERR(print_single_MC_result(mode, estimator, stardis));
  }
end:
  if(estimator) SDIS(estimator_ref_put(estimator));
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

static res_T
compute_probe_on_interface
  (struct sdis_scene* scn,
   const struct counts* counts,
   struct stardis* stardis,
   const int mode)
{
  res_T res = RES_OK;
  double time[2], pos[3], uv[2] = { 0,0 };
  size_t iprim = SIZE_MAX;
  struct sdis_estimator* estimator = NULL;
  struct sdis_green_function* green = NULL;

  ASSERT(scn && counts && stardis && (mode & PROBE_COMPUTE_ON_INTERFACE));
  d3_set(pos, stardis->probe);
  ERR(select_probe_type(scn, stardis, 1, pos, &iprim, uv));

  if(mode & GREEN_MODE) {
    ASSERT(iprim == SIZE_MAX);
    ERR(sdis_solve_probe_boundary_green_function(scn,
      stardis->samples,
      iprim,
      uv,
      SDIS_FRONT,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      &green));
    ERR(dump_green(green, counts, &stardis->descriptions, stardis->radiative_temp));
  } else {
    ASSERT(iprim == SIZE_MAX);
    d2_splat(time, stardis->probe[3]);
    ERR(sdis_solve_probe_boundary(scn,
      stardis->samples,
      iprim,
      uv,
      time,
      SDIS_FRONT, /*                                       FIXME!!!!!!! */
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      stardis->dump_paths,
      &estimator));
    ERR(print_single_MC_result(mode, estimator, stardis));
  }
end:
  if(estimator) SDIS(estimator_ref_put(estimator));
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

static res_T
compute_camera
  (struct sdis_device* dev,
   struct sdis_scene* scn,
   const struct stardis* stardis,
   const int mode)
{
  res_T res = RES_OK;
  double time[2];
  size_t width, height;
  struct sdis_estimator_buffer* buf = NULL;
  struct sdis_camera* cam = NULL;

  ASSERT(!(mode & GREEN_MODE) && (mode & IR_COMPUTE) && scn && stardis);
  (void)mode;

  width = (size_t)stardis->camera.img[0];
  height = (size_t)stardis->camera.img[1];
  /* Setup the camera */
  ERR(sdis_camera_create(dev, &cam));
  ERR(sdis_camera_set_proj_ratio(cam, (double)width / (double)height));
  ERR(sdis_camera_set_fov(cam, MDEG2RAD(stardis->camera.fov)));
  ERR(sdis_camera_look_at(cam,
    stardis->camera.pos,
    stardis->camera.tgt,
    stardis->camera.up));

  /* Launch the simulation */
  time[0] = time[1] = stardis->camera.time;
  ERR(sdis_solve_camera(scn,
    cam,
    time,
    stardis->scale_factor,
    stardis->radiative_temp[0],
    stardis->radiative_temp[1],
    width,
    height,
    (size_t)stardis->camera.spp,
    stardis->dump_paths,
    &buf));

  /* Write the image */
  ERR(dump_image(buf));

end:
  if(cam) SDIS(camera_ref_put(cam));
  if(buf) SDIS(estimator_buffer_ref_put(buf));
  return res;
error:
  goto end;
}

static res_T
compute_medium
  (struct sdis_scene* scn,
   struct darray_media_ptr* media_ptr,
   struct stardis* stardis,
   const int mode)
{
  res_T res = RES_OK;
  double time[2];
  size_t sz, ii;
  struct sdis_medium* medium = NULL;
  struct sdis_estimator* estimator = NULL;
  struct sdis_green_function* green = NULL;

  ASSERT(scn && media_ptr && stardis && (mode & MEDIUM_COMPUTE));

  /* Find medium */
  sz = darray_descriptions_size_get(&stardis->descriptions);
  FOR_EACH(ii, 0, sz) {
    struct description* 
      desc = darray_descriptions_data_get(&stardis->descriptions) + ii;
    if(desc->type == DESC_MAT_SOLID) {
      if(str_eq(&stardis->solve_name, &desc->d.solid.name)) {
        ASSERT(darray_media_ptr_size_get(media_ptr) > ii);
        medium = darray_media_ptr_data_get(media_ptr)[ii];
        break;
      }
    }
    else if(desc->type == DESC_MAT_FLUID) {
      if(str_eq(&stardis->solve_name, &desc->d.fluid.name) ){
        ASSERT(darray_media_ptr_size_get(media_ptr) > ii);
        medium = darray_media_ptr_data_get(media_ptr)[ii];
        break;
      }
    }
  }
  if(medium == NULL) { /* Not found */
    fprintf(stderr, "Cannot solve medium %s (unknown medium)\n",
      str_cget(&stardis->solve_name));
    res = RES_BAD_ARG;
    goto error;
  }

  if(mode & GREEN_MODE) {
    ERR(sdis_solve_medium_green_function(scn,
      stardis->samples,
      medium,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      &green));
  } else {
    d2_splat(time, stardis->probe[3]);
    time[0] = time[1] = stardis->probe[3];
    ERR(sdis_solve_medium(scn,
      stardis->samples,
      medium,
      time,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      stardis->dump_paths,
      &estimator));
    ERR(print_single_MC_result(mode, estimator, stardis));
  }
end:
  if(estimator) SDIS(estimator_ref_put(estimator));
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

static res_T
add_compute_region_triangle
  (const unsigned unique_id, const unsigned itri, void* context)
{
  res_T res = RES_OK;
  const struct add_geom_ctx* ctx = context;
  struct compute_region* region;
  ASSERT(ctx); (void)itri;
  region = ctx->custom;
  /* The triangle should be already in the model, but is not
   * Keep it in err_triangles to allow dumps */
  ERR(darray_uint_push_back(&region->err_triangles, &unique_id));

end:
  return res;
error:
  goto end;
}

static res_T
merge_compute_region_triangle
  (const unsigned unique_id,
   const unsigned itri,
   const int reversed_triangle,
   unsigned triangle_properties[SG3D_PROP_TYPES_COUNT__],
   const unsigned merged_properties[SG3D_PROP_TYPES_COUNT__],
   void* context,
   int* merge_conflict_status)
{
  res_T res = RES_OK;
  const struct add_geom_ctx* ctx = context;
  struct compute_region* region;
  size_t uid;
  enum sdis_side side = reversed_triangle ? SDIS_BACK : SDIS_FRONT;
  ASSERT(ctx);
  (void)itri; (void)reversed_triangle; (void)triangle_properties;
  (void)merged_properties; (void)merge_conflict_status;
  region = ctx->custom;
  /* Build the compute region */
  uid = unique_id;
  ERR(darray_size_t_push_back(&region->primitives, &uid));
  ERR(darray_sides_push_back(&region->sides, &side));
end:
  return res;
error:
  goto end;
}

/* Process an STL file describing a compute region *
 * Triangles must be members of the geometry already
 * TODO: what if same triangle appears more than once (same side or not)?
 */
res_T
read_compute_region(struct stardis* stardis)
{
  res_T res = RES_OK;
  struct sstl* sstl = NULL;
  struct add_geom_ctx add_geom_ctx;
  struct sg3d_geometry_add_callbacks callbacks = SG3D_ADD_CALLBACKS_NULL__;
  int new_triangles_initialized = 0;
  const char* file;
  
  ASSERT(stardis);

  file = str_cget(&stardis->solve_name);
  callbacks.get_indices = add_geom_ctx_indices;
  callbacks.get_position = add_geom_ctx_position;
  callbacks.add_triangle = add_compute_region_triangle;
  callbacks.merge_triangle = merge_compute_region_triangle;

  ERR(sstl_create(NULL, stardis->allocator, 0, &sstl));
  res = sstl_load(sstl, file);
  if(res != RES_OK) {
    fprintf(stderr, "Cannot read STL file: %s\n", file);
    goto error;
  }
  ERR(sstl_get_desc(sstl, &add_geom_ctx.stl_desc));
  ASSERT(add_geom_ctx.stl_desc.vertices_count <= UINT_MAX
    && add_geom_ctx.stl_desc.triangles_count <= UINT_MAX);

  new_triangles_initialized = 1;
  add_geom_ctx.custom = &stardis->compute_region;

  res = sg3d_geometry_add(
    stardis->geometry.sg3d,
    (unsigned)add_geom_ctx.stl_desc.vertices_count,
    (unsigned)add_geom_ctx.stl_desc.triangles_count,
    &callbacks,
    &add_geom_ctx);
  if(res != RES_OK) {
    fprintf(stderr, "Cannot add file content: %s\n", file);
    goto error;
  }

end:
  if(sstl) SSTL(ref_put(sstl));
  return res;
error:
  goto end;
}

static res_T
compute_boundary
  (struct sdis_scene* scn,
   const struct counts* counts,
   struct stardis* stardis,
   const int mode)
{
  res_T res = RES_OK;
  double time[2], pos[3];
  struct sdis_green_function* green = NULL;
  struct sdis_estimator* estimator = NULL;  

  ASSERT(scn && counts && stardis && (mode & BOUNDARY_COMPUTE));

  d3_set(pos, stardis->probe);

  if(mode & GREEN_MODE) {
    ERR(sdis_solve_boundary_green_function(scn,
      stardis->samples,
      darray_size_t_cdata_get(&stardis->compute_region.primitives),
      darray_sides_cdata_get(&stardis->compute_region.sides),
      darray_size_t_size_get(&stardis->compute_region.primitives),
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      &green));
    ERR(dump_green(green, counts, &stardis->descriptions, stardis->radiative_temp));
  } else {
    d2_splat(time, stardis->probe[3]);
    ERR(sdis_solve_boundary(scn,
      stardis->samples,
      darray_size_t_cdata_get(&stardis->compute_region.primitives),
      darray_sides_cdata_get(&stardis->compute_region.sides),
      darray_size_t_size_get(&stardis->compute_region.primitives),
      time,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      stardis->dump_paths,
      &estimator));
    ERR(print_single_MC_result(mode, estimator, stardis));
  }
end:
  if(estimator) SDIS(estimator_ref_put(estimator));
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

static res_T
compute_flux_boundary
  (struct sdis_scene* scn,
   struct stardis* stardis,
   const int mode)
{
  res_T res = RES_OK;
  double time[2], pos[3];
  struct sdis_green_function* green = NULL;
  struct sdis_estimator* estimator = NULL;

  ASSERT(scn && stardis && (mode & FLUX_BOUNDARY_COMPUTE));

  d3_set(pos, stardis->probe);
  d2_splat(time, stardis->probe[3]);
  ERR(sdis_solve_boundary_flux(scn,
    stardis->samples,
    darray_size_t_cdata_get(&stardis->compute_region.primitives),
    darray_size_t_size_get(&stardis->compute_region.primitives),
    time,
    stardis->scale_factor,
    stardis->radiative_temp[0],
    stardis->radiative_temp[1],
    &estimator));
  ERR(print_single_MC_result(mode, estimator, stardis));
end:
  if(estimator) SDIS(estimator_ref_put(estimator));
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

static res_T
compute_map
  (struct sdis_scene* scn,
   struct stardis* stardis,
   const int mode)
{
  res_T res = RES_OK;
  double time[2], pos[3];
  struct sdis_green_function* green = NULL;
  struct darray_estimators estimators;
  int estimators_initialized = 0;
  size_t p;

  ASSERT(scn && stardis && (mode & MAP_COMPUTE)
    && !(mode & GREEN_MODE));
  (void)mode;

  darray_estimators_init(stardis->allocator, &estimators);
  estimators_initialized = 1;
  d3_set(pos, stardis->probe);
  d2_splat(time, stardis->probe[3]);

  ERR(darray_estimators_resize(&estimators,
    darray_estimators_size_get(&estimators) +
    darray_size_t_size_get(&stardis->compute_region.primitives)));

  FOR_EACH(p, 0, darray_size_t_size_get(&stardis->compute_region.primitives)) {
    ERR(sdis_solve_boundary(scn,
      stardis->samples,
      darray_size_t_cdata_get(&stardis->compute_region.primitives) + p,
      darray_sides_cdata_get(&stardis->compute_region.sides) + p,
      1,
      time,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      stardis->dump_paths,
      darray_estimators_data_get(&estimators) + p));
  }
  dump_map(stardis, &estimators);

end:
  if(estimators_initialized) {
    struct sdis_estimator** est = darray_estimators_data_get(&estimators);
    FOR_EACH(p, 0, darray_size_t_size_get(&stardis->compute_region.primitives)) {
      SDIS(estimator_ref_put(est[p]));
    }
    darray_estimators_release(&estimators);
  }
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
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

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
stardis_compute
  (struct stardis* stardis,
   const int mode)
{
  res_T res = RES_OK;
  struct sdis_device* dev = NULL;
  struct darray_media_ptr media;
  struct sg3d_sdisXd_scene_create_context create_context;
  struct dummies dummies = DUMMIES_NULL;
  struct htable_intface htable_interfaces;
  int struct_initialised = 0;
  struct counts counts = COUNTS_NULL;
  int try_compute = 0, try_scene = 0;
  unsigned i, vcount, tcount;

  ASSERT(stardis && (mode & COMPUTE_MODES));

  ERR(sdis_device_create(NULL, stardis->allocator, stardis->nthreads,
    stardis->verbose, &dev));

  htable_intface_init(stardis->allocator, &htable_interfaces);
  darray_media_ptr_init(stardis->allocator, &media);
  struct_initialised = 1;

  /* Create media and property holders found in descriptions */
  for(i = 0; i < darray_descriptions_size_get(&stardis->descriptions); ++i) {
    ERR(create_holder(stardis->allocator, &counts, &dummies,
      darray_descriptions_data_get(&stardis->descriptions) + i, dev,
      &media, mode & GREEN_MODE));
  }

  /* Create interfaces */
  ERR(sg3d_geometry_get_unique_vertices_count(stardis->geometry.sg3d, &vcount));
  ERR(sg3d_geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tcount));

  for(i = 0; i < tcount; ++i) {
    ERR(create_intface(dev, i, &htable_interfaces, &stardis->geometry,
      &stardis->descriptions, &media));
  }

  /* Create scene */
  try_scene = 1;
  create_context.geometry = stardis->geometry.sg3d;
  create_context.app_interface_getter = geometry_get_interface;
  create_context.app_interface_data = &stardis->geometry.interf_bytrg;
  ERR(sdis_scene_create(dev, tcount, sg3d_sdisXd_geometry_get_indices,
    sg3d_sdisXd_geometry_get_interface, vcount, sg3d_sdisXd_geometry_get_position,
    &create_context, &stardis->geometry.sdis_scn));

  /* Compute */
  try_compute = 1;
  if(mode & PROBE_COMPUTE)
    ERR(compute_probe(stardis->geometry.sdis_scn, &counts, stardis, mode));
  else if(mode & PROBE_COMPUTE_ON_INTERFACE)
    ERR(compute_probe_on_interface(stardis->geometry.sdis_scn,
      &counts, stardis, mode));
  else if(mode & IR_COMPUTE)
    ERR(compute_camera(dev, stardis->geometry.sdis_scn, stardis, mode));
  else if(mode & MEDIUM_COMPUTE)
    ERR(compute_medium(stardis->geometry.sdis_scn, &media, stardis, mode));
  else if(mode & BOUNDARY_COMPUTE)
    ERR(compute_boundary(stardis->geometry.sdis_scn, &counts, stardis, mode));
  else if(mode & FLUX_BOUNDARY_COMPUTE)
    ERR(compute_flux_boundary(stardis->geometry.sdis_scn, stardis, mode));
  else if(mode & MAP_COMPUTE)
    ERR(compute_map(stardis->geometry.sdis_scn, stardis, mode));
  else FATAL("Unknown mode.\n");

end:
  if(struct_initialised) {
    htable_intface_release(&htable_interfaces);
    FOR_EACH(i, 0, darray_media_ptr_size_get(&media))
      SDIS(medium_ref_put(darray_media_ptr_data_get(&media)[i]));
    darray_media_ptr_release(&media);
  }
  if(dev) SDIS(device_ref_put(dev));

  return res;
error:
  if(try_compute) fprintf(stderr, "%s: computation failed!\n", FUNC_NAME);
  else if(try_scene) fprintf(stderr, "%s: could not setup the scene!\n", FUNC_NAME);
  goto end;
}
