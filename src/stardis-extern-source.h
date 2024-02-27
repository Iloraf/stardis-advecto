/* Copyright (C) 2018-2023 |Méso|Star> (contact@meso-star.com)
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

#ifndef SDIS_EXTERN_SOURCE_H
#define SDIS_EXTERN_SOURCE_H

#include <rsys/rsys.h>

/* Forward declaration */
struct stardis;

enum source_type {
  EXTERN_SOURCE_SPHERE,
  EXTERN_SOURCE_NONE__
};

struct spherical_source {
  double position[3];
  double radius;
  double power; /* [W] */
};
#define SPHERICAL_SOURCE_NULL__ {{0,0,0}, -1, 0}
static const struct spherical_source SPHERICAL_SOURCE_NULL =
  SPHERICAL_SOURCE_NULL__;

/* Interface of an external source */
struct extern_source {
  enum source_type type;
  union {
    struct spherical_source sphere;
  } data;
  struct sdis_source* sdis_src;
};
#define EXTERN_SOURCE_NULL__ \
  {EXTERN_SOURCE_NONE__, {SPHERICAL_SOURCE_NULL__}, NULL}
static const struct extern_source EXTERN_SOURCE_NULL = EXTERN_SOURCE_NULL__;

static INLINE void
extern_source_init(struct extern_source* src)
{
  ASSERT(src);
  *src = EXTERN_SOURCE_NULL;
}

extern LOCAL_SYM void
extern_source_release
  (struct extern_source* src);

extern LOCAL_SYM res_T
extern_source_create_solver_source
  (struct extern_source* src,
   struct stardis* stardis);

#endif /* SDIS_EXTERN_SOURCE_H */
