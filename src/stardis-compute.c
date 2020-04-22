/* Copyright (C) 2018-2020 |Meso|Star> (contact@meso-star.com)
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
#include "stardis-fluid.h"
#include "stardis-solid.h"

#include <sdis.h>
#include <sdis_version.h>

#include <star/sg3d_sdisXd_helper.h>

#include <rsys/double3.h>
#include <rsys/double2.h>
#include <rsys/logger.h>
#include <rsys/image.h>

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

/* Check probe position and move it to the closest point on an interface 
 * if on_interface is set */
static res_T
select_probe_type
  (const struct stardis* stardis,
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

  ASSERT(stardis && pos && iprim && uv);
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

    ERR(sdis_scene_boundary_project_position(stardis->sdis_scn, i, pos, tmp_uv));
    ERR(sdis_scene_get_boundary_position(stardis->sdis_scn, i, tmp_uv, projected_pos));
    d3_sub(dp, projected_pos, pos);
    d = d3_len(dp);

    if(d == 0) {
      /* Best possible match */
      found = i;
      logger_print(stardis->logger, LOG_OUTPUT,
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
    logger_print(stardis->logger, LOG_ERROR, "The probe is outside the model.\n");
    return RES_BAD_ARG;
  }

  if(on_interface) {
    *iprim = found;
    if(min_type == DESC_MAT_FLUID)
      logger_print(stardis->logger, LOG_OUTPUT, "Probe was on a fluid.\n");
    logger_print(stardis->logger, LOG_OUTPUT,
      "The probe is moved to %g,%g,%g (primitive %llu)\n",
      SPLIT3(new_pos), (long long int)*iprim);
    if(min_type == DESC_MAT_SOLID && min_d > min_delta)
      logger_print(stardis->logger, LOG_WARNING,
        "The probe move is %.1f delta long.\n", min_d / min_delta);
    d3_set(pos, new_pos);
    d2_set(uv, min_uv);
  } else {
    if(min_type == DESC_MAT_FLUID)
      logger_print(stardis->logger, LOG_WARNING,
        "The probe is in a fluid: computing fluid temperature, not using a specific position.\n");
    if(min_type == DESC_MAT_SOLID && min_d < min_delta) {
      logger_print(stardis->logger, LOG_WARNING,
        "The probe is very close to the primitive %llu (%.1f delta).\n",
        (long long int)found, min_d / min_delta);
      logger_print(stardis->logger, LOG_WARNING,
        "To have this probe moved on the closest interface use -P instead of -p\n");
    }
  }

end:
  return res;
error:
  goto end;
}

static res_T
compute_probe(struct stardis* stardis)
{
  res_T res = RES_OK;
  double time[2], pos[3], uv[2] = { 0,0 };
  size_t iprim = SIZE_MAX;
  struct sdis_green_function* green = NULL;
  struct sdis_estimator* estimator = NULL;
  struct dump_path_context dump_ctx;
  
  ASSERT(stardis && (stardis->mode & PROBE_COMPUTE));

  d3_set(pos, stardis->probe);
  ERR(select_probe_type(stardis, 0, pos, &iprim, uv));

  if(stardis->mode & GREEN_MODE) {
    ASSERT(iprim == SIZE_MAX);
    ERR(sdis_solve_probe_green_function(stardis->sdis_scn,
      stardis->samples,
      pos,
      stardis->scale_factor,
      stardis->ambient_temp,
      stardis->ref_temp,
      &green));
    ERR(dump_green(green, stardis, stdout));
  } else {
    ASSERT(iprim == SIZE_MAX);
    d2_splat(time, stardis->probe[3]);
    ERR(sdis_solve_probe(stardis->sdis_scn,
      stardis->samples,
      pos,
      time,
      stardis->scale_factor,
      stardis->ambient_temp,
      stardis->ref_temp,
      stardis->dump_paths,
      &estimator));
    ERR(print_single_MC_result(estimator, stardis, stdout));
  }

  /* Dump paths recorded according to user settings */
  dump_ctx.stardis = stardis;
  dump_ctx.rank = 0;
  ERR(sdis_estimator_for_each_path(estimator, dump_path, &dump_ctx));

end:
  if(estimator) SDIS(estimator_ref_put(estimator));
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

static res_T
compute_probe_on_interface(struct stardis* stardis)
{
  res_T res = RES_OK;
  double time[2], pos[3], uv[2] = { 0,0 };
  size_t iprim = SIZE_MAX;
  struct sdis_estimator* estimator = NULL;
  struct sdis_green_function* green = NULL;
  struct dump_path_context dump_ctx;

  ASSERT(stardis && (stardis->mode & PROBE_COMPUTE_ON_INTERFACE));
  d3_set(pos, stardis->probe);
  ERR(select_probe_type(stardis, 1, pos, &iprim, uv));

  if(stardis->mode & GREEN_MODE) {
    ASSERT(iprim == SIZE_MAX);
    ERR(sdis_solve_probe_boundary_green_function(stardis->sdis_scn,
      stardis->samples,
      iprim,
      uv,
      SDIS_FRONT,
      stardis->scale_factor,
      stardis->ambient_temp,
      stardis->ref_temp,
      &green));
    ERR(dump_green(green, stardis, stdout));
  } else {
    ASSERT(iprim == SIZE_MAX);
    d2_splat(time, stardis->probe[3]);
    ERR(sdis_solve_probe_boundary(stardis->sdis_scn,
      stardis->samples,
      iprim,
      uv,
      time,
      SDIS_FRONT,
      stardis->scale_factor,
      stardis->ambient_temp,
      stardis->ref_temp,
      stardis->dump_paths,
      &estimator));
    ERR(print_single_MC_result(estimator, stardis, stdout));
  }

  /* Dump paths recorded according to user settings */
  dump_ctx.stardis = stardis;
  dump_ctx.rank = 0;
  ERR(sdis_estimator_for_each_path(estimator, dump_path, &dump_ctx));

end:
  if(estimator) SDIS(estimator_ref_put(estimator));
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

static res_T
auto_look_at
  (struct sdis_scene* scn,
   const double fov_x, /* Horizontal field of view in radian */
   const double proj_ratio, /* Width / height */
   const double up[3], /* Up vector */
   double position[3],
   double target[3])
{
  double lower[3], upper[3];
  double up_abs[3];
  double axis_min[3];
  double axis_x[3];
  double axis_z[3];
  double tmp[3];
  double radius;
  double depth;
  res_T res;
  ASSERT(scn && fov_x && proj_ratio && up);

  ERR(sdis_scene_get_aabb(scn, lower, upper));

  if(lower[0] > upper[0] || lower[1] > upper[1] || lower[2] > upper[2]) {
    /* Empty scene */
    d3(position, STARDIS_DEFAULT_RENDERING_POS);
    d3(target, STARDIS_DEFAULT_RENDERING_TGT);
    goto exit;
  }

  /* The target is the scene centroid */
  d3_muld(target, d3_add(target, lower, upper), 0.5);

  /* Define which up dimension is minimal and use its unit vector to compute a
   * vector orthogonal to `up'. This ensures that the unit vector and `up' are
   * not collinear so that their cross product is not a zero vector. */
  up_abs[0] = fabs(up[0]);
  up_abs[1] = fabs(up[1]);
  up_abs[2] = fabs(up[2]);
  if(up_abs[0] < up_abs[1]) {
    if(up_abs[0] < up_abs[2]) d3(axis_min, 1, 0, 0);
    else d3(axis_min, 0, 0, 1);
  } else {
    if(up_abs[1] < up_abs[2]) d3(axis_min, 0, 1, 0);
    else d3(axis_min, 0, 0, 1);
  }
  d3_normalize(axis_x, d3_cross(axis_x, up, axis_min));
  d3_normalize(axis_z, d3_cross(axis_z, up, axis_x));

  /* Find whether the XYZ or the ZYX basis maximise the model's visibility */
  if(fabs(d3_dot(axis_x, upper)) < fabs(d3_dot(axis_z, upper))) {
    SWAP(double, axis_x[0], axis_z[0]);
    SWAP(double, axis_x[1], axis_z[1]);
    SWAP(double, axis_x[2], axis_z[2]);
  }

  /* Ensure that the whole model is visible */
  radius = d3_len(d3_sub(tmp, upper, lower)) * 0.5;
  if(proj_ratio < 1) {
    depth = radius / sin(fov_x / 2.0);
  } else {
    depth = radius / sin(fov_x / (2.0 * proj_ratio));
  }

  /* Define the camera position */
  d3_sub(position, target, d3_muld(tmp, axis_z, depth));

  /* Empirically move the position to find a better point of view */
  d3_add(position, position, d3_muld(tmp, up, radius)); /*Empirical offset*/
  d3_add(position, position, d3_muld(tmp, axis_x, radius)); /*Empirical offset*/
  d3_normalize(tmp, d3_sub(tmp, target, position));
  d3_sub(position, target, d3_muld(tmp, tmp, depth));

exit:
  return res;
error:
  goto exit;
}

static res_T
compute_camera(struct stardis* stardis)
{
  res_T res = RES_OK;
  double proj_ratio, time[2];
  size_t width, height;
  struct sdis_estimator_buffer* buf = NULL;
  struct sdis_camera* cam = NULL;
  struct dump_path_context dump_ctx;
  size_t definition[2];
  size_t ix, iy;

  ASSERT(stardis
    && !(stardis->mode & GREEN_MODE) && (stardis->mode & IR_COMPUTE));

  width = (size_t)stardis->camera.img_width;
  height = (size_t)stardis->camera.img_height;
  proj_ratio = (double)width / (double)height;
  /* Setup the camera */
  ERR(sdis_camera_create(stardis->dev, &cam));
  ERR(sdis_camera_set_proj_ratio(cam, proj_ratio));
  ERR(sdis_camera_set_fov(cam, MDEG2RAD(stardis->camera.fov)));
  if(stardis->camera.auto_look_at) {
    ERR(auto_look_at(stardis->sdis_scn, MDEG2RAD(stardis->camera.fov), proj_ratio,
      stardis->camera.up, stardis->camera.pos, stardis->camera.tgt));
    logger_print(stardis->logger, LOG_OUTPUT,
      "Camera auto-look at: position=%g,%g,%g target=%g,%g,%g.\n",
      SPLIT3(stardis->camera.pos), SPLIT3(stardis->camera.tgt));
  }
  ERR(sdis_camera_look_at(cam,
    stardis->camera.pos,
    stardis->camera.tgt,
    stardis->camera.up));

  /* Launch the simulation */
  time[0] = time[1] = stardis->camera.time;
  ERR(sdis_solve_camera(stardis->sdis_scn,
    cam,
    time,
    stardis->scale_factor,
    stardis->ambient_temp,
    stardis->ref_temp,
    width,
    height,
    (size_t)stardis->camera.spp,
    stardis->dump_paths,
    &buf));

  /* Write the image */
  ERR(dump_image(buf, stdout));

  /* Dump paths recorded according to user settings */
  dump_ctx.stardis = stardis;
  dump_ctx.rank = 0;
  ERR(sdis_estimator_buffer_get_definition(buf, definition));
  FOR_EACH(iy, 0, definition[1]) {
    FOR_EACH(ix, 0, definition[0]) {
      const struct sdis_estimator* estimator;
      ERR(sdis_estimator_buffer_at(buf, ix, iy, &estimator));
      ERR(sdis_estimator_for_each_path(estimator, dump_path, &dump_ctx));
    }
  }

end:
  if(cam) SDIS(camera_ref_put(cam));
  if(buf) SDIS(estimator_buffer_ref_put(buf));
  return res;
error:
  goto end;
}

static res_T
compute_medium(struct stardis* stardis)
{
  res_T res = RES_OK;
  double time[2];
  struct sdis_medium* medium = NULL;
  struct sdis_estimator* estimator = NULL;
  struct sdis_green_function* green = NULL;
  struct dump_path_context dump_ctx;

  ASSERT(stardis && (stardis->mode & MEDIUM_COMPUTE));

  /* Find medium */
  medium = find_medium_by_name(stardis, &stardis->solve_name, NULL);
  if(medium == NULL) { /* Not found */
    logger_print(stardis->logger, LOG_ERROR,
      "Cannot solve medium '%s' (unknown medium)\n",
      str_cget(&stardis->solve_name));
    res = RES_BAD_ARG;
    goto error;
  }

  if(stardis->mode & GREEN_MODE) {
    ERR(sdis_solve_medium_green_function(stardis->sdis_scn,
      stardis->samples,
      medium,
      stardis->scale_factor,
      stardis->ambient_temp,
      stardis->ref_temp,
      &green));
    ERR(dump_green(green, stardis, stdout));
  } else {
    d2_splat(time, stardis->probe[3]);
    time[0] = time[1] = stardis->probe[3];
    ERR(sdis_solve_medium(stardis->sdis_scn,
      stardis->samples,
      medium,
      time,
      stardis->scale_factor,
      stardis->ambient_temp,
      stardis->ref_temp,
      stardis->dump_paths,
      &estimator));
    ERR(print_single_MC_result(estimator, stardis, stdout));
  }

  /* Dump paths recorded according to user settings */
  dump_ctx.stardis = stardis;
  dump_ctx.rank = 0;
  ERR(sdis_estimator_for_each_path(estimator, dump_path, &dump_ctx));

end:
  if(estimator) SDIS(estimator_ref_put(estimator));
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

static res_T
add_compute_surface_triangle
  (const unsigned unique_id, const unsigned itri, void* context)
{
  res_T res = RES_OK;
  const struct add_geom_ctx* ctx = context;
  struct compute_surface* region;
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
merge_compute_surface_triangle
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
  struct compute_surface* region;
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

static res_T
compute_boundary(struct stardis* stardis)
{
  res_T res = RES_OK;
  double time[2], pos[3];
  struct sdis_green_function* green = NULL;
  struct sdis_estimator* estimator = NULL;
  struct dump_path_context dump_ctx;

  ASSERT(stardis && (stardis->mode & BOUNDARY_COMPUTE));

  d3_set(pos, stardis->probe);

  if(stardis->mode & GREEN_MODE) {
    ERR(sdis_solve_boundary_green_function(stardis->sdis_scn,
      stardis->samples,
      darray_size_t_cdata_get(&stardis->compute_surface.primitives),
      darray_sides_cdata_get(&stardis->compute_surface.sides),
      darray_size_t_size_get(&stardis->compute_surface.primitives),
      stardis->scale_factor,
      stardis->ambient_temp,
      stardis->ref_temp,
      &green));
    ERR(dump_green(green, stardis, stdout));
  } else {
    d2_splat(time, stardis->probe[3]);
    ERR(sdis_solve_boundary(stardis->sdis_scn,
      stardis->samples,
      darray_size_t_cdata_get(&stardis->compute_surface.primitives),
      darray_sides_cdata_get(&stardis->compute_surface.sides),
      darray_size_t_size_get(&stardis->compute_surface.primitives),
      time,
      stardis->scale_factor,
      stardis->ambient_temp,
      stardis->ref_temp,
      stardis->dump_paths,
      &estimator));
    ERR(print_single_MC_result(estimator, stardis, stdout));
  }

  /* Dump paths recorded according to user settings */
  dump_ctx.stardis = stardis;
  dump_ctx.rank = 0;
  ERR(sdis_estimator_for_each_path(estimator, dump_path, &dump_ctx));

end:
  if(estimator) SDIS(estimator_ref_put(estimator));
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

static res_T
compute_flux_boundary(struct stardis* stardis)
{
  res_T res = RES_OK;
  double time[2], pos[3];
  struct sdis_green_function* green = NULL;
  struct sdis_estimator* estimator = NULL;
  struct dump_path_context dump_ctx;

  ASSERT(stardis && (stardis->mode & FLUX_BOUNDARY_COMPUTE));

  d3_set(pos, stardis->probe);
  d2_splat(time, stardis->probe[3]);
  ERR(sdis_solve_boundary_flux(stardis->sdis_scn,
    stardis->samples,
    darray_size_t_cdata_get(&stardis->compute_surface.primitives),
    darray_size_t_size_get(&stardis->compute_surface.primitives),
    time,
    stardis->scale_factor,
    stardis->ambient_temp,
    stardis->ref_temp,
    &estimator));
  ERR(print_single_MC_result(estimator, stardis, stdout));

  /* Dump paths recorded according to user settings */
  dump_ctx.stardis = stardis;
  dump_ctx.rank = 0;
  ERR(sdis_estimator_for_each_path(estimator, dump_path, &dump_ctx));

end:
  if(estimator) SDIS(estimator_ref_put(estimator));
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

static res_T
compute_map(struct stardis* stardis)
{
  res_T res = RES_OK;
  double time[2], pos[3];
  struct sdis_green_function* green = NULL;
  struct darray_estimators estimators;
  int estimators_initialized = 0;
  size_t p;

  ASSERT(stardis
    && (stardis->mode & MAP_COMPUTE) && !(stardis->mode & GREEN_MODE));

  darray_estimators_init(stardis->allocator, &estimators);
  estimators_initialized = 1;
  d3_set(pos, stardis->probe);
  d2_splat(time, stardis->probe[3]);

  ERR(darray_estimators_resize(&estimators,
    darray_estimators_size_get(&estimators) +
    darray_size_t_size_get(&stardis->compute_surface.primitives)));

  FOR_EACH(p, 0, darray_size_t_size_get(&stardis->compute_surface.primitives)) {
    ERR(sdis_solve_boundary(stardis->sdis_scn,
      stardis->samples,
      darray_size_t_cdata_get(&stardis->compute_surface.primitives) + p,
      darray_sides_cdata_get(&stardis->compute_surface.sides) + p,
      1,
      time,
      stardis->scale_factor,
      stardis->ambient_temp,
      stardis->ref_temp,
      stardis->dump_paths,
      darray_estimators_data_get(&estimators) + p));
  }
  dump_map(stardis, &estimators, stdout);

end:
  if(estimators_initialized) {
    struct sdis_estimator** est = darray_estimators_data_get(&estimators);
    FOR_EACH(p, 0, darray_size_t_size_get(&stardis->compute_surface.primitives)) {
      SDIS(estimator_ref_put(est[p]));
    }
    darray_estimators_release(&estimators);
  }
  if(green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

struct sdis_medium*
find_medium_by_name
  (struct stardis* stardis,
   const struct str* name,
   unsigned* out_id)
{
  struct sdis_medium* medium = NULL;
  size_t i;

  ASSERT(stardis && name);

  FOR_EACH(i, 0, darray_descriptions_size_get(&stardis->descriptions)) {
    struct description*
      desc = darray_descriptions_data_get(&stardis->descriptions) + i;
    if(desc->type == DESC_MAT_SOLID) {
      if(str_eq(name, &desc->d.solid.name)) {
        unsigned id = desc->d.solid.solid_id;
        ASSERT(darray_media_ptr_size_get(&stardis->media) > id);
        medium = darray_media_ptr_data_get(&stardis->media)[id];
        if(out_id) *out_id = id;
        break;
      }
    }
    else if(desc->type == DESC_MAT_FLUID) {
      if(str_eq(name, &desc->d.fluid.name)) {
        unsigned id = desc->d.fluid.fluid_id;
        ASSERT(darray_media_ptr_size_get(&stardis->media) > id);
        medium = darray_media_ptr_data_get(&stardis->media)[id];
        if(out_id) *out_id = id;
        break;
      }
    }
  }
  return medium;
}

/* Process an STL file describing a compute region
 * Triangles must be members of the geometry already */
res_T
read_compute_surface
  (struct stardis* stardis)
{
  res_T res = RES_OK;
  struct sstl* sstl = NULL;
  struct add_geom_ctx add_geom_ctx;
  struct sg3d_geometry_add_callbacks callbacks = SG3D_ADD_CALLBACKS_NULL__;
  const char* file;

  ASSERT(stardis);

  file = str_cget(&stardis->solve_name);
  callbacks.get_indices = add_geom_ctx_indices;
  callbacks.get_position = add_geom_ctx_position;
  callbacks.add_triangle = add_compute_surface_triangle;
  callbacks.merge_triangle = merge_compute_surface_triangle;

  ERR(sstl_create(stardis->logger, stardis->allocator, 0, &sstl));
  res = sstl_load(sstl, file);
  if(res != RES_OK) {
    logger_print(stardis->logger, LOG_ERROR,
      "Cannot read STL file: '%s'\n", file);
    goto error;
  }
  ERR(sstl_get_desc(sstl, &add_geom_ctx.stl_desc));
  ASSERT(add_geom_ctx.stl_desc.vertices_count <= UINT_MAX
    && add_geom_ctx.stl_desc.triangles_count <= UINT_MAX);

  add_geom_ctx.custom = &stardis->compute_surface;

  res = sg3d_geometry_add(
    stardis->geometry.sg3d,
    (unsigned)add_geom_ctx.stl_desc.vertices_count,
    (unsigned)add_geom_ctx.stl_desc.triangles_count,
    &callbacks,
    &add_geom_ctx);
  if(res != RES_OK) {
    logger_print(stardis->logger, LOG_ERROR,
      "Cannot add file content: '%s'\n", file);
    goto error;
  }

end:
  if(sstl) SSTL(ref_put(sstl));
  return res;
error:
  goto end;
}

res_T
stardis_compute
  (struct stardis* stardis)
{
  res_T res = RES_OK;

  ASSERT(stardis && (stardis->mode & COMPUTE_MODES));

  /* Compute */
  if(stardis->mode & PROBE_COMPUTE)
    ERR(compute_probe(stardis));
  else if(stardis->mode & PROBE_COMPUTE_ON_INTERFACE)
    ERR(compute_probe_on_interface(stardis));
  else if(stardis->mode & IR_COMPUTE)
    ERR(compute_camera(stardis));
  else if(stardis->mode & MEDIUM_COMPUTE)
    ERR(compute_medium(stardis));
  else if(stardis->mode & BOUNDARY_COMPUTE)
    ERR(compute_boundary(stardis));
  else if(stardis->mode & FLUX_BOUNDARY_COMPUTE)
    ERR(compute_flux_boundary(stardis));
  else if(stardis->mode & MAP_COMPUTE)
    ERR(compute_map(stardis));
  else FATAL("Unknown mode.\n");

end:
  return res;
error:
  logger_print(stardis->logger, LOG_ERROR,
    "%s: computation failed!\n", FUNC_NAME);
  goto end;
}
