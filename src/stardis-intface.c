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

#include "stardis-intface.h"
#include "stardis-app.h"
#include "stardis-compute.h"
#include "stardis-output.h"
#include "stardis-solid.h"
#include "stardis-hbound-prog.h"
#include "stardis-tbound-prog.h"
#include "stardis-fbound-prog.h"
#include "stardis-ssconnect-prog.h"
#include "stardis-sfconnect-prog.h"
#include "stardis-prog.h"

#include <sdis.h>

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
  (void)frag;
  return interface_props->imposed_temperature;
}

static double
interface_get_flux
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->imposed_flux;
}

static double
interface_get_ref_temperature
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->ref_temperature;
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

static double
interface_get_tcr
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  (void)frag;
  return interface_props->tcr;
}

static double
emissivity_1
  (const struct stardis_interface_fragment* frag,
   void* data)
{
  (void)frag, (void)data;
  return 1;
}
 
static double
intface_prog_get_temp
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  struct stardis_interface_fragment f;
  d3_set(f.P, frag->P);
  d3_set(f.Ng, frag->Ng);
  d3_set(f.uv, frag->uv);
  f.time = frag->time;
  ASSERT(frag->side == SDIS_FRONT || frag->side == SDIS_BACK);
  f.side = (frag->side == SDIS_FRONT) ? FRONT : BACK;
  return interface_props->get_temp(&f, interface_props->prog_data);
}

static double
intface_prog_get_flux
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  struct stardis_interface_fragment f;
  d3_set(f.P, frag->P);
  d3_set(f.Ng, frag->Ng);
  d3_set(f.uv, frag->uv);
  f.time = frag->time;
  ASSERT(frag->side == SDIS_FRONT || frag->side == SDIS_BACK);
  f.side = (frag->side == SDIS_FRONT) ? FRONT : BACK;
  return interface_props->get_flux(&f, interface_props->prog_data);
}

static double
intface_prog_get_hc
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  struct stardis_interface_fragment f;
  d3_set(f.P, frag->P);
  d3_set(f.Ng, frag->Ng);
  d3_set(f.uv, frag->uv);
  f.time = frag->time;
  ASSERT(frag->side == SDIS_FRONT || frag->side == SDIS_BACK);
  f.side = (frag->side == SDIS_FRONT) ? FRONT : BACK;
  return interface_props->get_hc(&f, interface_props->prog_data);
}

static double
intface_prog_get_emissivity
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  struct stardis_interface_fragment f;
  d3_set(f.P, frag->P);
  d3_set(f.Ng, frag->Ng);
  d3_set(f.uv, frag->uv);
  f.time = frag->time;
  ASSERT(frag->side == SDIS_FRONT || frag->side == SDIS_BACK);
  f.side = (frag->side == SDIS_FRONT) ? FRONT : BACK;
  return interface_props->get_emissivity(&f, interface_props->prog_data);
}

static double
intface_prog_get_alpha
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  struct stardis_interface_fragment f;
  d3_set(f.P, frag->P);
  d3_set(f.Ng, frag->Ng);
  d3_set(f.uv, frag->uv);
  f.time = frag->time;
  ASSERT(frag->side == SDIS_FRONT || frag->side == SDIS_BACK);
  f.side = (frag->side == SDIS_FRONT) ? FRONT : BACK;
  return interface_props->get_alpha(&f, interface_props->prog_data);
}

static double
intface_prog_get_ref_temp
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  struct stardis_interface_fragment f;
  d3_set(f.P, frag->P);
  d3_set(f.Ng, frag->Ng);
  d3_set(f.uv, frag->uv);
  f.time = frag->time;
  ASSERT(frag->side == SDIS_FRONT || frag->side == SDIS_BACK);
  f.side = (frag->side == SDIS_FRONT) ? FRONT : BACK;
  return interface_props->get_ref_temp(&f, interface_props->prog_data);
}

static double
intface_prog_get_tcr
  (const struct sdis_interface_fragment* frag,
   struct sdis_data* data)
{
  const struct intface* interface_props = sdis_data_cget(data);
  struct stardis_interface_fragment f;
  d3_set(f.P, frag->P);
  d3_set(f.Ng, frag->Ng);
  d3_set(f.uv, frag->uv);
  f.time = frag->time;
  ASSERT(frag->side == SDIS_FRONT || frag->side == SDIS_BACK);
  f.side = (frag->side == SDIS_FRONT) ? FRONT : BACK;
  return interface_props->get_tcr(&f, interface_props->prog_data);
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
create_intface
  (struct stardis* stardis,
   unsigned itri,
   struct htable_intface* htable_interfaces)
{
  struct int_descs int_descs = INT_DESCS_NULL;
  struct sdis_interface** p_intface;
  struct sdis_interface* intface;
  struct sdis_medium* front_med = NULL;
  struct sdis_medium* back_med = NULL;
  struct sdis_interface_side_shader* fluid_side_shader = NULL;
  struct sdis_data* data = NULL;
  unsigned fd, bd, cd, descr[SG3D_PROP_TYPES_COUNT__];
  int fluid_count = 0, solid_count = 0;
  int solid_fluid_connection_count = 0, solid_solid_connection_count = 0;
  int connection_count = 0, boundary_count = 0, prog_count = 0;
  const struct description* descriptions;
  struct sdis_medium** media;
  res_T res = RES_OK;

  ASSERT(stardis && htable_interfaces);

  /* Get the description IDs for this triangle */
  ERR(sg3d_geometry_get_unique_triangle_properties(stardis->geometry.sg3d,
    itri, descr));

  descriptions = darray_descriptions_cdata_get(&stardis->descriptions);
  media = darray_media_ptr_data_get(&stardis->media);

  /* Create key */
  int_descs.front = fd = descr[SG3D_FRONT];
  int_descs.back = bd = descr[SG3D_BACK];
  int_descs.connect = cd = descr[SG3D_INTFACE];

  /* Search if interface already exists, or create it */
  p_intface = htable_intface_find(htable_interfaces, &int_descs);
  if(p_intface) {
    intface = *p_intface;
  } else {
    /* create new interface and register */
    struct sdis_interface_shader interface_shader = SDIS_INTERFACE_SHADER_NULL;
    struct intface* interface_props = NULL;
    unsigned id;
    int front_defined = (fd != SG3D_UNSPECIFIED_PROPERTY);
    int back_defined = (bd != SG3D_UNSPECIFIED_PROPERTY);
    int connect_defined = (cd != SG3D_UNSPECIFIED_PROPERTY);

    ERR(sdis_data_create(stardis->dev, sizeof(struct intface),
      ALIGNOF(struct intface), NULL, &data));
    interface_props = sdis_data_get(data);
    interface_props->imposed_temperature = -1;
    interface_props->imposed_flux = SDIS_FLUX_NONE;
    interface_props->front_medium_id = UINT_MAX;
    interface_props->back_medium_id = UINT_MAX;
    interface_props->desc_id = UINT_MAX;
    interface_props->get_temp = NULL;
    interface_props->get_flux = NULL;
    interface_props->get_hc = NULL;
    interface_props->get_emissivity = NULL;
    interface_props->get_alpha = NULL;
    interface_props->get_ref_temp = NULL;
    interface_props->get_tcr = NULL;
    interface_props->prog_data = NULL;

    if(front_defined) {
      description_get_medium_id(&descriptions[fd], &id);
      interface_props->front_medium_id = id;
      front_med = media[id];
      switch (descriptions[fd].type) {
      case DESC_MAT_SOLID_PROG:
        prog_count++;
      /* fall through */
      case DESC_MAT_SOLID:
        solid_count++;
        break;
      case DESC_MAT_FLUID_PROG:
        prog_count++;
      /* fall through */
      case DESC_MAT_FLUID:
        fluid_count++;
        fluid_side_shader = &interface_shader.front;
        break;
      default:
        FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
      }
    }
    if(back_defined) {
      description_get_medium_id(&descriptions[bd], &id);
      interface_props->back_medium_id = id;
      back_med = media[id];
      switch (descriptions[bd].type) {
      case DESC_MAT_SOLID_PROG:
        prog_count++;
      /* fall through */
      case DESC_MAT_SOLID:
        solid_count++;
        break;
      case DESC_MAT_FLUID_PROG:
        prog_count++;
      /* fall through */
      case DESC_MAT_FLUID:
        fluid_count++;
        /* Can overwrite fluid_side_shader. However it would imply two
         * fluids and this case lead to an error */
        fluid_side_shader = &interface_shader.back;
        break;
      default:
        FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
      }
    }
    if(fluid_count == 2) {
      res = RES_BAD_ARG;
      goto error;
    }
    if(connect_defined) {
      const struct description* connect = descriptions + cd;
      int for_fluid = 0;
      struct sdis_medium* def_medium = NULL;
      unsigned ext_id;
      if(!DESC_IS_SOLID_FLUID(connect)
          && !DESC_IS_SOLID_SOLID(connect)
          && front_defined == back_defined)
      {
        /* 1 and only 1 defined */
        res = RES_BAD_ARG;
        goto error;
      }
      if(front_defined) {
        def_medium = front_med;
        fluid_side_shader = DESC_IS_FLUID(descriptions+fd)
          ? &interface_shader.front : &interface_shader.back;
      } else {
        ASSERT(back_defined);
        def_medium = back_med;
        fluid_side_shader = DESC_IS_FLUID(descriptions+bd)
          ? &interface_shader.back : &interface_shader.front;
      }
      interface_props->desc_id = cd;
      switch(connect->type) {
      case DESC_BOUND_H_FOR_FLUID:
        if(sdis_medium_get_type(def_medium) != SDIS_FLUID) {
          res = RES_BAD_ARG;
          goto error;
        }
        for_fluid = 1;
        ASSERT(connect->d.h_boundary->imposed_temperature >= 0);
        interface_props->imposed_temperature
          = connect->d.h_boundary->imposed_temperature;
        ASSERT(fluid_side_shader);
        fluid_side_shader->temperature = interface_get_temperature;
        /* fall through */
      case DESC_BOUND_H_FOR_SOLID:
        if(!for_fluid
          &&  sdis_medium_get_type(def_medium) != SDIS_SOLID)
        {
          res = RES_BAD_ARG; goto error;
        }
        ext_id = connect->d.h_boundary->mat_id; /* External material id */
        ASSERT(ext_id < darray_media_ptr_size_get(&stardis->media));
        ASSERT(sdis_medium_get_type(media[ext_id]) ==
          (for_fluid ? SDIS_SOLID : SDIS_FLUID));
        connection_count++;
        boundary_count++;
        if(front_defined) back_med = media[ext_id];
        else front_med = media[ext_id];
        interface_shader.convection_coef_upper_bound = connect->d.h_boundary->hc;
        interface_props->hc = connect->d.h_boundary->hc;
        interface_props->ref_temperature = connect->d.h_boundary->ref_temperature;
        interface_props->emissivity = connect->d.h_boundary->emissivity;
        interface_props->alpha = connect->d.h_boundary->specular_fraction;
        if(connect->d.h_boundary->hc > 0) {
          interface_shader.convection_coef = interface_get_convection_coef;
        }
        ASSERT(fluid_side_shader);
        fluid_side_shader->reference_temperature = interface_get_ref_temperature;
        if(connect->d.h_boundary->emissivity > 0) {
          fluid_side_shader->emissivity = interface_get_emissivity;
          fluid_side_shader->specular_fraction = interface_get_alpha;
        }
        break;
      case DESC_BOUND_H_FOR_FLUID_PROG:
        if(sdis_medium_get_type(def_medium) != SDIS_FLUID) {
          res = RES_BAD_ARG;
          goto error;
        }
        for_fluid = 1;
        ASSERT(fluid_side_shader);
        fluid_side_shader->temperature = intface_prog_get_temp;
        /* fall through */
      case DESC_BOUND_H_FOR_SOLID_PROG:
        if(!for_fluid
          &&  sdis_medium_get_type(def_medium) != SDIS_SOLID)
        {
          res = RES_BAD_ARG; goto error;
        }
        ext_id = connect->d.h_boundary_prog->mat_id; /* External material id */
        ASSERT(ext_id < darray_media_ptr_size_get(&stardis->media));
        ASSERT(sdis_medium_get_type(media[ext_id]) ==
          (for_fluid ? SDIS_SOLID : SDIS_FLUID));
        prog_count++;
        connection_count++;
        boundary_count++;
        if(front_defined) back_med = media[ext_id];
        else front_med = media[ext_id];
        interface_shader.convection_coef_upper_bound
          = h_bound_prog_get_hmax(connect->d.h_boundary_prog);
        ASSERT(interface_shader.convection_coef_upper_bound >= 0);
        interface_shader.convection_coef
          = intface_prog_get_hc;
        ASSERT(fluid_side_shader);
        fluid_side_shader->reference_temperature
          = intface_prog_get_ref_temp;
        fluid_side_shader->emissivity = intface_prog_get_emissivity;
        fluid_side_shader->specular_fraction = intface_prog_get_alpha;
        interface_props->get_temp = connect->d.h_boundary_prog->boundary_temp;
        interface_props->get_hc = connect->d.h_boundary_prog->hc;
        interface_props->get_emissivity = connect->d.h_boundary_prog->emissivity;
        interface_props->get_alpha = connect->d.h_boundary_prog->alpha;
        interface_props->get_ref_temp = connect->d.h_boundary_prog->ref_temp;
        interface_props->prog_data = connect->d.h_boundary_prog->prog_data;
        break;
      case DESC_BOUND_T_FOR_SOLID:
        if(sdis_medium_get_type(def_medium) != SDIS_SOLID) {
          res = RES_BAD_ARG;
          goto error;
        }
        ext_id = connect->d.t_boundary->mat_id; /* External material id */
        ASSERT(ext_id < darray_media_ptr_size_get(&stardis->media));
        ASSERT(sdis_medium_get_type(media[ext_id]) == SDIS_FLUID);
        connection_count++;
        boundary_count++;
        if(front_defined) {
          ASSERT(!back_med);
          back_med = media[ext_id];
        } else {
          ASSERT(!front_med);
          front_med = media[ext_id];
        }
        /* The imposed T is for the 2 sides (there is no contact resistance) */
        interface_shader.front.temperature = interface_get_temperature;
        interface_shader.back.temperature = interface_get_temperature;
        /* Set emissivity to 1 to allow radiative paths comming from
         * a possible external fluid to 'see' the imposed T */
        ASSERT(fluid_side_shader);
        fluid_side_shader->emissivity = interface_get_emissivity;
        interface_props->emissivity = 1;
        ASSERT(connect->d.t_boundary->imposed_temperature >= 0);
        interface_props->imposed_temperature
          = connect->d.t_boundary->imposed_temperature;
        break;
      case DESC_BOUND_T_FOR_SOLID_PROG:
        if(sdis_medium_get_type(def_medium) != SDIS_SOLID) {
          res = RES_BAD_ARG;
          goto error;
        }
        ext_id = connect->d.t_boundary_prog->mat_id; /* External material id */
        ASSERT(ext_id < darray_media_ptr_size_get(&stardis->media));
        ASSERT(sdis_medium_get_type(media[ext_id]) == SDIS_FLUID);
        prog_count++;
        connection_count++;
        boundary_count++;
        if(front_defined) {
          ASSERT(!back_med);
          back_med = media[ext_id];
        } else {
          ASSERT(!front_med);
          front_med = media[ext_id];
        }
        /* The imposed T is for the 2 sides (there is no contact resistance) */
        interface_shader.front.temperature = intface_prog_get_temp;
        interface_shader.back.temperature = intface_prog_get_temp;
        /* Set emissivity to 1 to allow radiative paths comming from
         * a possible external fluid to 'see' the imposed T */
        ASSERT(fluid_side_shader);
        fluid_side_shader->emissivity = interface_get_emissivity;
        interface_props->get_emissivity = emissivity_1;
        interface_props->get_temp = connect->d.t_boundary_prog->temperature;
        interface_props->prog_data = connect->d.t_boundary_prog->prog_data;
        break;
      case DESC_BOUND_F_FOR_SOLID:
        if(sdis_medium_get_type(def_medium) != SDIS_SOLID) {
          res = RES_BAD_ARG;
          goto error;
        }
        connection_count++;
        boundary_count++;
        if(front_defined) {
          back_med = media[connect->d.f_boundary->mat_id];
          interface_shader.front.flux = interface_get_flux;
        } else {
          front_med = media[connect->d.f_boundary->mat_id];
          interface_shader.back.flux = interface_get_flux;
        }
        ASSERT(connect->d.f_boundary->imposed_flux != SDIS_FLUX_NONE);
        interface_props->imposed_flux = connect->d.f_boundary->imposed_flux;
        break;
      case DESC_BOUND_F_FOR_SOLID_PROG:
        if(sdis_medium_get_type(def_medium) != SDIS_SOLID) {
          res = RES_BAD_ARG;
          goto error;
        }
        prog_count++;
        connection_count++;
        boundary_count++;
        if(front_defined) {
          back_med = media[connect->d.f_boundary_prog->mat_id];
          interface_shader.front.flux = intface_prog_get_flux;
        } else {
          front_med = media[connect->d.f_boundary_prog->mat_id];
          interface_shader.back.flux = intface_prog_get_flux;
        }
        interface_props->get_flux = connect->d.f_boundary_prog->flux;
        interface_props->prog_data = connect->d.f_boundary_prog->prog_data;
        break;
      case DESC_SOLID_FLUID_CONNECT:
        /* Both front and back should be defined */
        if(solid_count != 1 || fluid_count != 1) {
          res = RES_BAD_ARG;
          goto error;
        }
        ASSERT(front_defined && back_defined);
        connection_count++;
        solid_fluid_connection_count++;
        interface_shader.convection_coef_upper_bound = connect->d.sf_connect->hc;
        interface_props->hc = connect->d.sf_connect->hc;
        interface_props->ref_temperature = connect->d.sf_connect->ref_temperature;
        interface_props->emissivity = connect->d.sf_connect->emissivity;
        interface_props->alpha = connect->d.sf_connect->specular_fraction;
        if(connect->d.sf_connect->hc > 0) {
          interface_shader.convection_coef = interface_get_convection_coef;
        }
        ASSERT(fluid_side_shader);
        fluid_side_shader->reference_temperature = interface_get_ref_temperature;
        if(connect->d.sf_connect->emissivity > 0) {
          fluid_side_shader->emissivity = interface_get_emissivity;
          fluid_side_shader->specular_fraction = interface_get_alpha;
        }
        break;
      case DESC_SOLID_FLUID_CONNECT_PROG:
        /* Both front and back should be defined */
        if(solid_count != 1 || fluid_count != 1) {
          res = RES_BAD_ARG;
          goto error;
        }
        ASSERT(front_defined && back_defined);
        prog_count++;
        connection_count++;
        solid_fluid_connection_count++;
        interface_shader.convection_coef_upper_bound
          = sf_connect_prog_get_hmax(connect->d.sf_connect_prog);
        ASSERT(interface_shader.convection_coef_upper_bound >= 0);
        interface_props->get_ref_temp = connect->d.sf_connect_prog->ref_temp;
        interface_props->get_hc = connect->d.sf_connect_prog->hc;
        interface_props->get_emissivity
          = connect->d.sf_connect_prog->emissivity;
        interface_props->get_alpha = connect->d.sf_connect_prog->alpha;
        interface_shader.convection_coef = intface_prog_get_hc;
        ASSERT(fluid_side_shader);
        fluid_side_shader->emissivity = intface_prog_get_emissivity;
        fluid_side_shader->specular_fraction = intface_prog_get_alpha;
        fluid_side_shader->reference_temperature = intface_prog_get_ref_temp;
        interface_props->prog_data = connect->d.sf_connect_prog->prog_data;
        break;
      case DESC_SOLID_SOLID_CONNECT:
        /* Both front and back should be defined */
        if(solid_count != 2 || fluid_count != 0) {
          res = RES_BAD_ARG;
          goto error;
        }
        ASSERT(front_defined && back_defined);
        connection_count++;
        solid_solid_connection_count++;
        interface_props->tcr = connect->d.ss_connect->tcr;
        if(connect->d.ss_connect->tcr > 0) {
          interface_shader.thermal_contact_resistance = interface_get_tcr;
        }
        break;
      case DESC_SOLID_SOLID_CONNECT_PROG:
        /* Both front and back should be defined */
        if(solid_count != 2 || fluid_count != 0) {
          res = RES_BAD_ARG;
          goto error;
        }
        ASSERT(front_defined && back_defined);
        prog_count++;
        connection_count++;
        solid_solid_connection_count++;
        interface_props->get_tcr = connect->d.ss_connect_prog->tcr;
        interface_shader.thermal_contact_resistance = intface_prog_get_tcr;
        interface_props->prog_data = connect->d.ss_connect_prog->prog_data;
        break;
      default:
        FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
      }
    }

    if((fluid_count + solid_count + connection_count < 2)
      || (boundary_count && (fluid_count + solid_count != 1)))
    {
      res = RES_BAD_ARG;
      goto error;
    }

    ERR(sdis_interface_create(stardis->dev, front_med, back_med,
      &interface_shader, data, &intface));
    SDIS(data_ref_put(data)); data = NULL;
    ERR(darray_interface_ptrs_push_back(&stardis->geometry.interfaces, &intface));
    ERR(htable_intface_set(htable_interfaces, &int_descs, &intface));
  }
  ERR(darray_interface_ptrs_push_back(&stardis->geometry.interf_bytrg, &intface));

end:
  if(data) SDIS(data_ref_put(data));
  return res;
error:
  goto end;
}

