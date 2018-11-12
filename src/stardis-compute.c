/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#include "stardis-app.h"
#include <sdis.h>
#include <sdis_version.h>
#include <rsys/double3.h>

#include <rsys/image.h>

static void
geometry_get_position
(const size_t ivert, 
 double pos[3],
 void* context)
{
  struct geometry* geom = context;
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
  ids[0] = geom->triangle[itri].indices[0];
  ids[1] = geom->triangle[itri].indices[1];
  ids[2] = geom->triangle[itri].indices[2];
}

static void
geometry_get_interface
(const size_t itri, 
 struct sdis_interface** interf, 
 void* context)
{
  struct geometry* geom = context;
  *interf = geom->interfaces[itri];
}

/*******************************************************************************
 * Fluid data
 ******************************************************************************/
struct fluid {
  double cp; /* Calorific capacity */
  double rho; /* Volumic mass */
  /* TinyExpr stuff to compute temperature */
  struct te_expr *temperature;
};

static double
fluid_get_calorific_capacity
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  (void)vtx;
  return fluid_props->cp;
}

static double
fluid_get_volumic_mass
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  (void)vtx;
  return fluid_props->rho;
}

res_T
set_fluid_temperature_fn(struct fluid* fluid, const char* math_expr)
{
  te_variable vars[4] = {
    TE_OFFSET("x", offsetof(struct sdis_rwalk_vertex, P[0])),
    TE_OFFSET("y", offsetof(struct sdis_rwalk_vertex, P[1])),
    TE_OFFSET("z", offsetof(struct sdis_rwalk_vertex, P[2])),
    TE_OFFSET("t", offsetof(struct sdis_rwalk_vertex, time))
  };

  ASSERT(math_expr);
  fluid->temperature = te_compile(math_expr, vars, 4, NULL);
  if(!fluid->temperature) return RES_BAD_ARG;
  return RES_OK;
}

static double
fluid_get_temperature
(const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  return te_eval(fluid_props->temperature, vtx);
}

void release_fluid_data(void* f)
{
  struct fluid* fluid = (struct fluid*)f;
  te_free(fluid->temperature);
}

/*******************************************************************************
 * Solid data
 ******************************************************************************/
struct solid {
  double cp; /* Calorific capacity */
  double lambda; /* Conductivity */
  double rho; /* Volumic mass */
  double delta; /* Numerical parameter */
  /* TinyExpr stuff to compute temperature & power */
  struct te_expr *t_init;
  struct te_expr *power;
};

static double
solid_get_calorific_capacity
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->cp;
}

static double
solid_get_thermal_conductivity
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->lambda;
}

static double
solid_get_volumic_mass
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->rho;
}

static double
solid_get_delta
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  (void)vtx;
  return solid_props->delta;
}

#if Stardis_VERSION_MINOR == 3
static double
solid_get_delta_boundary
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return solid_props->delta
    /* Emperical scale factor that ensures that delta_boundary > delta withouht
     * being an exact multiple of delta. */
    /** 2.1 */;
}
#endif

res_T
set_solid_temperature_fn(struct solid* solid, const char* math_expr)
{
  te_variable vars[4] = {
    TE_OFFSET("x", offsetof(struct sdis_rwalk_vertex, P[0])),
    TE_OFFSET("y", offsetof(struct sdis_rwalk_vertex, P[1])),
    TE_OFFSET("z", offsetof(struct sdis_rwalk_vertex, P[2])),
    TE_OFFSET("t", offsetof(struct sdis_rwalk_vertex, time))
  };

  ASSERT(math_expr);
  solid->t_init = te_compile(math_expr, vars, 4, NULL);
  if (!solid->t_init) return RES_BAD_ARG;
  return RES_OK;
}

static double
solid_get_temperature
(const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  if (vtx->time > 0) {
    return -1;
  }
  return te_eval(solid_props->t_init, vtx);
}

res_T
set_solid_power_fn(struct solid* solid, const char* math_expr, int* has)
{
  te_variable vars[4] = {
    TE_OFFSET("x", offsetof(struct sdis_rwalk_vertex, P[0])),
    TE_OFFSET("y", offsetof(struct sdis_rwalk_vertex, P[1])),
    TE_OFFSET("z", offsetof(struct sdis_rwalk_vertex, P[2])),
    TE_OFFSET("t", offsetof(struct sdis_rwalk_vertex, time))
  };

  ASSERT(math_expr);
  solid->power = te_compile(math_expr, vars, 4, NULL);
  if (!solid->power) return RES_BAD_ARG;
  if(has) /* has power? */
    *has = !(solid->power->type == TE_CONSTANT && solid->power->v.value == 0);
  return RES_OK;
}

static double
solid_get_power
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return te_eval(solid_props->power, vtx);
}

void release_solid_data(void* s)
{
  struct solid* solid = (struct solid*)s;
  te_free(solid->t_init);
  te_free(solid->power);
}

/*******************************************************************************
 * Interface data
 ******************************************************************************/
struct intface {
  double hc; /* Convection coefficient */
  double emissivity;
  double alpha;
  /* TinyExpr stuff to compute temperature & flux */
  struct te_expr *temperature;
  struct te_expr *flux;
};

static double
interface_get_convection_coef
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->hc;
}

res_T
set_interface_temperature_fn(struct intface* intface, const char* math_expr)
{
  te_variable vars[4] = {
    TE_OFFSET("x", offsetof(struct sdis_interface_fragment, P[0])),
    TE_OFFSET("y", offsetof(struct sdis_interface_fragment, P[1])),
    TE_OFFSET("z", offsetof(struct sdis_interface_fragment, P[2])),
    TE_OFFSET("t", offsetof(struct sdis_interface_fragment, time))
  };

  ASSERT(math_expr);
  intface->temperature = te_compile(math_expr, vars, 4, NULL);
  if (!intface->temperature) return RES_BAD_ARG;
  return RES_OK;
}

static double
interface_get_temperature
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);

  if (interface_props->temperature == NULL)
    return -1;
  
  return te_eval(interface_props->temperature, frag);
}

res_T
set_interface_flux_fn(struct intface* intface, const char* math_expr)
{
  te_variable vars[4] = {
    TE_OFFSET("x", offsetof(struct sdis_interface_fragment, P[0])),
    TE_OFFSET("y", offsetof(struct sdis_interface_fragment, P[1])),
    TE_OFFSET("z", offsetof(struct sdis_interface_fragment, P[2])),
    TE_OFFSET("t", offsetof(struct sdis_interface_fragment, time))
  };

  ASSERT(math_expr);
  intface->flux = te_compile(math_expr, vars, 4, NULL);
  if (!intface->flux) return RES_BAD_ARG;
  return RES_OK;
}

static double
interface_get_flux
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);

  if (interface_props->flux == NULL)
    return SDIS_FLUX_NONE;
  
  return te_eval(interface_props->flux, frag);
}

void release_interface_data(void* s)
{
  struct intface* intface = (struct intface*)s;
  te_free(intface->temperature);
  te_free(intface->flux);
}

static double
interface_get_emissivity
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->emissivity;
}


static double
interface_get_alpha
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->alpha;
}


static res_T
select_probe_type
(const struct sdis_scene* scn, 
 const struct triangle* triangle,
 const struct material* mat,
 double pos[], 
 size_t* iprim, 
 double uv[])
{
  res_T res = RES_OK;
  size_t i = 0;
  double projected_pos[3] = {0,0,0};
  double dp[3] = {0,0,0};
  double delta = 0;

  for (i=0; i<sa_size(triangle); ++i){
      
    res = sdis_scene_boundary_project_position(scn, i, pos, uv);

    res = sdis_scene_get_boundary_position(scn, i, uv, projected_pos);
  
    d3_sub(dp, pos, projected_pos);

    delta = mat[triangle[i].medium_front].delta; 
    if (d3_len(dp) < delta){
      *iprim = i;
      fprintf(stderr,"The probe is very close to the primitive %llu.\n",
        (long long int)*iprim );
      fprintf(stderr,"So the probe is moved to this primitive at the position: %g %g %g\n",
          SPLIT3(projected_pos));
      d3_set(pos, projected_pos);
      break;
    }
  }
  return res;
}

static void
dump_image(const struct sdis_accum_buffer* buf)
{
  struct sdis_accum_buffer_layout layout = SDIS_ACCUM_BUFFER_LAYOUT_NULL;
  double* temps = NULL;
  const struct sdis_accum* accums = NULL;
  size_t ix, iy;

  CHK(buf != NULL);
  CHK(sdis_accum_buffer_get_layout(buf, &layout) == RES_OK);

  temps = mem_alloc(layout.width*layout.height*sizeof(double));
  CHK(temps != NULL);

  /* Compute the per pixel temperature */
  CHK(sdis_accum_buffer_map(buf, &accums) == RES_OK);
  fprintf(stdout,"# vtk DataFile Version 2.0\nvtk output\nASCII\nDATASET STRUCTURED_POINTS\n");
  fprintf(stdout,"DIMENSIONS %i %i 1\n", (int)layout.width, (int)layout.height);
  fprintf(stdout,"ORIGIN 0 0 0\n");
  fprintf(stdout,"SPACING 1 1 1\n");
  fprintf(stdout,"POINT_DATA %i\n", (int)layout.width*(int)layout.height);
  fprintf(stdout,"SCALARS temperature float 1\n");
  fprintf(stdout,"LOOKUP_TABLE default\n");
  FOR_EACH(iy, 0, layout.height) {
    const struct sdis_accum* row_accums = accums + (layout.height - 1 - iy) * layout.width;
    double* row = temps + iy * layout.width;
    FOR_EACH(ix, 0, layout.width) {
      row[ix] = row_accums[ix].sum_weights / (double)row_accums[ix].nweights;
      fprintf(stdout,"%f\n",row[ix]);
    }
  }
  mem_rm(temps);
}


/*******************************************************************************
 *
 ******************************************************************************/
  
res_T
stardis_compute(struct stardis* stardis, enum stardis_mode mode)
{
  res_T res = RES_OK;
  struct sdis_device* dev = NULL;
  struct sdis_data* data = NULL;
  struct solid* solid_props = NULL;
  struct fluid* fluid_props = NULL;
  struct sdis_solid_shader solid_shader = SDIS_SOLID_SHADER_NULL;
  struct sdis_medium** solid_medium = NULL;
  struct sdis_fluid_shader fluid_shader = SDIS_FLUID_SHADER_NULL;
  struct sdis_medium** fluid_medium = NULL;

  struct sdis_interface_side_shader interface_shader_front =
    SDIS_INTERFACE_SIDE_SHADER_NULL;
  struct sdis_interface_side_shader interface_shader_back =
    SDIS_INTERFACE_SIDE_SHADER_NULL;
  struct sdis_interface_shader interface_shader_boundary = 
    SDIS_INTERFACE_SHADER_NULL;
  struct sdis_interface_shader interface_shader_connection = 
    SDIS_INTERFACE_SHADER_NULL;
  struct intface* interface_props = NULL;
  struct sdis_interface** interfaces = NULL;

  struct sdis_scene* scn = NULL; 
  struct sdis_estimator* estimator = NULL;
  struct sdis_mc temperature;
  struct sdis_accum_buffer* buf = NULL;
  struct sdis_camera* cam = NULL;

  int* bound2fluid = NULL;
  double pos[3] = {0,0,0};
  double time[2] = { 0, 0};
  size_t nfailures;
  unsigned i = 0;

  SDIS(device_create(NULL, NULL, stardis->nthreads, 1, &dev));

  /* Setup the fluid shader */
  fluid_shader.calorific_capacity = fluid_get_calorific_capacity;
  fluid_shader.volumic_mass = fluid_get_volumic_mass;
  fluid_shader.temperature = fluid_get_temperature;

  /* create fluid for each hc boundary */
  for (i=0; i<stardis->geometry.boundary_count; ++i){
    SDIS(data_create(dev, sizeof(struct fluid), ALIGNOF(struct fluid),
      release_fluid_data, &data));
    fluid_props = sdis_data_get(data); /* Fetch the allocated memory space */
    fluid_props->cp = 0;
    fluid_props->rho = 0;
    res = set_fluid_temperature_fn(fluid_props, stardis->boundary[i].T);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid temperature expression: %s\n", stardis->boundary[i].T);
      goto error;
    }
    SDIS(fluid_create(dev, &fluid_shader, data, sa_add(fluid_medium, 1)));
    SDIS(data_ref_put(data)); data = NULL;
    if (stardis->boundary[i].hc > -1) {
      size_t sz = sa_size(fluid_medium) - 1;
      ASSERT(sz < INT_MAX);
      sa_push(bound2fluid, (int)sz);
    } else {
      sa_push(bound2fluid, 0);
    }
  }

  /* Setup the solid shader */
  solid_shader.calorific_capacity = solid_get_calorific_capacity;
  solid_shader.thermal_conductivity = solid_get_thermal_conductivity;
  solid_shader.volumic_mass = solid_get_volumic_mass;
  solid_shader.delta_solid = solid_get_delta;  
#if Stardis_VERSION_MINOR == 3
  solid_shader.delta_boundary = solid_get_delta_boundary;
#endif
  solid_shader.temperature = solid_get_temperature;

  /* Create the solid medium */
  for (i=0; i<stardis->geometry.medium_count; ++i){
     /* Create and setup the solid physical properties */
    int has;
    SDIS(data_create(dev, sizeof(struct solid), ALIGNOF(struct solid), 
      release_solid_data, &data));
    solid_props = sdis_data_get(data); /* Fetch the allocated memory space */
    solid_props->cp     = stardis->material[i].cp;
    solid_props->lambda = stardis->material[i].lambda;
    solid_props->rho    = stardis->material[i].rho;
    solid_props->delta  = stardis->material[i].delta;
    solid_props->power = NULL; /* just in case we go to error */
    res = set_solid_temperature_fn(solid_props, stardis->material[i].Tinit);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid initial temperature expression: %s\n",
        stardis->material[i].Tinit);
      goto error;
    }
    res = set_solid_power_fn(solid_props, stardis->material[i].power, &has);
    if (res != RES_OK) {
      fprintf(stderr, "Invalid volumic power expression: %s\n",
        stardis->material[i].power);
      goto error;
    }
    if (has) solid_shader.volumic_power = solid_get_power;
    SDIS(solid_create(dev, &solid_shader, data, sa_add(solid_medium, 1)));
    SDIS(data_ref_put(data)); data = NULL;
  }

  /* Setup the interface shader */
  interface_shader_front.temperature = interface_get_temperature;
  interface_shader_front.flux = interface_get_flux;
  interface_shader_front.emissivity = NULL;
  interface_shader_front.specular_fraction = NULL;

  interface_shader_back.temperature = interface_get_temperature;
  interface_shader_back.flux = NULL;
  interface_shader_back.emissivity = interface_get_emissivity;
  interface_shader_back.specular_fraction = interface_get_alpha;

  interface_shader_boundary.convection_coef = interface_get_convection_coef;
  interface_shader_boundary.front = interface_shader_front;
  interface_shader_boundary.back = interface_shader_back;

  for (i=0; i<sa_size(stardis->geometry.triangle); ++i){
    unsigned bound_id = stardis->geometry.triangle[i].bound_id;

    if (bound_id != UINT_MAX){  /* boundary interface */
      double hc = stardis->boundary[bound_id].hc;
      char* T = stardis->boundary[bound_id].T;
      char* flux = stardis->boundary[bound_id].flux;
      double emissivity =
        stardis->boundary[stardis->geometry.triangle[i].bound_id].emissivity;
      double alpha =
        stardis->boundary[stardis->geometry.triangle[i].bound_id].specular_fraction;

      SDIS(data_create(dev, sizeof(struct intface), ALIGNOF(struct intface), 
        release_interface_data, &data));
      interface_props = sdis_data_get(data);
      if (hc > -1) {
        interface_props->hc = hc;
        interface_props->temperature = NULL;
        interface_props->flux = NULL;
      } else if (T != NULL){
        interface_props->flux = NULL;
        res = set_interface_temperature_fn(interface_props, T);
        if (res != RES_OK) {
          fprintf(stderr, "Invalid temperature expression: %s\n", T);
          goto error;
        }
      } else if (flux != NULL){
        interface_props->temperature = NULL;
        res = set_interface_flux_fn(interface_props, flux);
        if (res != RES_OK) {
          fprintf(stderr, "Invalid flux expression: %s\n", flux);
          goto error;
        }
      }
      interface_props->emissivity = emissivity; 
      interface_props->alpha = alpha;
      SDIS(interface_create(dev,
            solid_medium[stardis->geometry.triangle[i].medium_front],
            fluid_medium[bound2fluid[bound_id]],
            &interface_shader_boundary, data, sa_add(interfaces,1)));
      SDIS(data_ref_put(data)); data = NULL;

    } else { /* solid/solid interface */
      SDIS(interface_create(dev,
            solid_medium[stardis->geometry.triangle[i].medium_front],
            solid_medium[stardis->geometry.triangle[i].medium_back],
            &interface_shader_connection, NULL, sa_add(interfaces,1)));
    }
  }
  stardis->geometry.interfaces = interfaces;

  SDIS(scene_create(dev,
        sa_size(stardis->geometry.triangle),
        geometry_get_indices, geometry_get_interface,
        sa_size(stardis->geometry.vertex), 
        geometry_get_position, &stardis->geometry, &scn));


  if (mode == IR_COMPUTE){
   size_t width = (size_t)stardis->camera.img[0];
   size_t height = (size_t)stardis->camera.img[1];

    /* Setup the camera */
    SDIS(camera_create(dev, &cam));
    SDIS(camera_set_proj_ratio(cam, (double)width/(double)height));
    SDIS(camera_set_fov(cam, MDEG2RAD(stardis->camera.fov)));
    SDIS(camera_look_at(cam, 
          stardis->camera.pos,
          stardis->camera.tgt,
          stardis->camera.up));

    /* Create the accum buffer */
    SDIS(accum_buffer_create(dev,
          width,
          height,
          &buf));

    /* Launch the simulation */
    SDIS(solve_camera(scn, 
          cam,
          INF,
          stardis->scale_factor,
          stardis->radiative_temp[0], 
          stardis->radiative_temp[1], 
          width,
          height,
          (size_t)stardis->camera.spp,
          sdis_accum_buffer_write,
          buf));

    /* Write the image */
    dump_image(buf);
  }

  if (mode == PROBE_COMPUTE){
    /* Launch the probe simulation */
    pos[0] = stardis->probe[0];
    pos[1] = stardis->probe[1];
    pos[2] = stardis->probe[2];
    time[0] = time[1]  = stardis->probe[3];
    {
      double uv[2] = {0,0};
      size_t iprim = SIZE_MAX;

      res = select_probe_type(scn,
          stardis->geometry.triangle,
          stardis->material,
          pos,
          &iprim,
          uv);

      if(iprim == SIZE_MAX){
        SDIS(solve_probe(scn,
              stardis->N, 
              pos,
              time, 
              stardis->scale_factor,
              stardis->radiative_temp[0], 
              stardis->radiative_temp[1], 
              &estimator));
      } else {
        SDIS(solve_probe_boundary(scn,
              stardis->N,
              iprim,
              uv,
              time,
              SDIS_FRONT,
              stardis->scale_factor,
              stardis->radiative_temp[0], 
              stardis->radiative_temp[1], 
              &estimator));
      }
    }

    /* Fetch the estimation data */
    SDIS(estimator_get_temperature(estimator, &temperature));
    SDIS(estimator_get_failure_count(estimator, &nfailures));

    /* Print the results */
    printf("Temperature at [%g, %g, %g, %g] = %g +/- %g\n",
        pos[0], pos[1], pos[2], time[0],
        temperature.E, /* Expected value */
        temperature.SE); /* Standard error */
    printf("#failures: %lu/%lu\n", 
        (unsigned long)nfailures,
        (unsigned long)stardis->N);
  }

end:
  for (i=0; i<sa_size(solid_medium); ++i){
    SDIS(medium_ref_put(solid_medium[i]));
  }
  sa_release(solid_medium);

  for (i=0; i<sa_size(fluid_medium); ++i){
    SDIS(medium_ref_put(fluid_medium[i]));
  }
  sa_release(fluid_medium);

  for (i=0; i<sa_size(interfaces); ++i){
    SDIS(interface_ref_put(interfaces[i]));
  }
  sa_release(interfaces);
  stardis->geometry.interfaces = NULL;

  sa_release(bound2fluid);

  if (cam) SDIS(camera_ref_put(cam));
  if (buf) SDIS(accum_buffer_ref_put(buf));
  if (estimator) SDIS(estimator_ref_put(estimator));
  if (scn) SDIS(scene_ref_put(scn));
  if (dev) SDIS(device_ref_put(dev));

  return res;
error:
  if(data) SDIS(data_ref_put(data));
  goto end;
}
