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

#include <star/s3d.h>
#include <star/sg3d_sXd_helper.h>

#include <rsys/double3.h>
#include <rsys/double2.h>
#include <rsys/logger.h>
#include <rsys/image.h>

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

struct filter_ctx {
  const struct stardis* stardis;
  unsigned prim;
  const struct description* desc;
  float pos[3];
  float dist;
  int outside;
  int probe_on_boundary;
};

#define FILTER_CTX_DEFAULT__ \
 { NULL, S3D_INVALID_ID, NULL, { 0, 0, 0 }, FLT_MAX, 0, 0 }

/* Filter used from a point query to determine not only one of the closest
 * point, but the better one if there are more than one. In some circumstances
 * it is not possible to determine the medium we are in using a given hit, but
 * it is possible using another equidistant hit :
 *
 *
 *   P             C
 *   +.............+---trg 1---
 *                 |
 *   medium 1    trg 2  medium 2
 *                 |
 *
 * C is the closest point from P, and 2 different hits at C are possible (one
 * on each triangle). However, only hit on trg 2 allows to find out that P is
 * in medium 1 using sign(PC.Ntrg1) as PC.Ntrg2 = 0.
 * The following filter function aims at selecting the hit on trg2 regardless
 * of the order in which the 2 triangles are checked.
 * One unexpected case cannot be decided though, but it implies that the
 * closest triangle has 2 different media on its sides and that P lies on the
 * triangle's plane :
 *
 *   P       C  medium 1
 *   +       +---trg---
 *              medium 2
 */
static int
hit_filter
  (const struct s3d_hit* hit,
   const float ray_org[3],
   const float* invalid_, /* In closest_point queries ray_dir is not informed */
   void* ray_data,
   void* filter_data)
{
  struct filter_ctx* filter_ctx = ray_data;
  float s, dir[3];
  enum sg3d_property_type prop;
  struct s3d_attrib interf_pos;
  const struct description* descriptions;
  unsigned descr[SG3D_PROP_TYPES_COUNT__];
  const struct stardis* stardis;

  (void)ray_org; (void)invalid_; (void)filter_data;
  ASSERT(hit && filter_ctx);
  ASSERT(hit->uv[0] == CLAMP(hit->uv[0], 0, 1));
  ASSERT(hit->uv[1] == CLAMP(hit->uv[1], 0, 1));

  if(filter_ctx->dist == hit->distance && filter_ctx->desc) {
    /* Cannot improve: keep previous!
     * Or we could end with a NULL desc */
    return 1; /* Skip */
  }

  stardis = filter_ctx->stardis;
  descriptions = darray_descriptions_cdata_get(&stardis->descriptions);

  CHK(s3d_primitive_get_attrib(&hit->prim, S3D_POSITION, hit->uv, &interf_pos)
    == RES_OK);

  if(hit->distance == 0) {
    filter_ctx->prim = hit->prim.prim_id;
    filter_ctx->desc = NULL; /* Not apply */
    f3_set(filter_ctx->pos, interf_pos.value);
    filter_ctx->dist = hit->distance;
    filter_ctx->outside = 0;
    filter_ctx->probe_on_boundary = 1;
    return 0; /* Keep */
  }

  /* Get the description IDs for this triangle */
  CHK(sg3d_geometry_get_unique_triangle_properties(stardis->geometry.sg3d,
    hit->prim.prim_id, descr) == RES_OK);

  if(descr[SG3D_FRONT] == descr[SG3D_BACK]) {
    /* Don't need to bother with sides */
    filter_ctx->prim = hit->prim.prim_id;
    filter_ctx->desc = (descr[SG3D_FRONT] == SG3D_UNSPECIFIED_PROPERTY)
      ? NULL : descriptions + descr[SG3D_FRONT];
    f3_set(filter_ctx->pos, interf_pos.value);
    filter_ctx->dist = hit->distance;
    filter_ctx->outside = (descr[SG3D_FRONT] == SG3D_UNSPECIFIED_PROPERTY);
    filter_ctx->probe_on_boundary = 0;
    return 0; /* Keep */
  }

  f3_sub(dir, interf_pos.value, ray_org);
  s = f3_dot(dir, hit->normal);

  if(s == 0) {
    filter_ctx->prim = hit->prim.prim_id;
    filter_ctx->desc = NULL; /* Cannot decide side */
    f3_set(filter_ctx->pos, interf_pos.value);
    filter_ctx->dist = hit->distance;
    filter_ctx->outside = 0;
    filter_ctx->probe_on_boundary = 0;
  }

  /* Determine which side was hit and the property to look at.
   * Warning: following Embree 2 convention for geometrical normals,
   * the Star3D hit normals are left-handed */
  prop = (s > 0) ? SG3D_BACK : SG3D_FRONT;
  if(descr[prop] == SG3D_UNSPECIFIED_PROPERTY) {
    filter_ctx->prim = hit->prim.prim_id;
    filter_ctx->desc = NULL;
    f3_set(filter_ctx->pos, interf_pos.value);
    filter_ctx->dist = hit->distance;
    filter_ctx->outside = 1;
    filter_ctx->probe_on_boundary = 0;
  } else {
    filter_ctx->prim = hit->prim.prim_id;
    filter_ctx->desc = descriptions + descr[prop];
    f3_set(filter_ctx->pos, interf_pos.value);
    filter_ctx->dist = hit->distance;
    filter_ctx->outside = 0;
    filter_ctx->probe_on_boundary = 0;
  }

  return 0; /* Keep */
}

/* Check probe position and move it to the closest point on an interface
 * if on_interface is set */
static res_T
check_probe_conform_to_type
  (const struct stardis* stardis,
   const int move2boundary,
   double pos[3],
   size_t* iprim,
   double uv[2])
{
  res_T res = RES_OK;
  struct s3d_device* s3d = NULL;
  struct s3d_scene* s3d_scn = NULL;
  struct s3d_shape* s3d_shp = NULL;
  struct s3d_scene_view* s3d_view = NULL;
  struct s3d_hit hit = S3D_HIT_NULL;
  struct s3d_vertex_data attribs;
  struct filter_ctx filter_ctx = FILTER_CTX_DEFAULT__;
  float origin[3];
  unsigned vcount, tcount, j;

  ASSERT(stardis && pos && iprim && uv);

  attribs.type = S3D_FLOAT3;
  attribs.usage = S3D_POSITION;
  attribs.get = sg3d_sXd_geometry_get_position;

  ERR(s3d_device_create(stardis->logger, stardis->allocator, 0, &s3d));
  ERR(s3d_scene_create(s3d, &s3d_scn));
  ERR(s3d_shape_create_mesh(s3d, &s3d_shp));

  /* Back to s3d API type for ntris and nverts */
  ERR(sg3d_geometry_get_unique_triangles_count(stardis->geometry.sg3d, &tcount));
  ERR(sg3d_geometry_get_unique_vertices_count(stardis->geometry.sg3d, &vcount));
  ERR(s3d_mesh_setup_indexed_vertices(s3d_shp,
    tcount, sg3d_sXd_geometry_get_indices,
    vcount, &attribs, 1, stardis->geometry.sg3d));
  /* Need a filter to sort out some tricky configurations (see filter comments) */
  ERR(s3d_mesh_set_hit_filter_function(s3d_shp, hit_filter, NULL));
  ERR(s3d_scene_attach_shape(s3d_scn, s3d_shp));
  ERR(s3d_scene_view_create(s3d_scn, S3D_TRACE, &s3d_view));

  S3D(device_ref_put(s3d)); s3d = NULL;
  S3D(scene_ref_put(s3d_scn)); s3d_scn = NULL;
  S3D(shape_ref_put(s3d_shp)); s3d_shp = NULL;

  f3_set_d3(origin, pos);
  filter_ctx.stardis = stardis;
  ERR(s3d_scene_view_closest_point(s3d_view, origin, FLT_MAX, &filter_ctx, &hit));
  S3D(scene_view_ref_put(s3d_view)); s3d_view = NULL;
  if(S3D_HIT_NONE(&hit)) {
    res = RES_BAD_ARG;
    goto error;
  }
  ASSERT(filter_ctx.dist == hit.distance);

  if(move2boundary) {
    /* Need to move probe to closest boundary */
    int danger = 0;
    if(filter_ctx.outside) {
      /* Not an error as probe moved to boundary */
      logger_print(stardis->logger, LOG_WARNING,
        "Probe was outside the model.\n");
    }
    if(!filter_ctx.desc) {
      /* Not an error as probe moved to boundary */
      if(!filter_ctx.probe_on_boundary && !filter_ctx.outside)
        logger_print(stardis->logger, LOG_WARNING,
          "Could not determine the medium probe is in.\n");
    } else {
      if(filter_ctx.desc->type == DESC_MAT_SOLID) {
        double delta = filter_ctx.desc->d.solid.delta;
        ASSERT(delta < INF);
        logger_print(stardis->logger, LOG_OUTPUT,
          "Probe was in solid '%s'.\n", str_cget(&filter_ctx.desc->d.solid.name));
        if(filter_ctx.dist > 2 * delta) {
          logger_print(stardis->logger, LOG_ERROR,
            "Probe moved to (%g, %g, %g), primitive %u, uv = (%g, %g).\n",
            SPLIT3(filter_ctx.pos), hit.prim.prim_id, SPLIT2(hit.uv));
          logger_print(stardis->logger, LOG_ERROR,
            "Move is %g delta long. Use -p instead of -P.\n",
            filter_ctx.dist / delta);
          res = RES_BAD_ARG;
          goto error;
        }
        if(filter_ctx.dist > 0.5 * delta) {
          logger_print(stardis->logger, LOG_WARNING,
            "Probe was %g delta from closest boundary. "
            "Consider using -P instead of -p.\n",
            filter_ctx.dist / delta);
        } else {
          if(filter_ctx.dist != 0)
            logger_print(stardis->logger, LOG_OUTPUT,
              "Probe was %g delta from closest boundary.\n",
              filter_ctx.dist / delta);
        }
      } else {
        /* TODO: check move length wrt local geometry? */
        logger_print(stardis->logger, LOG_OUTPUT,
          "Probe was in fluid '%s'.\n", str_cget(&filter_ctx.desc->d.fluid.name));
        logger_print(stardis->logger, LOG_OUTPUT,
          "Probe distance from closest boundary was %g.\n", filter_ctx.dist);
      }
    }
    /* Check if moved to vertex/edge */
    FOR_EACH(j, 0, 2) {
      if(CLAMP(hit.uv[j], 0.0005, 0.9995) != hit.uv[j])
        danger++;
    }
    if(danger) {
      logger_print(stardis->logger, LOG_WARNING,
        "Probe %s close to / on %s. "
        "If computation fails, try moving it slightly.\n",
        (filter_ctx.dist != 0 ? "moved" : "is"), (danger == 1 ? "an edge" : "a vertex"));
    }
    if(filter_ctx.probe_on_boundary) {
      logger_print(stardis->logger, LOG_OUTPUT,
        "Probe is on primitive %u, uv = (%g, %g), not moved.\n",
        hit.prim.prim_id, SPLIT2(hit.uv));
    } else {
      logger_print(stardis->logger, LOG_OUTPUT,
        "Probe moved to (%g, %g, %g), primitive %u, uv = (%g, %g).\n",
        SPLIT3(filter_ctx.pos), hit.prim.prim_id, SPLIT2(hit.uv));
    }
    d3_set_f3(pos, filter_ctx.pos);
  } else {
    /* Need to check medium */
    if(filter_ctx.outside) {
      logger_print(stardis->logger, LOG_ERROR,
        "Probe is outside the model.\n");
      logger_print(stardis->logger, LOG_ERROR,
        "Closest geometry is primitive %u, uv = (%g, %g), pos (%g, %g, %g).\n",
        hit.prim.prim_id, SPLIT2(hit.uv), SPLIT3(filter_ctx.pos));
      res = RES_BAD_ARG;
      goto error;
    }
    if(filter_ctx.probe_on_boundary) {
      logger_print(stardis->logger, LOG_ERROR,
        "Probe is on primitive %u, uv = (%g, %g). Use -P instead of -p.\n",
        hit.prim.prim_id, SPLIT2(hit.uv));
      res = RES_BAD_ARG;
      goto error;
    }
    if(!filter_ctx.desc) {
      logger_print(stardis->logger, LOG_ERROR,
        "Could not determine the medium probe is in. "
        "Try moving it slightly.\n");
      logger_print(stardis->logger, LOG_ERROR,
        "Closest geometry is primitive %u, uv = (%g, %g), distance %g.\n",
        hit.prim.prim_id, SPLIT2(hit.uv), filter_ctx.dist);
      res = RES_BAD_ARG;
      goto error;
    }
    logger_print(stardis->logger, LOG_OUTPUT,
      "Probe is in solid '%s'.\n", str_cget(&filter_ctx.desc->d.solid.name));
    if(filter_ctx.desc->type == DESC_MAT_SOLID) {
      double delta = filter_ctx.desc->d.solid.delta;
      if(filter_ctx.dist < 0.25 * delta) {
        logger_print(stardis->logger, LOG_ERROR,
          "Probe is %g delta from closest boundary. Use -P instead of -p.\n",
          filter_ctx.dist / delta);
        logger_print(stardis->logger, LOG_ERROR,
          "Closest geometry is primitive %u, uv = (%g, %g).\n",
          hit.prim.prim_id, SPLIT2(hit.uv));
        res = RES_BAD_ARG;
        goto error;
      }
      if(filter_ctx.dist < 0.5 * delta) {
        logger_print(stardis->logger, LOG_WARNING,
          "Probe is %g delta from closest boundary. "
          "Consider using -P instead of -p.\n",
          filter_ctx.dist / delta);
      } else {
        logger_print(stardis->logger, LOG_OUTPUT,
          "Probe is %g delta from closest boundary.\n",
          filter_ctx.dist / delta);
      }
    } else {
      logger_print(stardis->logger, LOG_WARNING,
        "Probe is in fluid '%s': computing fluid temperature, "
        "not using a specific position.\n",
        str_cget(&filter_ctx.desc->d.fluid.name));
      /* In fluid; TODO: check distance wrt local geometry (use 4V/S?) */
    }
  }

  *iprim = hit.prim.prim_id;
  d2_set_f2(uv, hit.uv);
end:
  if(s3d) S3D(device_ref_put(s3d));
  if(s3d_scn) S3D(scene_ref_put(s3d_scn));
  if(s3d_shp) S3D(shape_ref_put(s3d_shp));
  if(s3d_view) S3D(scene_view_ref_put(s3d_view));

  return res;
error:
  goto end;
}

static res_T
compute_probe(struct stardis* stardis)
{
  res_T res = RES_OK;
  double uv[2] = { 0,0 };
  size_t iprim = SIZE_MAX;
  struct sdis_green_function* green = NULL;
  struct sdis_estimator* estimator = NULL;
  struct dump_path_context dump_ctx;
  struct sdis_solve_probe_args args = SDIS_SOLVE_PROBE_ARGS_DEFAULT;
  FILE* stream = NULL;

  ASSERT(stardis && (stardis->mode & MODE_PROBE_COMPUTE));

  ERR(check_probe_conform_to_type(stardis, 0, stardis->probe, &iprim, uv));

  args.nrealisations = stardis->samples;
  d3_set(args.position, stardis->probe);
  d2_set(args.time_range, stardis->time_range);

  if(stardis->mode & (MODE_BIN_GREEN | MODE_GREEN)) {
    ERR(sdis_solve_probe_green_function(stardis->sdis_scn, &args, &green));
    if(stardis->mode & MODE_BIN_GREEN) {
      ASSERT(!str_is_empty(&stardis->bin_green_filename));
      stream = fopen(str_cget(&stardis->bin_green_filename), "wb");
      if(!stream) {
        res = RES_IO_ERR;
        goto error;
      }
      ERR(dump_green_bin(green, stardis, stream));
      fclose(stream); stream = NULL;
      if(str_cget(&stardis->end_paths_filename)
        && strlen(str_cget(&stardis->end_paths_filename)))
      {
        stream = fopen(str_cget(&stardis->end_paths_filename), "w");
        if(!stream) {
          res = RES_IO_ERR;
          goto error;
        }
        ERR(dump_paths_end(green, stardis, stream));
        fclose(stream); stream = NULL;
      }
    }
    if(stardis->mode & MODE_GREEN) {
      ERR(dump_green_ascii(green, stardis, stdout));
    }
  } else {
    args.register_paths = stardis->dump_paths;
    ERR(sdis_solve_probe(stardis->sdis_scn, &args, &estimator));
    ERR(print_single_MC_result(estimator, stardis, stdout));
    /* Dump paths recorded according to user settings */
    dump_ctx.stardis = stardis;
    dump_ctx.rank = 0;
    ERR(sdis_estimator_for_each_path(estimator, dump_path, &dump_ctx));
  }

end:
  if(stream) fclose(stream);
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
  double uv[2] = { 0,0 };
  size_t iprim = SIZE_MAX;
  struct sdis_estimator* estimator = NULL;
  struct sdis_green_function* green = NULL;
  struct dump_path_context dump_ctx;
  struct sdis_solve_probe_boundary_args args
    = SDIS_SOLVE_PROBE_BOUNDARY_ARGS_DEFAULT;
  FILE* stream = NULL;

  ASSERT(stardis && (stardis->mode & MODE_PROBE_COMPUTE_ON_INTERFACE));
  ERR(check_probe_conform_to_type(stardis, 1, stardis->probe, &iprim, uv));
  ASSERT(iprim != SIZE_MAX);

  args.nrealisations = stardis->samples;
  args.iprim = iprim;
  d3_set(args.uv, uv);
  args.side = SDIS_FRONT;
  d2_set(args.time_range, stardis->time_range);

  if(stardis->mode & (MODE_BIN_GREEN | MODE_GREEN)) {
    ERR(sdis_solve_probe_boundary_green_function(stardis->sdis_scn, &args,
      &green));
    if(stardis->mode & MODE_BIN_GREEN) {
      ASSERT(!str_is_empty(&stardis->bin_green_filename));
      stream = fopen(str_cget(&stardis->bin_green_filename), "wb");
      if(!stream) {
        res = RES_IO_ERR;
        goto error;
      }
      ERR(dump_green_bin(green, stardis, stream));
      fclose(stream); stream = NULL;
      if(str_cget(&stardis->end_paths_filename)
        && strlen(str_cget(&stardis->end_paths_filename)))
      {
        stream = fopen(str_cget(&stardis->end_paths_filename), "w");
        if(!stream) {
          res = RES_IO_ERR;
          goto error;
        }
        ERR(dump_paths_end(green, stardis, stream));
        fclose(stream); stream = NULL;
      }
    }
    if(stardis->mode & MODE_GREEN) {
      ERR(dump_green_ascii(green, stardis, stdout));
    }
  } else {
    args.register_paths = stardis->dump_paths;
    ERR(sdis_solve_probe_boundary(stardis->sdis_scn, &args, &estimator));
    ERR(print_single_MC_result(estimator, stardis, stdout));
    /* Dump paths recorded according to user settings */
    dump_ctx.stardis = stardis;
    dump_ctx.rank = 0;
    ERR(sdis_estimator_for_each_path(estimator, dump_path, &dump_ctx));
  }

end:
  if(stream) fclose(stream);
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
  double proj_ratio;
  size_t width, height;
  struct sdis_estimator_buffer* buf = NULL;
  struct sdis_solve_camera_args args = SDIS_SOLVE_CAMERA_ARGS_DEFAULT;
  struct sdis_camera* cam = NULL;
  struct dump_path_context dump_ctx;
  size_t definition[2];
  size_t ix, iy;

  ASSERT(stardis
    && !(stardis->mode & (MODE_BIN_GREEN | MODE_GREEN))
    && (stardis->mode & MODE_IR_COMPUTE));

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

  args.cam = cam;
  d2_set(args.time_range, stardis->camera.time_range);
  args.image_resolution[0] = width;
  args.image_resolution[1] = height;
  args.spp = (size_t)stardis->camera.spp;
  args.register_paths = stardis->dump_paths;

  /* Launch the simulation */
  ERR(sdis_solve_camera(stardis->sdis_scn, &args, &buf));

  /* Write the image */
  ASSERT(stardis->camera.fmt == STARDIS_RENDERING_OUTPUT_FILE_FMT_VTK
  || stardis->camera.fmt == STARDIS_RENDERING_OUTPUT_FILE_FMT_HT);
  if(stardis->camera.fmt == STARDIS_RENDERING_OUTPUT_FILE_FMT_VTK)
    ERR(dump_vtk_image(buf, stdout));
  else ERR(dump_ht_image(buf, stdout));

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
  struct sdis_medium* medium = NULL;
  struct sdis_estimator* estimator = NULL;
  struct sdis_green_function* green = NULL;
  struct sdis_solve_medium_args args = SDIS_SOLVE_MEDIUM_ARGS_DEFAULT;
  struct dump_path_context dump_ctx;
  FILE* stream = NULL;

  ASSERT(stardis && (stardis->mode & MODE_MEDIUM_COMPUTE));

  /* Find medium */
  medium = find_medium_by_name(stardis, &stardis->solve_name, NULL);
  if(medium == NULL) { /* Not found */
    logger_print(stardis->logger, LOG_ERROR,
      "Cannot solve medium '%s' (unknown medium)\n",
      str_cget(&stardis->solve_name));
    res = RES_BAD_ARG;
    goto error;
  }

  args.nrealisations = stardis->samples;
  args.medium = medium;
  d2_set(args.time_range, stardis->time_range);

  if(stardis->mode & (MODE_BIN_GREEN | MODE_GREEN)) {
    ERR(sdis_solve_medium_green_function(stardis->sdis_scn, &args, &green));
    if(stardis->mode & MODE_BIN_GREEN) {
      ASSERT(!str_is_empty(&stardis->bin_green_filename));
      stream = fopen(str_cget(&stardis->bin_green_filename), "wb");
      if(!stream) {
        res = RES_IO_ERR;
        goto error;
      }
      ERR(dump_green_bin(green, stardis, stream));
      fclose(stream); stream = NULL;
      if(str_cget(&stardis->end_paths_filename)
        && strlen(str_cget(&stardis->end_paths_filename)))
      {
        stream = fopen(str_cget(&stardis->end_paths_filename), "w");
        if(!stream) {
          res = RES_IO_ERR;
          goto error;
        }
        ERR(dump_paths_end(green, stardis, stream));
        fclose(stream); stream = NULL;
      }
    }
    if(stardis->mode & MODE_GREEN) {
      ERR(dump_green_ascii(green, stardis, stdout));
    }
  } else {
    args.register_paths = stardis->dump_paths;
    ERR(sdis_solve_medium(stardis->sdis_scn, &args, &estimator));
    ERR(print_single_MC_result(estimator, stardis, stdout));
    /* Dump paths recorded according to user settings */
    dump_ctx.stardis = stardis;
    dump_ctx.rank = 0;
    ERR(sdis_estimator_for_each_path(estimator, dump_path, &dump_ctx));
  }

end:
  if(stream) fclose(stream);
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
  struct sdis_green_function* green = NULL;
  struct sdis_estimator* estimator = NULL;
  struct dump_path_context dump_ctx;
  struct sdis_solve_boundary_args args = SDIS_SOLVE_BOUNDARY_ARGS_DEFAULT;
  FILE* stream = NULL;

  ASSERT(stardis && (stardis->mode & MODE_BOUNDARY_COMPUTE));

  args.nrealisations = stardis->samples;
  args.primitives
    = darray_size_t_cdata_get(&stardis->compute_surface.primitives);
  args.sides = darray_sides_cdata_get(&stardis->compute_surface.sides);
  args.nprimitives
    = darray_size_t_size_get(&stardis->compute_surface.primitives);
  d2_set(args.time_range, stardis->time_range);

  if(stardis->mode & (MODE_BIN_GREEN | MODE_GREEN)) {
    ERR(sdis_solve_boundary_green_function(stardis->sdis_scn, &args, &green));
    if(stardis->mode & MODE_BIN_GREEN) {
      ASSERT(!str_is_empty(&stardis->bin_green_filename));
      stream = fopen(str_cget(&stardis->bin_green_filename), "wb");
      if(!stream) {
        res = RES_IO_ERR;
        goto error;
      }
      ERR(dump_green_bin(green, stardis, stream));
      fclose(stream); stream = NULL;
      if(str_cget(&stardis->end_paths_filename)
        && strlen(str_cget(&stardis->end_paths_filename)))
      {
        stream = fopen(str_cget(&stardis->end_paths_filename), "w");
        if(!stream) {
          res = RES_IO_ERR;
          goto error;
        }
        ERR(dump_paths_end(green, stardis, stream));
        fclose(stream); stream = NULL;
      }
    }
    if(stardis->mode & MODE_GREEN) {
      ERR(dump_green_ascii(green, stardis, stdout));
    }
  } else {
    args.register_paths = stardis->dump_paths;
    ERR(sdis_solve_boundary(stardis->sdis_scn, &args, &estimator));
    ERR(print_single_MC_result(estimator, stardis, stdout));
    /* Dump paths recorded according to user settings */
    dump_ctx.stardis = stardis;
    dump_ctx.rank = 0;
    ERR(sdis_estimator_for_each_path(estimator, dump_path, &dump_ctx));
  }

end:
  if(stream) fclose(stream);
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
  struct sdis_green_function* green = NULL;
  struct sdis_estimator* estimator = NULL;
  struct sdis_solve_boundary_flux_args args
    = SDIS_SOLVE_BOUNDARY_FLUX_ARGS_DEFAULT;
  struct dump_path_context dump_ctx;

  ASSERT(stardis && (stardis->mode & MODE_FLUX_BOUNDARY_COMPUTE));

  args.nrealisations = stardis->samples;
  args.primitives
    = darray_size_t_cdata_get(&stardis->compute_surface.primitives);
  args.nprimitives
    = darray_size_t_size_get(&stardis->compute_surface.primitives);
  d2_set(args.time_range, stardis->time_range);

  ERR(sdis_solve_boundary_flux(stardis->sdis_scn, &args, &estimator));
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
  struct sdis_green_function* green = NULL;
  struct sdis_solve_boundary_args args = SDIS_SOLVE_BOUNDARY_ARGS_DEFAULT;
  struct darray_estimators estimators;
  int estimators_initialized = 0;
  size_t p;

  ASSERT(stardis
    && (stardis->mode & MODE_MAP_COMPUTE)
    && !(stardis->mode & (MODE_BIN_GREEN | MODE_GREEN)));

  darray_estimators_init(stardis->allocator, &estimators);
  estimators_initialized = 1;

  ERR(darray_estimators_resize(&estimators,
    darray_estimators_size_get(&estimators) +
    darray_size_t_size_get(&stardis->compute_surface.primitives)));

  FOR_EACH(p, 0, darray_size_t_size_get(&stardis->compute_surface.primitives)) {

    args.nrealisations = stardis->samples;
    args.primitives
      = darray_size_t_cdata_get(&stardis->compute_surface.primitives);
    args.sides = darray_sides_cdata_get(&stardis->compute_surface.sides);
    args.nprimitives
      = darray_size_t_size_get(&stardis->compute_surface.primitives);
    d2_set(args.time_range, stardis->time_range);
    args.register_paths = stardis->dump_paths;

    ERR(sdis_solve_boundary(stardis->sdis_scn, &args,
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
  size_t i;
  double _2area = 0;

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

  /* Compute area of the compute surface */
  FOR_EACH(i, 0, add_geom_ctx.stl_desc.triangles_count) {
    const unsigned* trg = add_geom_ctx.stl_desc.indices + (i * 3);
    const float* v0f = add_geom_ctx.stl_desc.vertices + (trg[0] * 3);
    const float* v1f = add_geom_ctx.stl_desc.vertices + (trg[1] * 3);
    const float* v2f = add_geom_ctx.stl_desc.vertices + (trg[2] * 3);
    double v0[3], v1[3], v2[3], edge0[3], edge1[3], normal[3], norm;

    /* Compute component area and volume */
    d3_sub(edge0, d3_set_f3(v1, v1f), d3_set_f3(v0, v0f));
    d3_sub(edge1, d3_set_f3(v2, v2f), d3_set_f3(v0, v0f));
    d3_cross(normal, edge0, edge1);
    norm = d3_normalize(normal, normal);
    ASSERT(norm);
    _2area += norm;
  }

  stardis->compute_surface.area = _2area * 0.5
    * stardis->scale_factor * stardis->scale_factor;

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
  if(stardis->mode & MODE_PROBE_COMPUTE)
    ERR(compute_probe(stardis));
  else if(stardis->mode & MODE_PROBE_COMPUTE_ON_INTERFACE)
    ERR(compute_probe_on_interface(stardis));
  else if(stardis->mode & MODE_IR_COMPUTE)
    ERR(compute_camera(stardis));
  else if(stardis->mode & MODE_MEDIUM_COMPUTE)
    ERR(compute_medium(stardis));
  else if(stardis->mode & MODE_BOUNDARY_COMPUTE)
    ERR(compute_boundary(stardis));
  else if(stardis->mode & MODE_FLUX_BOUNDARY_COMPUTE)
    ERR(compute_flux_boundary(stardis));
  else if(stardis->mode & MODE_MAP_COMPUTE)
    ERR(compute_map(stardis));
  else FATAL("Unknown mode.\n");

end:
  return res;
error:
  logger_print(stardis->logger, LOG_ERROR,
    "%s: computation failed!\n", FUNC_NAME);
  goto end;
}
