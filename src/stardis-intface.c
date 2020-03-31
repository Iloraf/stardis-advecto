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

#include "stardis-intface.h"
#include "stardis-app.h"
#include "stardis-compute.h"
#include "stardis-output.h"
#include "stardis-solid.h"

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
  unsigned fd, bd, cd, properties[3];
  int fluid_count = 0, solid_count = 0;
  int solid_fluid_connection_count = 0, connection_count = 0, boundary_count = 0;
  const struct description* descriptions;
  struct sdis_medium** media;
  res_T res = RES_OK;

  ASSERT(stardis && htable_interfaces);
  
  ERR(sg3d_geometry_get_unique_triangle_properties(stardis->geometry.sg3d,
    itri, properties));

  descriptions = darray_descriptions_cdata_get(&stardis->descriptions);
  media = darray_media_ptr_data_get(&stardis->media);

  /* Create key */
  int_descs.front = fd = properties[SG3D_FRONT];
  int_descs.back = bd = properties[SG3D_BACK];
  int_descs.connect = cd = properties[SG3D_INTFACE];

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
    interface_props->front_boundary_id = UINT_MAX;
    interface_props->back_boundary_id = UINT_MAX;

    if(front_defined) {
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
      default:
        FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
      }
    }
    if(back_defined) {
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
      default:
        FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
      }
    }
    if(connect_defined) {
      const struct description* connect = descriptions + cd;
      int type_checked = 0;
      struct sdis_medium* def_medium = NULL;
      unsigned ext_id;
      if(connect->type == DESC_SOLID_FLUID_CONNECT) {
        ASSERT(front_defined && back_defined);
        fluid_side_shader = (descriptions[fd].type == DESC_MAT_SOLID)
          ? &interface_shader.back : &interface_shader.front;
        if(solid_count != 1 || fluid_count != 1) goto error;
      } else {
        if(front_defined == back_defined) goto error;
        def_medium = front_defined ? front_med : back_med;
        if(front_defined) {
          ASSERT(descriptions[fd].type == DESC_MAT_SOLID
            || descriptions[fd].type == DESC_MAT_FLUID);
          fluid_side_shader = (descriptions[fd].type == DESC_MAT_SOLID)
            ? &interface_shader.back : &interface_shader.front;
          interface_props->front_boundary_id = cd;
        } else {
          ASSERT(back_defined);
          ASSERT(descriptions[bd].type == DESC_MAT_SOLID
            || descriptions[bd].type == DESC_MAT_FLUID);
          fluid_side_shader = (descriptions[bd].type == DESC_MAT_SOLID)
            ? &interface_shader.back : &interface_shader.front;
          interface_props->back_boundary_id = cd;
        }
      }
      switch(connect->type) {
      case DESC_BOUND_H_FOR_FLUID:
        if(sdis_medium_get_type(def_medium) != SDIS_FLUID) goto error;
        type_checked = 1;
        ASSERT(connect->d.h_boundary.imposed_temperature >= 0);
        interface_props->imposed_temperature
          = connect->d.h_boundary.imposed_temperature;
        fluid_side_shader->temperature = interface_get_temperature;
        /* fall through */
      case DESC_BOUND_H_FOR_SOLID:
        if(!type_checked
          &&  sdis_medium_get_type(def_medium) != SDIS_SOLID) goto error;
        ext_id = connect->d.h_boundary.mat_id; /* External material id */
        ASSERT(ext_id < darray_media_ptr_size_get(&stardis->media));
        ASSERT(sdis_medium_get_type(media[ext_id]) ==
          (connect->type == DESC_BOUND_H_FOR_SOLID ? SDIS_FLUID : SDIS_SOLID));
        connection_count++;
        boundary_count++;
        if(front_defined) back_med = media[ext_id];
        else front_med = media[ext_id];
        interface_shader.convection_coef_upper_bound = connect->d.h_boundary.hc;
        interface_props->hc = connect->d.h_boundary.hc;
        interface_props->emissivity = connect->d.h_boundary.emissivity;
        interface_props->alpha = connect->d.h_boundary.specular_fraction;
        if(connect->d.h_boundary.hc > 0) {
          interface_shader.convection_coef = interface_get_convection_coef;
        }
        if(connect->d.h_boundary.emissivity > 0) {
          ASSERT(fluid_side_shader);
          fluid_side_shader->emissivity = interface_get_emissivity;
          fluid_side_shader->specular_fraction = interface_get_alpha;
        }
        break;
      case DESC_BOUND_T_FOR_FLUID:
        if(sdis_medium_get_type(def_medium) != SDIS_FLUID) goto error;
        type_checked = 1;
        interface_shader.convection_coef_upper_bound = connect->d.t_boundary.hc;
        interface_props->hc = connect->d.t_boundary.hc;
        if(connect->d.t_boundary.hc > 0) {
          ASSERT(connect->type == DESC_BOUND_T_FOR_FLUID);
          interface_shader.convection_coef = interface_get_convection_coef;
        }
        interface_props->emissivity = connect->d.t_boundary.emissivity;
        interface_props->alpha = connect->d.t_boundary.specular_fraction;
        if(connect->d.t_boundary.emissivity > 0) {
          ASSERT(fluid_side_shader);
          fluid_side_shader->emissivity = interface_get_emissivity;
          fluid_side_shader->specular_fraction = interface_get_alpha;
        }
        /* fall through */
      case DESC_BOUND_T_FOR_SOLID:
        if(!type_checked
          &&  sdis_medium_get_type(def_medium) != SDIS_SOLID) goto error;
        ext_id = connect->d.t_boundary.mat_id; /* External material id */
        ASSERT(ext_id < darray_media_ptr_size_get(&stardis->media));
        ASSERT(sdis_medium_get_type(media[ext_id])
          == (connect->type == DESC_BOUND_T_FOR_FLUID ? SDIS_SOLID: SDIS_FLUID));
        connection_count++;
        boundary_count++;
        if(front_defined) {
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
        ASSERT(connect->d.t_boundary.imposed_temperature >= 0);
        interface_props->imposed_temperature
          = connect->d.t_boundary.imposed_temperature;
        break;
      case DESC_BOUND_F_FOR_SOLID:
        if(sdis_medium_get_type(def_medium) != SDIS_SOLID) goto error;
        connection_count++;
        boundary_count++;
        if(front_defined) {
          back_med = media[connect->d.f_boundary.mat_id];
          interface_shader.front.flux = interface_get_flux;
        } else {
          front_med = media[connect->d.f_boundary.mat_id];
          interface_shader.back.flux = interface_get_flux;
        }
        ASSERT(connect->d.f_boundary.imposed_flux != SDIS_FLUX_NONE);
        interface_props->imposed_flux = connect->d.f_boundary.imposed_flux;
        break;
      case DESC_SOLID_FLUID_CONNECT:
        /* Both front and back should be defined;
         * if not will raise an error below */
        connection_count++;
        solid_fluid_connection_count++;
        interface_shader.convection_coef_upper_bound = connect->d.sf_connect.hc;
        interface_props->hc = connect->d.sf_connect.hc;
        interface_props->emissivity = connect->d.sf_connect.emissivity;
        interface_props->alpha = connect->d.sf_connect.specular_fraction;
        if(connect->d.sf_connect.hc > 0) {
          interface_shader.convection_coef = interface_get_convection_coef;
        }
        if(connect->d.sf_connect.emissivity > 0) {
          ASSERT(fluid_side_shader);
          fluid_side_shader->emissivity = interface_get_emissivity;
          fluid_side_shader->specular_fraction = interface_get_alpha;
        }
        break;
      default:
        FATAL("error:" STR(__FILE__) ":" STR(__LINE__)": Invalid type.\n");
      }
    }

    if((fluid_count == 2)
      || (fluid_count + solid_count + connection_count < 2)
      || (boundary_count ?
      (fluid_count + solid_count != 1) : (fluid_count + solid_count != 2))
      || (solid_fluid_connection_count && (fluid_count != 1 || solid_count != 1)))
      goto error;

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

