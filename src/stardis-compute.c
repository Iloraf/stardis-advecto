/* Copyright (C) 2018 |Meso|Star> (contact@meso-star.com)*/

#include "stardis-app.h"
#include <sdis.h>

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
  char* temperature; 
};

static double
fluid_get_calorific_capacity
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  return fluid_props->cp;
}

static double
fluid_get_volumic_mass
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  return fluid_props->rho;
}

static double
fluid_get_temperature
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct fluid* fluid_props = sdis_data_cget(data);
  double temperature = 0;
  char* math_expr = fluid_props->temperature;
  double x, y, z, t;  
  /* Store variable names and pointers. */
  te_variable vars[] = {{"x", &x}, {"y", &y}, {"z", &z}, {"t", &t}};
  int err;
    
  /* Compile the expression with variables. */
  te_expr *expr = te_compile(math_expr, vars, 4, &err);
  x = vtx->P[0];
  y = vtx->P[1];
  z = vtx->P[2];
  t = vtx->time;
  temperature = te_eval(expr);
  te_free(expr);
  return temperature;
}

/*******************************************************************************
 * Solid data
 ******************************************************************************/
struct solid {
  double cp; /* Calorific capacity */
  double lambda; /* Conductivity */
  double rho; /* Volumic mass */
  double delta; /* Numerical parameter */
  char* temperature;
  char* power;
};

static double
solid_get_calorific_capacity
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return solid_props->cp;
}

static double
solid_get_thermal_conductivity
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return solid_props->lambda;
}

static double
solid_get_volumic_mass
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return solid_props->rho;
}

static double
solid_get_delta
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return solid_props->delta;
}

static double
solid_get_delta_boundary
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  const struct solid* solid_props = sdis_data_cget(data);
  return solid_props->delta
    /* Emperical scale factor that ensures that delta_boundary > delta withouht
     * being an exact multiple of delta. */
    * 2.1 ;
}

static double
solid_get_temperature
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  double temperature = 0;
  const struct solid* solid_props = sdis_data_cget(data);
  if (vtx->time > 0){
    return -1;
  }else{
    char* math_expr = solid_props->temperature;
    double x, y, z;  
    /* Store variable names and pointers. */
    te_variable vars[] = {{"x", &x}, {"y", &y}, {"z", &z}};
    int err;
    
    /* Compile the expression with variables. */
    te_expr *expr = te_compile(math_expr, vars, 3, &err);
    x = vtx->P[0];
    y = vtx->P[1];
    z = vtx->P[2];
    temperature = te_eval(expr);
    te_free(expr);
    return temperature;
  }
}

static double
solid_get_power
  (const struct sdis_rwalk_vertex* vtx, struct sdis_data* data)
{
  double power = 0;
  const struct solid* solid_props = sdis_data_cget(data);
  char* math_expr = solid_props->temperature;
  double x, y, z, t;  
  /* Store variable names and pointers. */
  te_variable vars[] = {{"x", &x}, {"y", &y}, {"z", &z}, {"t", &t}};
  int err;
    
  /* Compile the expression with variables. */
  te_expr *expr = te_compile(math_expr, vars, 3, &err);
  x = vtx->P[0];
  y = vtx->P[1];
  z = vtx->P[2];
  t = vtx->time;
  power = te_eval(expr);
  te_free(expr);
  return power;
}

/*******************************************************************************
 * Interface data
 ******************************************************************************/
struct interface {
  double hc; /* Convection coefficient */
  char* temperature;
  double epsilon;
  double alpha;
};

static double
interface_get_convection_coef
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct interface* interface_props = sdis_data_cget(data);
  return interface_props->hc;
}

static double
interface_get_temperature
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct interface* interface_props = sdis_data_cget(data);

  if (interface_props->temperature == NULL) {
    return -1;
  } else {

    double temperature = 0;
    char* math_expr = interface_props->temperature;
    double x, y, z, t;  
    /* Store variable names and pointers. */
    te_variable vars[] = {{"x", &x}, {"y", &y}, {"z", &z}, {"t", &t}};
    int err;

    /* Compile the expression with variables. */
    te_expr *expr = te_compile(math_expr, vars, 4, &err);
    x = frag->P[0];
    y = frag->P[1];
    z = frag->P[2];
    t = frag->time;
    temperature = te_eval(expr);
    te_free(expr);
    return temperature;
  }
}

static double
interface_get_epsilon
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct interface* interface_props = sdis_data_cget(data);
  return interface_props->epsilon;
}


static double
interface_get_alpha
  (const struct sdis_interface_fragment* frag, struct sdis_data* data)
{
  const struct interface* interface_props = sdis_data_cget(data);
  return interface_props->alpha;
}

/*******************************************************************************
 *
 ******************************************************************************/
  
res_T
stardis_compute(struct stardis* stardis)
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

  struct sdis_interface_side_shader interface_shader_front = SDIS_INTERFACE_SIDE_SHADER_NULL;
  struct sdis_interface_side_shader interface_shader_back = SDIS_INTERFACE_SIDE_SHADER_NULL;
  struct sdis_interface_shader interface_shader_boundary = SDIS_INTERFACE_SHADER_NULL;
  struct sdis_interface_shader interface_shader_connection = SDIS_INTERFACE_SHADER_NULL;
  struct interface* interface_props = NULL;
  struct sdis_interface** interfaces = NULL;

  struct sdis_scene* scn;
  struct sdis_estimator* estimator;
  struct sdis_mc temperature;

  int* bound2fluid = NULL;
  double pos[3] = {0,0,0};
  double time = 0;
  size_t nfailures;
  int i = 0;

  SDIS(device_create(NULL, NULL, stardis->nthreads, 0, &dev));

  /* Setup the fluid shader */
  fluid_shader.calorific_capacity = fluid_get_calorific_capacity;
  fluid_shader.volumic_mass = fluid_get_volumic_mass;
  fluid_shader.temperature = fluid_get_temperature;

  /* create fluid for each hc boundary */
  for (i=0; i<stardis->geometry.boundary_count; ++i){
    if (stardis->boundary[i].hc > -1) {
      
      SDIS(data_create(dev, sizeof(struct fluid), ALIGNOF(struct fluid), NULL, &data));
      fluid_props = sdis_data_get(data); /* Fetch the allocated memory space */
      fluid_props->cp = 0;
      fluid_props->rho = 0;
      fluid_props->temperature = stardis->boundary[i].T; 
      SDIS(fluid_create(dev, &fluid_shader, data, sa_add(fluid_medium, 1)));
      SDIS(data_ref_put(data));

      sa_push(bound2fluid, sa_size(fluid_medium)-1);
    } else {
      sa_push(bound2fluid, 0);
    }
  }

  /* Setup the solid shader */
  solid_shader.calorific_capacity = solid_get_calorific_capacity;
  solid_shader.thermal_conductivity = solid_get_thermal_conductivity;
  solid_shader.volumic_mass = solid_get_volumic_mass;
  solid_shader.delta_solid = solid_get_delta;
  solid_shader.delta_boundary = solid_get_delta_boundary;
  solid_shader.temperature = solid_get_temperature;
  solid_shader.volumic_power = solid_get_power;

  /* Create the solid medium */
  for (i=0; i<stardis->geometry.medium_count; ++i){
  /* Create and setup the solid physical properties */
    SDIS(data_create(dev, sizeof(struct solid), ALIGNOF(struct solid), NULL, &data));
    solid_props = sdis_data_get(data); /* Fetch the allocated memory space */
    solid_props->cp     = stardis->material[i].cp;
    solid_props->lambda = stardis->material[i].lambda;
    solid_props->rho    = stardis->material[i].rho;
    solid_props->delta  = stardis->material[i].delta;
    solid_props->temperature = stardis->material[i].Tinit;
    solid_props->temperature = stardis->material[i].power;
    SDIS(solid_create(dev, &solid_shader, data, sa_add(solid_medium, 1)));
    SDIS(data_ref_put(data));
  }

  /* Setup the interface shader */
  interface_shader_front.temperature = interface_get_temperature;
  interface_shader_front.flux = NULL;
  interface_shader_front.emissivity = interface_get_epsilon;
  interface_shader_front.specular_fraction = interface_get_alpha;

  interface_shader_back = interface_shader_front;

  interface_shader_boundary.convection_coef = interface_get_convection_coef;
  interface_shader_boundary.front = interface_shader_front;
  interface_shader_boundary.back = interface_shader_back;

  for (i=0; i<sa_size(stardis->geometry.triangle); ++i){
    int bound_id = stardis->geometry.triangle[i].bound_id;

    if (bound_id > -1){  /* boundary interface */
      double hc = stardis->boundary[bound_id].hc;
      char* T = stardis->boundary[bound_id].T;

      SDIS(data_create(dev, sizeof(struct interface), ALIGNOF(struct interface), NULL, &data));
      interface_props = sdis_data_get(data);
      if (hc > -1) {
        interface_props->hc = hc;
        interface_props->temperature = NULL;
      } else {
        interface_props->temperature = T;
      }
      interface_props->epsilon = 0;
      interface_props->alpha = 0;
      SDIS(interface_create(dev,
            solid_medium[stardis->geometry.triangle[i].medium_front],
            fluid_medium[bound2fluid[bound_id]],
            &interface_shader_boundary, data, sa_add(interfaces,1)));
      SDIS(data_ref_put(data));

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

  /* Launch the probe simulation */
  pos[0] = stardis->probe[0];
  pos[1] = stardis->probe[1];
  pos[2] = stardis->probe[2];
  time   = stardis->probe[3];
  SDIS(solve_probe(scn, stardis->N, pos, time, 1.0, 300.0, 300.0, &estimator));

  /* Fetch the estimation data */
  SDIS(estimator_get_temperature(estimator, &temperature));
  SDIS(estimator_get_failure_count(estimator, &nfailures));

  /* Print the results */
  printf("Temperature at [%g, %g, %g, %g] = %g +/- %g\n",
      pos[0], pos[1], pos[2], time,
      temperature.E, /* Expected value */
      temperature.SE); /* Standard error */
  printf("#failures: %lu/%lu\n", 
      (unsigned long)nfailures,
      (unsigned long)stardis->N);


exit:
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

  SDIS(estimator_ref_put(estimator));
  SDIS(scene_ref_put(scn));
  SDIS(device_ref_put(dev));
  return res;
error:
  goto exit;
}

