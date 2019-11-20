/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#include "stardis-app.h"
#include "stardis-output.h"
#include "stardis-compute.h"
#include "stardis-fluid.h"
#include "stardis-solid.h"
#include "stardis-intface.h"

#include <sdis.h>
#include <sdis_version.h>
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

static void
geometry_get_position
  (const size_t ivert, 
   double pos[3],
   void* context)
{
  struct geometry* geom = context;
  ASSERT(geom);
  pos[0] = geom->vertex[ivert].xyz[0];
  pos[1] = geom->vertex[ivert].xyz[1];
  pos[2] = geom->vertex[ivert].xyz[2];
}

static void
geometry_get_indices
  (const size_t itri,
   size_t ids[3],
   void* context)
{
  struct geometry* geom = context;
  ASSERT(geom);
  ids[0] = geom->triangle[itri].indices.data[0];
  ids[1] = geom->triangle[itri].indices.data[1];
  ids[2] = geom->triangle[itri].indices.data[2];
}

static void
geometry_get_interface
  (const size_t itri, 
   struct sdis_interface** interf, 
   void* context)
{
  struct geometry* geom = context;
  ASSERT(geom);
  *interf = geom->interf_bytrg[itri];
}

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
  size_t i = 0, found = SIZE_MAX;
  enum description_type min_type = DESCRIPTION_TYPE_COUNT__;
  const struct triangle* triangle;
  const struct vertex* vertex;
  const struct description* descriptions;

  ASSERT(scn && stardis && pos && iprim && uv);
  triangle = stardis->geometry.triangle;
  vertex = stardis->geometry.vertex;
  descriptions = stardis->descriptions;
  for (i = 0; i < sa_size(triangle); ++i) {
    const struct triangle* tr = triangle + i;
    double dp[3], d, projected_pos[3], nn[3];
    float v0[3], v1[3], v2[3], e1[3], e2[3], n[3];
    const struct description *desc = NULL;
    enum sdis_side s;

    res = sdis_scene_boundary_project_position(scn, i, pos, tmp_uv);
    if (res != RES_OK) return res;
    res = sdis_scene_get_boundary_position(scn, i, tmp_uv, projected_pos);
    if (res != RES_OK) return res;
    d3_sub(dp, projected_pos, pos);
    d = d3_len(dp);

    if (d == 0) {
      /* Best possible match */
      found = i;
      fprintf(stderr,
        "The probe is on the primitive %llu.\n", (long long int)*iprim);
      break;
    }
    if (d >= min_d) {
      /* No improvement */
      continue;
    }
    min_d = d;
    d2_set(min_uv, tmp_uv);

    /* Find side */
    f3_set(v0, vertex[tr->indices.data[0]].xyz);
    f3_set(v1, vertex[tr->indices.data[1]].xyz);
    f3_set(v2, vertex[tr->indices.data[2]].xyz);
    f3_sub(e1, v1, v0);
    f3_sub(e2, v2, v0);
    f3_cross(n, e1, e2);

    /* Stardis solver convention is triangle normal is back side */
    d3_set_f3(nn, n);
    s = (d3_dot(nn, dp) < 0) ? SDIS_BACK : SDIS_FRONT;

    if (s == SDIS_FRONT) {
      if (tr->front_description != UINT_MAX) {
        desc = descriptions + tr->front_description;
      }
    } else {
      if (tr->back_description != UINT_MAX) {
        desc = descriptions + tr->back_description;
      }
    }

    min_desc = desc;
    if (!desc) continue;

    min_type = desc->type;
    if (desc->type == DESC_MAT_SOLID) {
      found = i;
      min_delta = desc->d.solid.delta;
      d3_set(new_pos, projected_pos);
    }
  }

  if (!min_desc) {
    /* Doesn't handle the case where pos is outside the model
     * but very close and could be considered on the boundary */
    fprintf(stderr, "The probe is outside the model.\n");
    return RES_BAD_ARG;
  }

  if (on_interface) {
    *iprim = found;
    if (min_type == DESC_MAT_FLUID)
      fprintf(stderr, "Probe was on a fluid.\n");
    fprintf(stderr, "The probe is moved to %g,%g,%g (primitive %llu)\n",
      SPLIT3(new_pos), (long long int)*iprim);
    if (min_type == DESC_MAT_SOLID && min_d > min_delta)
      fprintf(stderr, "WARNING: The probe move is %.1f delta long.\n", min_d / min_delta);
    d3_set(pos, new_pos);
    d2_set(uv, min_uv);
  } else {
    if (min_type == DESC_MAT_FLUID)
      fprintf(stderr,
        "The probe is in a fluid: computing fluid temperature, not using a specific position.\n");
    if (min_type == DESC_MAT_SOLID && min_d < min_delta) {
      fprintf(stderr,
        "WARNING: The probe is very close to the primitive %llu (%.1f delta).\n",
        (long long int)found, min_d / min_delta);
      fprintf(stderr,
        "To have this probe moved on the closest interface use -P instead of -p\n");
    }
  }

  return res;
}

static res_T
create_holder
  (struct counts* counts,
   struct dummies* dummies,
   struct description* description,
   struct sdis_device* dev,
   struct sdis_medium*** media,
   const int is_green)
{
  res_T res = RES_OK;
  /* Could be less restrictive if green output included positions/dates */
  int prohibited = is_green ? NO_VAR_ALLOWED : ALL_VARS_ALLOWED;

  switch (description->type) {
  case DESC_BOUND_H_FOR_SOLID:
    counts->hbound_count++;
    /* Create an external fluid with bound temp as fluid temp */
    res = create_fluid(dev, NULL, 1, 1, is_green, 1, NULL,
      description->d.h_boundary.T, media, &description->d.h_boundary.mat_id);
    if (res != RES_OK) goto error;
    break;
  case DESC_BOUND_H_FOR_FLUID:
    /* Create an external solid with bound temp as solid temp */
    counts->hbound_count++;
    res = create_solid(dev, NULL, INF, 1, 1, 1, is_green, 1, NULL,
      description->d.h_boundary.T, NULL, media, NULL,
      &description->d.h_boundary.mat_id);
    if (res != RES_OK) goto error;
    break;
  case DESC_BOUND_T_FOR_SOLID:
    counts->tbound_count++;
    ASSERT(description->d.t_boundary.T != NULL);
    res = compile_expr_to_fn(&description->d.t_boundary.te_temperature,
      description->d.t_boundary.T, prohibited, NULL);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid boundary temperature expression: %s\n",
        description->d.t_boundary.T);
      goto error;
    }
    if (dummies->dummy_fluid) {
      /* Reuse external dummy fluid */
      description->d.t_boundary.mat_id = dummies->dummy_fluid_id;
    } else {
      /* Create dummy fluid */
      res = create_fluid(dev, NULL, 1, 1, is_green, 1, NULL,
        NULL, media, &description->d.t_boundary.mat_id);
      if (res != RES_OK) goto error;
      dummies->dummy_fluid = sa_last(*media);
      dummies->dummy_fluid_id = description->d.t_boundary.mat_id;
    }
    break;
  case DESC_BOUND_T_FOR_FLUID:
    counts->tbound_count++;
    ASSERT(description->d.t_boundary.T != NULL);
    res = compile_expr_to_fn(&description->d.t_boundary.te_temperature,
      description->d.t_boundary.T, prohibited, NULL);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid boundary temperature expression: %s\n",
        description->d.t_boundary.T);
      goto error;
    }
    if (dummies->dummy_solid) {
      /* Reuse external dummy solid */
      description->d.t_boundary.mat_id = dummies->dummy_solid_id;
    } else {
      /* Create dummy solid */
      res = create_solid(dev, NULL, 1, 1, 1, 1, is_green, 1, NULL,
        NULL, NULL, media, NULL, &description->d.t_boundary.mat_id);
      if (res != RES_OK) goto error;
      dummies->dummy_solid = sa_last(*media);
      dummies->dummy_solid_id = description->d.t_boundary.mat_id;
    }
    break;
  case DESC_BOUND_F_FOR_SOLID:
    counts->fbound_count++;
    ASSERT(description->d.f_boundary.flux != NULL);
    res = compile_expr_to_fn(&description->d.f_boundary.te_flux,
      description->d.f_boundary.flux, 1, NULL);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid boundary flux expression: %s\n",
        description->d.f_boundary.flux);
      goto error;
    }
    if (dummies->dummy_fluid) {
      /* Reuse external dummy fluid */
      description->d.f_boundary.mat_id = dummies->dummy_fluid_id;
    } else {
      /* Create dummy fluid */
      res = create_fluid(dev, NULL, 1, 1, is_green, 1, NULL, NULL,
        media, &description->d.f_boundary.mat_id);
      if (res != RES_OK) goto error;
      dummies->dummy_fluid = sa_last(*media);
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
    res = create_solid(dev,
      description->d.solid.name,
      description->d.solid.lambda,
      description->d.solid.rho,
      description->d.solid.cp,
      description->d.solid.delta,
      is_green,
      0,
      description->d.solid.Tinit,
      description->d.solid.Temp,
      description->d.solid.power,
      media,
      &description->d.solid.has_power,
      &description->d.solid.solid_id);
    if (res != RES_OK) goto error;
    break;
  case DESC_MAT_FLUID:
    counts->fmed_count++;
    res = create_fluid(dev,
      description->d.fluid.name,
      description->d.fluid.rho,
      description->d.fluid.cp,
      is_green,
      0,
      description->d.fluid.Tinit,
      description->d.fluid.Temp,
      media,
      &description->d.fluid.fluid_id);
    if (res != RES_OK) goto error;

    break;
  default: {
    char msg[1024];
    sprintf(msg, "%s (%s: %d): Invalid type.\n", __FUNCTION__, __FILE__, __LINE__);
    FATAL(msg);
  }
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
  res = select_probe_type(scn, stardis, 0, pos, &iprim, uv);
  if (res != RES_OK) goto error;

  if (mode & GREEN_MODE) {
    ASSERT(iprim == SIZE_MAX);
    res = sdis_solve_probe_green_function(scn,
      stardis->N,
      pos,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      &green);
    if (res != RES_OK) goto error;
    res = dump_green(green, counts, stardis->descriptions,
      stardis->radiative_temp);
    if (res != RES_OK) goto error;
  } else {
    ASSERT(iprim == SIZE_MAX);
    d2_splat(time, stardis->probe[3]);
    res = sdis_solve_probe(scn,
      stardis->N,
      pos,
      time,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      stardis->dump_paths,
      &estimator);
    if (res != RES_OK) goto error;
    res = print_single_MC_result(mode, estimator, stardis);
    if (res != RES_OK) goto error;
  }
end:
  if (estimator) SDIS(estimator_ref_put(estimator));
  if (green) SDIS(green_function_ref_put(green));
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
  res = select_probe_type(scn, stardis, 1, pos, &iprim, uv);
  if (res != RES_OK) goto error;

  if (mode & GREEN_MODE) {
    ASSERT(iprim == SIZE_MAX);
    res = sdis_solve_probe_boundary_green_function(scn,
      stardis->N,
      iprim,
      uv,
      SDIS_FRONT,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      &green);
    if (res != RES_OK) goto error;
    res = dump_green(green, counts, stardis->descriptions,
      stardis->radiative_temp);
    if (res != RES_OK) goto error;
  } else {
    ASSERT(iprim == SIZE_MAX);
    d2_splat(time, stardis->probe[3]);
    res = sdis_solve_probe_boundary(scn,
      stardis->N,
      iprim,
      uv,
      time,
      SDIS_FRONT, /*                                       FIXME!!!!!!! */
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      stardis->dump_paths,
      &estimator);
    if (res != RES_OK) goto error;
    res = print_single_MC_result(mode, estimator, stardis);
    if (res != RES_OK) goto error;
  }
end:
  if (estimator) SDIS(estimator_ref_put(estimator));
  if (green) SDIS(green_function_ref_put(green));
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
  res = sdis_camera_create(dev, &cam);
  if (res != RES_OK) goto error;
  res = sdis_camera_set_proj_ratio(cam, (double)width / (double)height);
  if (res != RES_OK) goto error;
  res = sdis_camera_set_fov(cam, MDEG2RAD(stardis->camera.fov));
  if (res != RES_OK) goto error;
  res = sdis_camera_look_at(cam,
    stardis->camera.pos,
    stardis->camera.tgt,
    stardis->camera.up);
  if (res != RES_OK) goto error;

  /* Launch the simulation */
  time[0] = time[1] = stardis->camera.u.time;
  res = sdis_solve_camera(scn,
    cam,
    time,
    stardis->scale_factor,
    stardis->radiative_temp[0],
    stardis->radiative_temp[1],
    width,
    height,
    (size_t)stardis->camera.spp,
    stardis->dump_paths,
    &buf);
  if (res != RES_OK) goto error;

  /* Write the image */
  res = dump_image(buf);
  if (res != RES_OK) goto error;

end:
  if (cam) SDIS(camera_ref_put(cam));
  if (buf) SDIS(estimator_buffer_ref_put(buf));
  return res;
error:
  goto end;
}

static res_T
compute_medium
  (struct sdis_scene* scn,
   struct sdis_medium** media,
   struct stardis* stardis,
   const int mode)
{
  res_T res = RES_OK;
  double time[2];
  size_t sz, ii;
  struct sdis_medium* medium = NULL;
  struct sdis_estimator* estimator = NULL;
  struct sdis_green_function* green = NULL;

  ASSERT(scn && media && stardis && (mode & MEDIUM_COMPUTE));

  /* Find medium */
  sz = sa_size(stardis->descriptions);
  FOR_EACH(ii, 0, sz) {
    struct description* desc = stardis->descriptions + ii;
    if (desc->type == DESC_MAT_SOLID) {
      if (0 == strcmp(stardis->solve_name, desc->d.solid.name)) {
        ASSERT(sa_size(media) > ii);
        medium = media[ii];
        break;
      }
    }
    else if (desc->type == DESC_MAT_FLUID) {
      if (0 == strcmp(stardis->solve_name, desc->d.fluid.name)) {
        ASSERT(sa_size(media) > ii);
        medium = media[ii];
        break;
      }
    }
  }
  if (medium == NULL) { /* Not found */
    fprintf(stderr, "Cannot solve medium %s (unknown medium)\n",
      stardis->solve_name);
    res = RES_BAD_ARG;
    goto error;
  }

  if (mode & GREEN_MODE) {
    res = sdis_solve_medium_green_function(scn,
      stardis->N,
      medium,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      &green);
    if (res != RES_OK) goto error;
  } else {
    d2_splat(time, stardis->probe[3]);
    time[0] = time[1] = stardis->probe[3];
    res = sdis_solve_medium(scn,
      stardis->N,
      medium,
      time,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      stardis->dump_paths,
      &estimator);
    if (res != RES_OK) goto error;
    res = print_single_MC_result(mode, estimator, stardis);
    if (res != RES_OK) goto error;
  }
end:
  if (estimator) SDIS(estimator_ref_put(estimator));
  if (green) SDIS(green_function_ref_put(green));
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

  /* FIXME: MOVE TO BOUNDARY SETTING */
  ASSERT(sa_size(stardis->boundary.primitives)
    == sa_size(stardis->boundary.sides));
  if (sa_size(stardis->boundary.primitives) == 0) {
    fprintf(stderr, "Cannot solve boundary %s (empty geometry)\n",
      stardis->solve_name);
  }

  d3_set(pos, stardis->probe);

  if (mode & GREEN_MODE) {
    res = sdis_solve_boundary_green_function(scn,
      stardis->N,
      stardis->boundary.primitives,
      stardis->boundary.sides,
      sa_size(stardis->boundary.primitives),
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      &green);
    if (res != RES_OK) goto error;
    res = dump_green(green, counts, stardis->descriptions,
      stardis->radiative_temp);
    if (res != RES_OK) goto error;
  } else {
    d2_splat(time, stardis->probe[3]);
    res = sdis_solve_boundary(scn,
      stardis->N,
      stardis->boundary.primitives,
      stardis->boundary.sides,
      sa_size(stardis->boundary.primitives),
      time,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      stardis->dump_paths,
      &estimator);
    if (res != RES_OK) goto error;
    res = print_single_MC_result(mode, estimator, stardis);
    if (res != RES_OK) goto error;
  }
end:
  if (estimator) SDIS(estimator_ref_put(estimator));
  if (green) SDIS(green_function_ref_put(green));
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

  /* FIXME: MOVE TO BOUNDARY SETTING */
  if (sa_size(stardis->boundary.primitives) == 0) {
    fprintf(stderr, "Cannot solve boundary %s (empty geometry)\n",
      stardis->solve_name);
  }

  d3_set(pos, stardis->probe);
  d2_splat(time, stardis->probe[3]);
  res = sdis_solve_boundary_flux(scn,
    stardis->N,
    stardis->boundary.primitives,
    sa_size(stardis->boundary.primitives),
    time,
    stardis->scale_factor,
    stardis->radiative_temp[0],
    stardis->radiative_temp[1],
    &estimator);
  if (res != RES_OK) goto error;
  res = print_single_MC_result(mode, estimator, stardis);
  if (res != RES_OK) goto error;
end:
  if (estimator) SDIS(estimator_ref_put(estimator));
  if (green) SDIS(green_function_ref_put(green));
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
  struct sdis_estimator** estimators = NULL;
  size_t p;

  ASSERT(scn && stardis && (mode & MAP_COMPUTE)
    && !(mode & GREEN_MODE));
  (void)mode;

  /* FIXME: MOVE TO BOUNDARY SETTING */
  ASSERT(sa_size(stardis->boundary.primitives)
    == sa_size(stardis->boundary.sides));
  if (sa_size(stardis->boundary.primitives) == 0) {
    fprintf(stderr, "Cannot solve boundary %s (empty geometry)\n",
      stardis->solve_name);
  }

  d3_set(pos, stardis->probe);
  d2_splat(time, stardis->probe[3]);

  sa_add(estimators, sa_size(stardis->boundary.primitives));

  FOR_EACH(p, 0, sa_size(stardis->boundary.primitives)) {
    res = sdis_solve_boundary(scn,
      stardis->N,
      stardis->boundary.primitives + p,
      stardis->boundary.sides + p,
      1,
      time,
      stardis->scale_factor,
      stardis->radiative_temp[0],
      stardis->radiative_temp[1],
      stardis->dump_paths,
      estimators + p);
    if (res != RES_OK) goto error;
  }
  dump_map(stardis, estimators);

end:
  if (estimators) {
    FOR_EACH(p, 0, sa_size(stardis->boundary.primitives)) {
      SDIS(estimator_ref_put(estimators[p]));
    }
    sa_release(estimators);
  }
  if (green) SDIS(green_function_ref_put(green));
  return res;
error:
  goto end;
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
compile_expr_to_fn
  (struct te_expr** f,
   const char* math_expr,
   const int prohibited,
   int* is_zero)
{
  te_variable vars[4] = {
    TE_DEF_OFFSET("x", offsetof(struct sdis_rwalk_vertex, P[0])),
    TE_DEF_OFFSET("y", offsetof(struct sdis_rwalk_vertex, P[1])),
    TE_DEF_OFFSET("z", offsetof(struct sdis_rwalk_vertex, P[2])),
    TE_DEF_OFFSET("t", offsetof(struct sdis_rwalk_vertex, time))
  };
  int fst = 0, lst = 4;

  ASSERT(f && math_expr);
  ASSERT(prohibited == (prohibited & NO_VAR_ALLOWED)); /* Use only defined flags */

  if (prohibited & XYZ_PROHIBITED) fst = 3;
  if (prohibited & T_PROHIBITED) lst = 3;
  *f = te_compile(math_expr, vars + fst, lst - fst, NULL);
  if (!*f) {
    if (!te_compile(math_expr, vars, 4, NULL))
      return RES_BAD_ARG; /* Expr is not valid */
    /* Expr is valid */
    if (prohibited != ALL_VARS_ALLOWED)
      fprintf(stderr, "Expression \"%s\" use a probibited variable ", math_expr);
    if (prohibited == NO_VAR_ALLOWED)
      fprintf(stderr, "(no variable allowed)\n");
    else if (prohibited == XYZ_PROHIBITED)
      fprintf(stderr, "(xyz prohibited)\n");
    else {
      ASSERT(prohibited == T_PROHIBITED);
      fprintf(stderr, "(t prohibited)\n");
    }
    return RES_BAD_OP; /* Expr is valid, but use a forbidden var */
  }
  if (is_zero) *is_zero = !((*f)->type == TE_CONSTANT && (*f)->v.value == 0);
  return RES_OK;
}

res_T
stardis_compute
  (struct stardis* stardis,
   const int mode)
{
  res_T res = RES_OK;
  struct sdis_device* dev = NULL;
  struct sdis_medium** media = NULL;
  struct sdis_scene* scn = NULL;
  struct dummies dummies = DUMMIES_NULL;
  struct htable_intface htable_interfaces;
  int htable_interfaces_initialised = 0;
  struct counts counts = COUNTS_NULL;
  unsigned i = 0;
  int verbose = 1;

  ASSERT(stardis && (mode & COMPUTE_MODES));

  res = sdis_device_create(NULL, &stardis->allocator, stardis->nthreads, verbose, &dev);
  if (res != RES_OK) goto error;

  /* Create media and property holders found in descriptions */
  for (i = 0; i < sa_size(stardis->descriptions); ++i) {
    res = create_holder(&counts, &dummies, stardis->descriptions + i,
      dev, &media, mode & GREEN_MODE);
    if (res != RES_OK) goto error;
  }

  /* Create interfaces */
  htable_intface_init(&stardis->allocator, &htable_interfaces);
  htable_interfaces_initialised = 1;
  for (i = 0; i < sa_size(stardis->geometry.triangle); ++i) {
    res = create_intface(dev, i, &htable_interfaces, &stardis->geometry,
      stardis->descriptions, media);
    if (res != RES_OK) goto error;
  }

  /* Create scene */
  res = sdis_scene_create(dev,
    sa_size(stardis->geometry.triangle),
    geometry_get_indices, geometry_get_interface,
    sa_size(stardis->geometry.vertex),
    geometry_get_position, &stardis->geometry, &scn);
  if(res != RES_OK) {
    fprintf(stderr, "%s: could not setup the scene!\n", __FUNCTION__);
    goto error;
  }

  /* If the scene has holes dump them */
  res = create_edge_file_if(scn, &stardis->allocator);
  if (res != RES_OK) goto error;

  /* Compute */
  if (mode & PROBE_COMPUTE)
    res = compute_probe(scn, &counts, stardis, mode);
  else if (mode & PROBE_COMPUTE_ON_INTERFACE)
    res = compute_probe_on_interface(scn, &counts, stardis, mode);
  else if (mode & IR_COMPUTE)
    res = compute_camera(dev, scn, stardis, mode);
  else if (mode & MEDIUM_COMPUTE)
    res = compute_medium(scn, media, stardis, mode);
  else if (mode & BOUNDARY_COMPUTE)
    res = compute_boundary(scn, &counts, stardis, mode);
  else if (mode & FLUX_BOUNDARY_COMPUTE)
    res = compute_flux_boundary(scn, stardis, mode);
  else if (mode & MAP_COMPUTE)
    res = compute_map(scn, stardis, mode);
  else FATAL("Unknown mode.\n");
  if(res != RES_OK) {
    fprintf(stderr, "%s: computation failed!\n", __FUNCTION__);
    goto error;
  }

end:
  if (htable_interfaces_initialised)
    htable_intface_release(&htable_interfaces);
  for (i = 0; i < sa_size(media); ++i)
    SDIS(medium_ref_put(media[i]));
  sa_release(media);

  if (scn) SDIS(scene_ref_put(scn));
  if (dev) SDIS(device_ref_put(dev));

  return res;
error:
  goto end;
}
