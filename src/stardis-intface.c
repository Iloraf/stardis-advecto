#include "stardis-intface.h"
#include "stardis-app.h"
#include "stardis-output.h"

#include <tinyexpr.h>
#include <sdis.h>
#include <rsys/stretchy_array.h>

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

static double
interface_get_convection_coef
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->hc;
}

static double
interface_get_temperature
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);

  if (interface_props->temperature == NULL)
    return -1;

  return te_eval(interface_props->temperature, frag);
}

static double
interface_get_flux
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);

  if (interface_props->flux == NULL)
    return SDIS_FLUX_NONE;

  return te_eval(interface_props->flux, frag);
}

static double
interface_get_emissivity
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->emissivity;
}

static double
interface_get_alpha
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->alpha;
}

static void
release_interface_data
  (void* s)
{
  struct intface* intface = (struct intface*)s;
  te_free(intface->temperature);
  te_free(intface->flux);
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
create_intface
  (struct sdis_device* dev,
   unsigned tr_idx,
   struct htable_intface* htable_interfaces,
   struct geometry* geometry,
   const struct description* descriptions,
   struct sdis_medium* const* media)
{
  struct triangle *trg;
  struct int_descs int_descs = INT_DESCS_NULL;
  struct sdis_interface** p_intface;
  struct sdis_interface* intface;
  struct sdis_medium* front_med = NULL;
  struct sdis_medium* back_med = NULL;
  struct sdis_interface_side_shader* fluid_side_shader = NULL;
  struct sdis_data* data = NULL;
  unsigned fd, bd, cd;
  int fluid_count = 0, solid_count = 0;
  int solid_fluid_connection_count = 0, connection_count = 0, boundary_count = 0;
  res_T res = RES_OK;

  ASSERT(dev && htable_interfaces && geometry && descriptions && media);
  
  trg = geometry->triangle + tr_idx;
  fd = trg->front_description;
  bd = trg->back_description;
  cd = trg->connection_description;

  /* Create key */
  int_descs.front = fd;
  int_descs.back = bd;
  int_descs.connect = trg->connection_description;

  /* Search if interface already exists, or create it */
  p_intface = htable_intface_find(htable_interfaces, &int_descs);
  if (p_intface) {
    intface = *p_intface;
  } else {
    /* create new interface and register */
    struct sdis_interface_shader interface_shader = SDIS_INTERFACE_SHADER_NULL;
    struct intface* interface_props = NULL;
    unsigned id;
    int front_defined = (fd != UINT_MAX);
    int back_defined = (bd != UINT_MAX);
    int connect_defined = (cd != UINT_MAX);

    SDIS(data_create(dev, sizeof(struct intface), ALIGNOF(struct intface),
      release_interface_data, &data));
    interface_props = sdis_data_get(data);
    interface_props->temperature = NULL;
    interface_props->flux = NULL;
    interface_props->front_boundary_id = UINT_MAX;
    interface_props->back_boundary_id = UINT_MAX;

    if (front_defined) {
      switch (descriptions[fd].type) {
      case DESC_MAT_SOLID:
        id = descriptions[fd].d.solid.solid_id;
        solid_count++;
        front_med = media[id];
        break;
      case DESC_MAT_FLUID:
        fluid_count++;
        id = descriptions[fd].d.fluid.fluid_id;
        front_med = media[id];
        fluid_side_shader = &interface_shader.front;
        break;
      default: {
        char msg[1024];
        sprintf(msg, "%s (%s: %d): Invalid type.\n", __FUNCTION__, __FILE__, __LINE__);
        FATAL(msg);
      }
      }
    }
    if (back_defined) {
      switch (descriptions[bd].type) {
      case DESC_MAT_SOLID:
        id = descriptions[bd].d.solid.solid_id;
        solid_count++;
        back_med = media[id];
        break;
      case DESC_MAT_FLUID:
        fluid_count++;
        id = descriptions[bd].d.fluid.fluid_id;
        back_med = media[id];
        fluid_side_shader = &interface_shader.back;
        break;
      default: {
        char msg[1024];
        sprintf(msg, "%s (%s: %d): Invalid type.\n", __FUNCTION__, __FILE__, __LINE__);
        FATAL(msg);
      }
      }
    }
    if (connect_defined) {
      const struct description* connect = descriptions + cd;
      int type_checked = 0;
      struct sdis_medium* def_medium = NULL;
      unsigned ext_id;
      if (connect->type == DESC_SOLID_FLUID_CONNECT) {
        if (solid_count != 1 || fluid_count != 1) {
          ASSERT(front_defined && back_defined);
          fprintf(stderr,
            "Can only define a DESC_SOLID_FLUID_CONNECT between a fluid and a solid\n");
          res = RES_BAD_ARG;
          goto error;
        }
      } else {
        if (front_defined == back_defined) {
          fprintf(stderr,
            "Cannot define a boundary between 2 %s regions\n",
            front_defined ? "defined" : "undefined");
          res = RES_BAD_ARG;
          goto error;
        }
        def_medium = front_defined ? front_med : back_med;
        if (front_defined)
          interface_props->front_boundary_id = cd;
        else interface_props->back_boundary_id = cd;
      }
      switch (connect->type) {
      case DESC_BOUND_H_FOR_FLUID:
        if (sdis_medium_get_type(def_medium) != SDIS_FLUID) {
          fprintf(stderr,
            "Can only define a DESC_BOUND_H_FOR_FLUID boundary for a fluid region\n");
          res = RES_BAD_ARG;
          goto error;
        }
        type_checked = 1;
        /* fall through */
      case DESC_BOUND_H_FOR_SOLID:
        if (!type_checked
          &&  sdis_medium_get_type(def_medium) != SDIS_SOLID) {
          fprintf(stderr,
            "Can only define a DESC_BOUND_H_FOR_SOLID boundary for a solid region\n");
          res = RES_BAD_ARG;
          goto error;
        }
        ext_id = connect->d.h_boundary.mat_id; /* External material id */
        ASSERT(ext_id < sa_size(media));
        ASSERT(sdis_medium_get_type(media[ext_id]) ==
          (connect->type == DESC_BOUND_H_FOR_SOLID ? SDIS_FLUID : SDIS_SOLID));
        connection_count++;
        boundary_count++;
        if (front_defined) back_med = media[ext_id];
        else front_med = media[ext_id];
        interface_shader.convection_coef = interface_get_convection_coef;
        interface_shader.convection_coef_upper_bound = connect->d.h_boundary.hc_max;
        interface_props->hc = connect->d.h_boundary.hc;
        ASSERT(!fluid_side_shader); /* Cause defined medium is solid */
        if (connect->d.h_boundary.has_emissivity) {
          fluid_side_shader =
            front_defined ? &interface_shader.back : &interface_shader.front;
          fluid_side_shader->emissivity = interface_get_emissivity;
          fluid_side_shader->specular_fraction = interface_get_alpha;
          interface_props->emissivity = connect->d.h_boundary.emissivity;
          interface_props->alpha = connect->d.h_boundary.specular_fraction;
        }
        break;
      case DESC_BOUND_T_FOR_FLUID:
        if (sdis_medium_get_type(def_medium) != SDIS_FLUID) {
          fprintf(stderr,
            "Can only define a DESC_BOUND_T_FOR_FLUID boundary for a fluid region\n");
          res = RES_BAD_ARG;
          goto error;
        }
        type_checked = 1;
        /* fall through */
      case DESC_BOUND_T_FOR_SOLID:
        if (!type_checked
          &&  sdis_medium_get_type(def_medium) != SDIS_SOLID) {
          fprintf(stderr,
            "Can only define a DESC_BOUND_T_FOR_SOLID boundary for a solid region\n");
          res = RES_BAD_ARG;
          goto error;
        }
        ext_id = connect->d.t_boundary.mat_id; /* External material id */
        ASSERT(ext_id < sa_size(media));
        ASSERT(sdis_medium_get_type(media[ext_id]) == SDIS_FLUID);
        connection_count++;
        boundary_count++;
        if (front_defined) {
          back_med = media[ext_id];
          /* We set the known T inside
           * TODO: should be outside to allow contact resistances */
          interface_shader.front.temperature = interface_get_temperature;
        } else {
          front_med = media[ext_id];
          /* We set the known T inside
           * TODO: should be outside to allow contact resistances */
          interface_shader.back.temperature = interface_get_temperature;
        }
        ASSERT(connect->d.t_boundary.te_temperature);
        interface_props->temperature
          = te_duplicate(connect->d.t_boundary.te_temperature);
        if (connect->d.t_boundary.has_hc) {
          ASSERT(connect->type == DESC_BOUND_T_FOR_FLUID);
          interface_shader.convection_coef = interface_get_convection_coef;
          interface_shader.convection_coef_upper_bound = connect->d.t_boundary.hc_max;
          interface_props->hc = connect->d.t_boundary.hc;
        }
        break;
      case DESC_BOUND_F_FOR_SOLID:
        if (sdis_medium_get_type(def_medium) != SDIS_SOLID) {
          fprintf(stderr,
            "Can only define a DESC_BOUND_F_FOR_SOLID boundary for a solid region\n");
          res = RES_BAD_ARG;
          goto error;
        }
        connection_count++;
        boundary_count++;
        if (front_defined) {
          back_med = media[connect->d.f_boundary.mat_id];
          interface_shader.front.flux = interface_get_flux;
        } else {
          front_med = media[connect->d.f_boundary.mat_id];
          interface_shader.back.flux = interface_get_flux;
        }
        ASSERT(connect->d.f_boundary.te_flux);
        interface_props->flux = te_duplicate(connect->d.f_boundary.te_flux);
        break;
      case DESC_SOLID_FLUID_CONNECT:
        /* Both front and back should be defined;
         * if not will raise an error below */
        connection_count++;
        solid_fluid_connection_count++;
        ASSERT(fluid_side_shader);
        if (connect->d.sf_connect.has_hc) {
          interface_shader.convection_coef = interface_get_convection_coef;
          interface_shader.convection_coef_upper_bound = connect->d.sf_connect.hc_max;
          interface_props->hc = connect->d.sf_connect.hc;
        }
        if (connect->d.sf_connect.has_emissivity) {
          fluid_side_shader->emissivity = interface_get_emissivity;
          fluid_side_shader->specular_fraction = interface_get_alpha;
          interface_props->emissivity = connect->d.sf_connect.emissivity;
          interface_props->alpha = connect->d.sf_connect.specular_fraction;
        }
        break;
      default: {
        char msg[1024];
        sprintf(msg, "%s (%s: %d): Invalid type.\n", __FUNCTION__, __FILE__, __LINE__);
        FATAL(msg);
      }
      }
    }

    if ((fluid_count == 2)
      || (fluid_count + solid_count + connection_count < 2)
      || (boundary_count ?
      (fluid_count + solid_count != 1) : (fluid_count + solid_count != 2))
      || (solid_fluid_connection_count && (fluid_count != 1 || solid_count != 1)))
    {
      /* Incoherent triangle description */
      fprintf(stderr, "Incoherent triangle description (%u)\n", tr_idx);
      print_trg_as_obj(stderr, geometry->vertex, trg->indices.data);
      fprintf(stderr, "Front: ");
      if (!front_defined) fprintf(stderr, "undefined\n");
      else print_description(stderr, descriptions + fd);
      fprintf(stderr, "Back: ");
      if (!back_defined) fprintf(stderr, "undefined\n");
      else print_description(stderr, descriptions + bd);
      fprintf(stderr, "Connection: ");
      if (!connect_defined) fprintf(stderr, "undefined\n");
      else print_description(stderr, descriptions + cd);
      res = RES_BAD_ARG;
      goto error;
    }

    res = sdis_interface_create(dev, front_med, back_med,
      &interface_shader, data, &intface);
    SDIS(data_ref_put(data)); data = NULL;
    if (res != RES_OK) {
      fprintf(stderr,
        "Cannot create interface associated to triangle %u\n", tr_idx);
      goto error;
    }
    sa_push(geometry->interfaces, intface);
    res = htable_intface_set(htable_interfaces, &int_descs, &intface);
    if (res != RES_OK) goto error;
  }
  sa_push(geometry->interf_bytrg, intface);

end:
  return res;
error:
  goto end;
}