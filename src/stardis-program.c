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

#include <rsys/rsys.h>
#include <rsys/library.h>

#include "stardis-program.h"

res_T
init_program
  (struct mem_allocator* allocator,
   struct program** dst)
{
  res_T res = RES_OK;
  int str_initialized = 0;
  ASSERT(allocator && dst && *dst == NULL);
  *dst = MEM_CALLOC(allocator, 1, sizeof(**dst));
  if(! *dst) {
    res = RES_MEM_ERR;
    goto error;
  }
  str_init(allocator, &(*dst)->name);
  str_init(allocator, &(*dst)->lib_path);
  str_init(allocator, &(*dst)->args);
  str_initialized = 1;
end:
  return res;
error:
  if(str_initialized) {
    str_release(&(*dst)->name);
    str_release(&(*dst)->lib_path);
    str_release(&(*dst)->args);
  }
  if(*dst) MEM_RM(allocator, *dst);
  goto end;
}

void
release_program
  (struct program* program,
   struct mem_allocator* allocator)
{
  ASSERT(program && allocator);
  str_release(&program->name);
  str_release(&program->lib_path);
  str_release(&program->args);
  if(program->prog_data) {
    ASSERT(program->release);
    program->release(program->prog_data);
  }
  library_close(program->lib_handle);
  MEM_RM(allocator, program);
}

res_T
str_print_program
  (struct str* str,
   const struct program* program)
{
  res_T res = RES_OK;
  ERR(str_append_printf(str, "Library %s", str_cget(&program->name)));
 
end:
  return res;
error:
  goto end;
}

