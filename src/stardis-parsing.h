/* Copyright (C) 2018-2022 |Meso|Star> (contact@meso-star.com)
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

#ifndef STARDIS_PARSING_H
#define STARDIS_PARSING_H

#include <sdis.h>

#include <rsys/rsys.h>
#include <rsys/dynamic_array_str.h>

#include <star/sstl.h>

struct logger;
struct stardis;

/* Utility macros */
#define CHK_TOK(x, Name) if((tk = (x)) == NULL) {\
   logger_print(stardis->logger, LOG_ERROR,\
     "Invalid data (missing token '" Name "')\n");\
   res = RES_BAD_ARG;\
   goto error;\
  } (void)0

/* Same ctx used for both media and interface add (some unused parts) */
struct add_geom_ctx {
  struct sstl_desc stl_desc;
  unsigned properties[3];
  void* custom;
};

/* Possible callbacks for sg3d_geometry_add calls
 * when void* context is a struct add_geom_ctx */
void
add_geom_ctx_indices
  (const unsigned itri,
   unsigned ids[3],
   void* context);

void
add_geom_ctx_position
  (const unsigned ivert,
   double pos[3],
   void* context);

res_T
get_dummy_solid_id
  (struct stardis* stardis,
   unsigned* id);

res_T
get_dummy_fluid_id
  (struct stardis* stardis,
   unsigned* id);

res_T
read_model
  (const struct darray_str* model_files,
   struct stardis* stardis);

#endif /*ARGS_H*/
