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

#define _GNU_SOURCE 1
#include "stardis-parsing.h"
#include "stardis-app.h"
#include "stardis-default.h"
#include "stardis-version.h"

#include <rsys/cstr.h>
#include <rsys/double3.h>
#include <sdis_version.h>
#include <rsys/logger.h>

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

#ifdef COMPILER_GCC
static char*
_strupr
  (char* s)
{
  char* ptr;
  for(ptr = s; *ptr; ++ptr) {
    int tmp = toupper(*ptr);
    ASSERT(tmp == (char)tmp);
    *ptr = (char)tmp;
  }
  return s;
}
#endif

static char**
split_line
  (char* a_str,
   const char a_delim)
{
  char** result = 0;
  size_t chunks_count;
  char* tmp = a_str;
  char delim[2];
  char* tok_ctx = NULL;

  ASSERT(a_str);

  delim[0] = a_delim;
  delim[1] = 0;

  /* if a_str starts with initial useless delimiters remove them */
  while(*a_str == a_delim) a_str++;

  /* if a_str ends with final useless delimiters remove them */
  tmp = a_str + strlen(a_str) - 1;
  while(*tmp == a_delim && tmp >= a_str) { *tmp = '\0'; tmp--; }

  if(tmp >= a_str) chunks_count = 1;
  else return NULL;

  tmp = a_str;
  while(*tmp) {
    int delim_found = 0;
    while(*tmp == a_delim) { delim_found = 1; tmp++; }
    if(delim_found) chunks_count++;
    tmp++;
  }

  /* Add space for terminating null string so caller
     knows where the list of returned strings ends. */
  result = malloc(sizeof(char*) * (1 + chunks_count));
  if(result) {
    size_t idx = 0;
    char* token = strtok_r(a_str, delim, &tok_ctx);

    while(token) {
      ASSERT(idx <= chunks_count);
#ifdef COMPILER_CL
      *(result + idx++) = _strdup(token);
#else
      *(result + idx++) = strdup(token);
#endif
      token = strtok_r(NULL, delim, &tok_ctx);
    }
    ASSERT(idx == chunks_count);
    *(result + idx) = 0;
  }
  return result;
}

void
print_version
  (FILE* stream)
{
  ASSERT(stream);
  fprintf(stream,
    "Stardis version %i.%i.%i built on stardis solver version %i.%i.%i\n",
    STARDIS_APP_VERSION_MAJOR, STARDIS_APP_VERSION_MINOR, STARDIS_APP_VERSION_PATCH,
    Stardis_VERSION_MAJOR, Stardis_VERSION_MINOR, Stardis_VERSION_PATCH);
}

void
add_geom_ctx_indices
  (const unsigned itri,
   unsigned ids[3],
   void* context)
{
  const struct add_geom_ctx* ctx = context;
  const unsigned* trg;
  int i;
  ASSERT(ids && ctx);
  ASSERT(itri < ctx->stl_desc.triangles_count);
  trg = ctx->stl_desc.indices + 3 * itri;
  for(i = 0; i < 3; i++) ids[i] = trg[i];
}

void
add_geom_ctx_properties
  (const unsigned itri,
   unsigned prop[3],
   void* context)
{
  const struct add_geom_ctx* ctx = context;
  int i;
  ASSERT(prop && ctx); (void)itri;
  ASSERT(itri < ctx->stl_desc.triangles_count);
  /* Same media for the whole add_geometry set of triangles */
  for(i = 0; i < SG3D_PROP_TYPES_COUNT__; i++) prop[i] = ctx->properties[i];
}

void
add_geom_ctx_position
  (const unsigned ivert,
   double pos[3],
   void* context)
{
  const struct add_geom_ctx* ctx = context;
  const float* v;
  ASSERT(pos && ctx);
  ASSERT(ivert < ctx->stl_desc.vertices_count);
  v = ctx->stl_desc.vertices + 3 * ivert;
  d3_set_f3(pos, v);
}

static res_T
read_sides_and_files
  (struct stardis* stardis,
   const int side_is_intface, /* if 1, don't read side */
   const unsigned id,
   char** tok_ctx)
{
  char* tk = NULL;
  int file_count = 0;
  struct sstl* sstl = NULL;
  struct add_geom_ctx add_geom_ctx;
  struct sg3d_geometry_add_callbacks callbacks = SG3D_ADD_CALLBACKS_NULL__;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  callbacks.get_indices = add_geom_ctx_indices;
  callbacks.get_properties = add_geom_ctx_properties;
  callbacks.get_position = add_geom_ctx_position;

  /* At least one side+name, no side without name */
  ERR(sstl_create(stardis->logger, stardis->allocator, 0, &sstl));
  for(;;) {
    struct str name;
    if(side_is_intface) {
      add_geom_ctx.properties[SG3D_FRONT] = SG3D_UNSPECIFIED_PROPERTY;
      add_geom_ctx.properties[SG3D_BACK] = SG3D_UNSPECIFIED_PROPERTY;
      add_geom_ctx.properties[SG3D_INTFACE] = id;
    } else {
      tk = strtok_r(NULL, " \t", tok_ctx);
      if(!tk) {
        if(file_count == 0) {
          /* At least 1 side */
          logger_print(stardis->logger, LOG_ERROR,
            "Invalid data (missing token 'side')\n");
          res = RES_BAD_ARG;
          goto error;
        }
        else break;
      }
      _strupr(tk);
      add_geom_ctx.properties[SG3D_INTFACE] = SG3D_UNSPECIFIED_PROPERTY;
      if(0 == strcmp(tk, "FRONT")) {
        add_geom_ctx.properties[SG3D_FRONT] = id;
        add_geom_ctx.properties[SG3D_BACK] = SG3D_UNSPECIFIED_PROPERTY;
      }
      else if(0 == strcmp(tk, "BACK")) {
        add_geom_ctx.properties[SG3D_FRONT] = SG3D_UNSPECIFIED_PROPERTY;
        add_geom_ctx.properties[SG3D_BACK] = id;
      }
      else if(0 == strcmp(tk, "BOTH")) {
        add_geom_ctx.properties[SG3D_FRONT] = id;
        add_geom_ctx.properties[SG3D_BACK] = id;
      }
      else {
        logger_print(stardis->logger, LOG_ERROR,
          "Invalid side specifier: %s\n", tk);
        res = RES_BAD_ARG;
        goto error;
      }
    }
    tk = strtok_r(NULL, " \t", tok_ctx);
    if(!tk) {
      if(!side_is_intface /* Has read a side */
        || !file_count) /* Need at least 1 file */
      {
        logger_print(stardis->logger, LOG_ERROR,
          "Invalid data (missing token 'file name')\n");
        res = RES_BAD_ARG;
        goto error;
      }
      else break;
    }
    file_count++;
    res = sstl_load(sstl, tk);
    if(res != RES_OK) {
      logger_print(stardis->logger, LOG_ERROR,
        "Cannot read STL file: '%s'\n", tk);
      goto error;
    }
    ERR(sstl_get_desc(sstl, &add_geom_ctx.stl_desc));
    ASSERT(add_geom_ctx.stl_desc.vertices_count <= UINT_MAX
      && add_geom_ctx.stl_desc.triangles_count <= UINT_MAX);
    /* Keep file names for error reports */
    str_init(stardis->allocator, &name);
    ERR(str_set(&name, tk));
    str_release(&name);

    res = sg3d_geometry_add(
      stardis->geometry.sg3d,
      (unsigned)add_geom_ctx.stl_desc.vertices_count,
      (unsigned)add_geom_ctx.stl_desc.triangles_count,
      &callbacks,
      &add_geom_ctx);

    if(res != RES_OK) {
      logger_print(stardis->logger, LOG_ERROR,
        "Cannot add file content: '%s'\n", tk);
      goto error;
    }
  }

end:
  if(sstl) SSTL(ref_put(sstl));
  return res;
error:
  goto end;
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

res_T
init_args
  (struct logger* logger,
   struct mem_allocator* allocator,
   struct args** out_args)
{
  res_T res = RES_OK;
  struct args* args = NULL;
  ASSERT(logger && allocator && out_args);

  args = calloc(sizeof(struct args), 1);
  if(!args) {
    res = RES_MEM_ERR;
    goto error;
  }

  args->logger = logger;
  args->allocator = allocator;
  darray_str_init(allocator, &args->model_files);
  /* Set default values */
  args->samples = STARDIS_DEFAULT_SAMPLES_COUNT;
  args->nthreads = DEFAULT_NTHREADS;
  args->probe[3] = STARDIS_DEFAULT_COMPUTE_TIME;
  args->scale_factor = STARDIS_DEFAULT_SCALE_FACTOR;
  args->ambient_temp = STARDIS_DEFAULT_AMBIENT_TEMP;
  args->ref_temp = STARDIS_DEFAULT_REFERENCE_TEMP;
  args->verbose = STARDIS_DEFAULT_VERBOSE_LEVEL;

end:
  *out_args = args;
  return res;
error:
  if(args) release_args(args);
  args = NULL;
  goto end;
}

void
release_args(struct args* args)
{
  ASSERT(args);
  darray_str_release(&args->model_files);
  free(args);
}

char
mode_option
  (const int m)
{
  int found = 0;
  char res = '?';
  if(m & MODE_DUMP_VTK) { found++; res = 'd'; }
  if(m & MODE_DUMP_PATHS) { found++; res = 'D'; }
  if(m & MODE_EXTENDED_RESULTS) { found++; res = 'e'; }
  if(m & MODE_FLUX_BOUNDARY_COMPUTE) { found++; res = 'F'; }
  if(m & MODE_GREEN) { found++; res = 'g'; }
  if(m & MODE_BIN_GREEN) { found++; res = 'G'; }
  if(m & MODE_DUMP_HELP) { found++; res = 'h'; }
  if(m & MODE_MEDIUM_COMPUTE) { found++; res = 'm'; }
  if(m & MODE_PROBE_COMPUTE) { found++; res = 'p'; }
  if(m & MODE_PROBE_COMPUTE_ON_INTERFACE) { found++; res = 'P'; }
  if(m & MODE_IR_COMPUTE) { found++; res = 'R'; }
  if(m & MODE_BOUNDARY_COMPUTE) { found++; res = 's'; }
  if(m & MODE_MAP_COMPUTE) { found++; res = 'S'; }
  if(m & MODE_VERBOSITY) { found++; res = 'V'; }
  if(m & MODE_DUMP_VERSION) { found++; res = 'v'; }
  ASSERT(found == 1);
  return res;
}

void
print_multiple_modes
  (char* buf,
   const size_t sz,
   const int modes,
   const int dont) /* Modes in dont are not printed */
{
  int b = 0, fst = 1;
  int m = UNDEF_MODE;
  size_t left = sz;
  ASSERT(buf);
  do {
    m = BIT(b++);
    if(m & dont) continue;
    if(m & modes) {
      size_t n =
        (size_t)snprintf(buf, left, (fst ? "-%c" : ", -%c"), mode_option(m));
      if(n >= left) FATAL("Buffer is too small.");
      left -= n;
      buf += n;
      fst = 0;
    }
  } while(m < modes);
}

void
short_help
  (FILE* stream,
   const char* prog)
{
  const char* name;
  ASSERT(stream && prog);

#ifdef COMPILER_GCC
  name = strrchr(prog, '/');
#else
  name = strrchr(prog, '\\');
#endif

  name = name ? name + 1 : prog;
  fprintf(stream,
    "Usage: %s [OPTIONS]\n"
    "\nSolve coupled thermal systems under the linear assumption.\n"
    "Refer to stardis(1) man page for more information.\n\n",
    name);
  print_version(stream);

  fprintf(stream, "\nMandatory arguments\n");
  fprintf(stream, "-------------------\n");

  fprintf(stream, "\n  -M <FILE>\n");
  fprintf(stream, "     Read a text file that contains (partial) description of the model.\n");

  fprintf(stream, "\nExclusive arguments\n");
  fprintf(stream, "-------------------\n");

  fprintf(stream, "\n  -F STL_FILE[,TIME]\n");
  fprintf(stream, "     Compute the mean flux on a given 2D region at a given time.\n");

  fprintf(stream, "\n  -m MEDIUM_NAME[,TIME]\n");
  fprintf(stream, "     Compute the mean temperature in a given medium at a given time.\n");

  fprintf(stream, "\n  -p X,Y,Z[,TIME]\n");
  fprintf(stream, "     Compute the temperature at the given probe.\n");

  fprintf(stream, "\n  -P X,Y,Z[,TIME]\n");
  fprintf(stream, "     Compute the temperature at the given probe on an interface.\n");

  fprintf(stream, "\n  -R [RENDERING_OPTIONS]\n");
  fprintf(stream, "     Compute an infra-red image of the model.\n");

  fprintf(stream, "\n  -s STL_FILE[,TIME]\n");
  fprintf(stream, "     Compute the mean temperature on a given 2D region.\n");

  fprintf(stream, "\n  -S STL_FILE[,TIME]\n");
  fprintf(stream, "     Compute the by-triangle mean temperature on a given 2D region.\n");

  fprintf(stream, "\nOptionnal arguments\n");
  fprintf(stream, "-------------------\n");

  fprintf(stream, "\n  -a AMBIENT_TEMP\n");
  fprintf(stream, "     Set the ambient radiative temperature .\n");

  fprintf(stream, "\n  -d\n");
  fprintf(stream, "     Dump the geometry to stdout in VTK format along with various properties.\n");

  fprintf(stream, "\n  -D TYPE,FILE_NAMES_PREFIX\n");
  fprintf(stream, "     Write thermal paths of the given TYPE in VTK format.\n");

  fprintf(stream, "\n  -e\n");
  fprintf(stream, "     Use extended format to output Monte-Carlo results.\n");

  fprintf(stream, "\n  -f SCALE_FACTOR\n");
  fprintf(stream, "     Rescale the geometry before sending it to the solver.\n");

  fprintf(stream, "\n  -g\n");
  fprintf(stream, "     Change the computation to produce the green function.\n");

  fprintf(stream, "\n  -G BIN_FILE_NAME\n");
  fprintf(stream, "     Change the computation to produce the green function.\n");

  fprintf(stream, "\n  -h\n");
  fprintf(stream, "     Print this help and exit.\n");

  fprintf(stream, "\n  -n SAMPLE_COUNT\n");
  fprintf(stream, "     Set the number of Monte-Carlo samples.\n");

  fprintf(stream, "\n  -r REFERENCE_TEMP\n");
  fprintf(stream, "     Set the temperature used for the linearization of the radiative transfer.\n");

  fprintf(stream, "\n  -t NUM_OF_THREADS\n");
  fprintf(stream, "     Hint on the number of threads.\n");

  fprintf(stream, "\n  -v\n");
  fprintf(stream, "     Print version information and exit.\n");

  fprintf(stream, "\n  -V LEVEL\n");
  fprintf(stream, "     Set the verbosity level.\n");
}

res_T
parse_args
  (const int argc,
   char** argv,
   struct args* args)
{
  int opt = 0, n_used = 0;
  size_t len = 0;
  const char option_list[] = "a:dD:ef:F:gG:hm:M:n:p:P:r:R:s:S:t:vV:";
  char buf[128];
  res_T res = RES_OK;

  ASSERT(argv && args);

  opterr = 0; /* No default error messages */
  while((opt = getopt(argc, argv, option_list)) != -1) {
    switch (opt) {

    case '?': /* Unreconised option */
    {
      char* ptr = strchr(option_list, optopt);
      if(ptr && ptr[1] == ':') {
        res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Missing argument for option -%c\n",
          optopt);
      } else {
        logger_print(args->logger, LOG_ERROR, "Invalid option -%c\n", optopt);
      }
      goto error;
    }

    case 'a':
      res = cstr_to_double(optarg, &args->ambient_temp);
      if(res != RES_OK
        || args->ambient_temp < 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      break;

    case 'd':
      if(args->mode & USE_STDOUT_MODES) {
        res = RES_BAD_ARG;
        print_multiple_modes(buf, sizeof(buf), USE_STDOUT_MODES, MODE_DUMP_VTK);
        logger_print(args->logger, LOG_ERROR,
          "Option -%c cannot be used in conjunction with other dump options (%s).\n",
          (char)opt, buf);
        goto error;
      }
      args->mode |= MODE_DUMP_VTK;
      break;

    case 'e':
      args->mode |= MODE_EXTENDED_RESULTS;
      break;

    case 'D': {
      char* ptr = strrchr(optarg, ',');
      if(!ptr || ptr != strchr(optarg, ','))
        res = RES_BAD_ARG; /* Single ',' expected */
      else {
        args->paths_filename = ptr + 1;
        *ptr = '\0';
      }
      if(res == RES_OK) {
        if(0 == strcmp(optarg, "all")) {
          args->dump_paths = DUMP_ALL;
        }
        else if(0 == strcmp(optarg, "error")) {
          args->dump_paths = DUMP_ERROR;
        }
        else if(0 == strcmp(optarg, "success")) {
          args->dump_paths = DUMP_SUCCESS;
        }
      }
      if(res != RES_OK) {
        res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      args->mode |= MODE_DUMP_PATHS;
      break;
    }

    case 'f':
      res = cstr_to_double(optarg, &args->scale_factor);
      if(res != RES_OK
        || args->scale_factor <= 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      break;

    /*case 'F': see 's' */

    case 'g':
      args->mode |= MODE_GREEN;
      break;

    case 'G':
      args->mode |= MODE_BIN_GREEN;
      args->bin_green_filename = optarg;
      break;

    case 'h':
      if(args->mode & USE_STDOUT_MODES) {
        res = RES_BAD_ARG;
        print_multiple_modes(buf, sizeof(buf), USE_STDOUT_MODES, MODE_DUMP_HELP);
        logger_print(args->logger, LOG_ERROR,
          "Option -%c cannot be used in conjunction with other dump options (%s).\n",
          (char)opt, buf);
        goto error;
      }
      args->mode |= MODE_DUMP_HELP;
      break;

    case 'm': {
      char* ptr;
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= MODE_MEDIUM_COMPUTE;
      ptr = strrchr(optarg, ',');
      if(ptr) {
        if(ptr != strchr(optarg, ','))
          res = RES_BAD_ARG; /* Single ',' allowed */
        else {
          res = cstr_to_double(ptr + 1, args->probe + 3);
          if(res == RES_OK && args->probe[3] < 0) res = RES_BAD_ARG;
        }
        *ptr = '\0';
      }
      if(res != RES_OK) {
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      args->medium_name = optarg;
      break;
    }

    case 'M': {
      struct str name;
      str_init(args->allocator, &name);
      ERR(str_set(&name, optarg));
      ERR(darray_str_push_back(&args->model_files, &name));
      str_release(&name);
      break;
    }
    case 'n': {
      unsigned long n;
      res = cstr_to_ulong(optarg, &n);
      if(res != RES_OK
        || n == 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      args->samples = n;
      n_used = 1;
      break;
    }

    case 'p':
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= MODE_PROBE_COMPUTE;
      res = cstr_to_list_double(optarg, ',', args->probe, &len, 4);
      if(res != RES_OK
        || len < 3
        || (len == 4 && args->probe[3] < 0)
        || len > 4)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      break;

    case 'P':
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= MODE_PROBE_COMPUTE_ON_INTERFACE;
      res = cstr_to_list_double(optarg, ',', args->probe, &len, 4);
      if(res != RES_OK
        || len < 3
        || (len == 4 && args->probe[3] < 0)
        || len > 4)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      break;

    case 'r':
      res = cstr_to_double(optarg, &args->ref_temp);
      if(res != RES_OK
        || args->ref_temp < 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      break;

    case 'R':
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= MODE_IR_COMPUTE;
      args->camera = optarg;
      break;

    case 's':
    case 'S':
    case 'F': {
      char *ptr;
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      switch (opt) {
      case 's':
        args->mode |= MODE_BOUNDARY_COMPUTE;
        break;
      case 'S':
        args->mode |= MODE_MAP_COMPUTE;
        break;
      case 'F':
        args->mode |= MODE_FLUX_BOUNDARY_COMPUTE;
        break;
      }
      ptr = strrchr(optarg, ',');
      if(ptr) {
        if(ptr != strchr(optarg, ','))
          res = RES_BAD_ARG; /* Single ',' allowed */
        else {
          res = cstr_to_double(ptr + 1, args->probe + 3);
          if(res == RES_OK && args->probe[3] < 0)
            res = RES_BAD_ARG;
        }
        *ptr = '\0';
      }
      args->solve_filename = optarg;
      if(res != RES_OK) {
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      break;
    }

    case 't':
      res = cstr_to_uint(optarg, &args->nthreads);
      if(res != RES_OK
        || args->nthreads <= 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      break;

    case 'v':
      if(args->mode & USE_STDOUT_MODES) {
        res = RES_BAD_ARG;
        print_multiple_modes(buf, sizeof(buf), USE_STDOUT_MODES, MODE_DUMP_VERSION);
        logger_print(args->logger, LOG_ERROR,
          "Option -%c cannot be used in conjunction with other dump options (%s).\n",
          (char)opt, buf);
        goto error;
      }
      args->mode |= MODE_DUMP_VERSION;
      break;

    case 'V':
      res = cstr_to_int(optarg, &args->verbose);
      if(res != RES_OK
        || args->verbose < 0
        || args->verbose > 3)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      break;
    }
  }

  if(argc > optind) {
    int i;
    for(i = optind; i < argc; i++) {
      logger_print(args->logger, LOG_ERROR, "Unexpected argument: %s.\n", argv[i]);
    }
    res = RES_BAD_ARG;
    goto error;
  }

  if(!darray_str_size_get(&args->model_files)
    && !(args->mode & SHORT_EXIT_MODES)) {
    logger_print(args->logger, LOG_ERROR,
      "Missing mandatory argument: -M <model_file_name>\n");
    res = RES_BAD_ARG;
    goto error;
  }

  if(args->mode == UNDEF_MODE) {
    print_multiple_modes(buf, sizeof(buf), EXCLUSIVE_MODES | USE_STDOUT_MODES, 0);
    logger_print(args->logger, LOG_WARNING,
      "Nothing to do.\nOne of the following options should be used: %s\n",
      buf);
    res = RES_BAD_ARG;
    goto error;
  }

  if(args->mode & (MODE_BIN_GREEN | MODE_GREEN)
    && !(args->mode & GREEN_COMPATIBLE_MODES))
  {
    print_multiple_modes(buf, sizeof(buf), GREEN_COMPATIBLE_MODES, 0);
    logger_print(args->logger, LOG_ERROR,
      "Option -g can only be used in conjunction with: %s\n",
      buf);
    res = RES_BAD_ARG;
    goto error;
  }

  if(args->mode & MODE_IR_COMPUTE && n_used) {
    logger_print(args->logger, LOG_ERROR,
      "The -n option has no effect in rendering mode;"
      " use rendering's SPP suboption instead.\n");
    res = RES_BAD_ARG;
    goto error;
  }

  if(args->mode & MODE_DUMP_PATHS) {
    if(!(args->mode & COMPUTE_MODES)) {
      res = RES_BAD_ARG;
      print_multiple_modes(buf, sizeof(buf), COMPUTE_MODES, 0);
      logger_print(args->logger, LOG_ERROR,
        "Option -%c can only be used in conjunction with an option"
        " that samples heat paths (%s).\n",
        mode_option(MODE_DUMP_PATHS), buf);
      goto error;
    }
    if(args->mode & (MODE_BIN_GREEN | MODE_GREEN)) {
      res = RES_BAD_ARG;
      logger_print(args->logger, LOG_ERROR,
        "Option -%c cannot be used in conjunction with -%c nor -%c.\n",
        mode_option(MODE_DUMP_PATHS), mode_option(MODE_GREEN)
        , mode_option(MODE_BIN_GREEN));
      goto error;
    }
  }

  if(args->mode & MODE_EXTENDED_RESULTS) {
    if(!(args->mode & COMPUTE_MODES)) {
      res = RES_BAD_ARG;
      print_multiple_modes(buf, sizeof(buf), EXT_COMPATIBLE_MODES, 0);
      logger_print(args->logger, LOG_ERROR,
        "Option -%c can only be used in conjunction with an option"
        " that computes a single Monte-Carlo (%s).\n",
        mode_option(MODE_EXTENDED_RESULTS), buf);
      goto error;
    }
    if(args->mode & (MODE_BIN_GREEN | MODE_GREEN)) {
      res = RES_BAD_ARG;
      logger_print(args->logger, LOG_ERROR,
        "Option -%c cannot be used in conjunction with -%c nor -%c.\n",
        mode_option(MODE_EXTENDED_RESULTS), mode_option(MODE_GREEN)
        , mode_option(MODE_BIN_GREEN));
      goto error;
    }
  }

end:
  return res;
error:
  logger_print(args->logger, LOG_ERROR, "Use option -h to print help.\n");
  goto end;
}

res_T
parse_camera
  (struct logger* logger,
   char* cam_param,
   struct camera* cam)
{
  char** line = NULL;
  char** opt = NULL;
  res_T res = RES_OK;

  ASSERT(cam_param && cam);

  line = split_line(cam_param, ':');

  if(line) {
    int i = 0;
    for(i = 0; *(line + i); i++) {
      size_t len = 0;
      opt = split_line(line[i], '=');
      ASSERT(opt[0] && opt[1] && !opt[2]);
      _strupr(opt[0]);
      if(strcmp(opt[0], "T") == 0) {
        ERR(cstr_to_double(opt[1], &cam->time));
      }
      else if(strcmp(opt[0], "FOV") == 0) {
        ERR(cstr_to_double(opt[1], &cam->fov));
      }
      else if(strcmp(opt[0], "UP") == 0) {
        ERR(cstr_to_list_double(opt[1], ',', cam->up, &len, 3));
      }
      else if(strcmp(opt[0], "TGT") == 0) {
        ERR(cstr_to_list_double(opt[1], ',', cam->tgt, &len, 3));
        cam->auto_look_at = 0;
      }
      else if(strcmp(opt[0], "POS") == 0) {
        ERR(cstr_to_list_double(opt[1], ',', cam->pos, &len, 3));
        cam->auto_look_at = 0;
      }
      else if(strcmp(opt[0], "IMG") == 0) {
        unsigned img_sz[2];
        ERR(cstr_to_list_uint(opt[1], 'x', img_sz, &len, 2));
        cam->img_width = img_sz[0];
        cam->img_height = img_sz[1];
      }
      else if(strcmp(opt[0], "SPP") == 0) {
        ERR(cstr_to_uint(opt[1], &cam->spp));
      } else {
        logger_print(logger, LOG_ERROR,
          "Unexpected option for rendering mode: %s.\n",
          opt[0]);
        res = RES_BAD_ARG;
        goto error;
      }
    }
  }

end:
#define FREE_AARRAY(ARRAY) if(ARRAY) {\
  int i = 0; \
  for(i=0; *(ARRAY+i);i++){\
    free(ARRAY[i]);\
  }\
  free(ARRAY);\
}
  FREE_AARRAY(line)
  FREE_AARRAY(opt)
#undef FREE_AARRAY

  return res;
error:
  logger_print(logger, LOG_ERROR, "Error parsing camera options.\n");
  logger_print(logger, LOG_ERROR, "Use the -h option to get help.\n");
  goto end;
}

static struct description*
find_description_by_name
  (struct stardis* stardis,
   const struct str* name,
   size_t* out_id)
{
  size_t i;
  ASSERT(stardis && name);

  FOR_EACH(i, 0, darray_descriptions_size_get(&stardis->descriptions)) {
    struct description* desc
      = darray_descriptions_data_get(&stardis->descriptions) + i;
    if(str_eq(name, get_description_name(desc))) {
      if(out_id) *out_id = i;
      return desc;
    }
  }
  return NULL;
}

/* H_BOUNDARY_FOR_SOLID Name emissivity specular_fraction hc T_env STL_filenames
 * H_BOUNDARY_FOR_FLUID Name emissivity specular_fraction hc T_env STL_filenames */
static res_T
process_h
  (struct stardis* stardis,
   const enum description_type type,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ASSERT(sz <= UINT_MAX);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz+1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  init_h(stardis->allocator, &desc->d.h_boundary);

  desc->d.h_boundary.mat_id = (unsigned)sz;
  desc->type = type;

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "h boundary name");
  ERR(str_set(&desc->d.h_boundary.name, tk));
  if(find_description_by_name(stardis, &desc->d.h_boundary.name, NULL)
    != desc)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "emissivity");
  res = cstr_to_double(tk, &desc->d.h_boundary.emissivity);
  if(res != RES_OK
    || desc->d.h_boundary.emissivity < 0
    || desc->d.h_boundary.emissivity > 1)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid emissivity: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "specular fraction");
  res = cstr_to_double(tk, &desc->d.h_boundary.specular_fraction);
  if(res != RES_OK
    || desc->d.h_boundary.specular_fraction < 0
    || desc->d.h_boundary.specular_fraction > 1)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid specular fraction: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "hc");
  res = cstr_to_double(tk, &desc->d.h_boundary.hc);
  if(res != RES_OK
    || desc->d.h_boundary.hc < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid hc: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "temperature");
  res = cstr_to_double(tk, &desc->d.h_boundary.imposed_temperature);
  if(res != RES_OK
    || desc->d.h_boundary.imposed_temperature < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid temperature: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  ERR(read_sides_and_files(stardis, 1, desc->d.h_boundary.mat_id, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* T_BOUNDARY_FOR_SOLID Name T STL_filenames
 * T_BOUNDARY_FOR_FLUID Name T emissivity specular_fraction hc STL_filenames */
static res_T
process_t
  (struct stardis* stardis,
   const enum description_type type,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ASSERT(sz <= UINT_MAX);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  init_t(stardis->allocator, &desc->d.t_boundary);

  desc->d.t_boundary.mat_id = (unsigned)sz;
  desc->type = type;

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "temperature boundary name");
  ERR(str_set(&desc->d.t_boundary.name, tk));
  if(find_description_by_name(stardis, &desc->d.t_boundary.name, NULL)
    != desc)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "temperature");
  res = cstr_to_double(tk, &desc->d.t_boundary.imposed_temperature);
  if(res != RES_OK
    || desc->d.t_boundary.imposed_temperature < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid temperature: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  if(type == DESC_BOUND_T_FOR_FLUID) {
    CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "emissivity");
    res = cstr_to_double(tk, &desc->d.t_boundary.emissivity);
    if(res != RES_OK
      || desc->d.t_boundary.emissivity < 0
      || desc->d.t_boundary.emissivity > 1)
    {
      logger_print(stardis->logger, LOG_ERROR, "Invalid emissivity: %s\n", tk);
      if(res == RES_OK) res = RES_BAD_ARG;
      goto end;
    }
    CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "specular fraction");
    res = cstr_to_double(tk, &desc->d.t_boundary.specular_fraction);
    if(res != RES_OK
      || desc->d.t_boundary.specular_fraction < 0
      || desc->d.t_boundary.specular_fraction > 1)
    {
      logger_print(stardis->logger, LOG_ERROR,
        "Invalid specular fraction: %s\n", tk);
      if(res == RES_OK) res = RES_BAD_ARG;
      goto end;
    }
    CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "hc");
    res = cstr_to_double(tk, &desc->d.t_boundary.hc);
    if(res != RES_OK
      || desc->d.t_boundary.hc < 0)
    {
      logger_print(stardis->logger, LOG_ERROR, "Invalid hc: %s\n", tk);
      if(res == RES_OK) res = RES_BAD_ARG;
      goto end;
    }
  }

  ERR(read_sides_and_files(stardis, 1, desc->d.t_boundary.mat_id, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* F_BOUNDARY_FOR_SOLID Name F STL_filenames */
static res_T
process_flx
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ASSERT(sz <= UINT_MAX);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  init_f(stardis->allocator, &desc->d.f_boundary);

  desc->d.f_boundary.mat_id = (unsigned)sz;
  desc->type = DESC_BOUND_F_FOR_SOLID;

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "flux boundary name");
  ERR(str_set(&desc->d.f_boundary.name, tk));
  if(find_description_by_name(stardis, &desc->d.f_boundary.name, NULL)
    != desc)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "flux");
  res = cstr_to_double(tk, &desc->d.f_boundary.imposed_flux);
  if(res != RES_OK
    || desc->d.f_boundary.imposed_flux == SDIS_FLUX_NONE) {
    /* Flux can be < 0 but not undefined */
    if(res == RES_OK) res = RES_BAD_ARG;
    logger_print(stardis->logger, LOG_ERROR, "Invalid flux: %s\n", tk);
    goto end;
  }

  ERR(read_sides_and_files(stardis, 1, desc->d.f_boundary.mat_id, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* SOLID_FLUID_CONNECTION Name emissivity specular_fraction hc STL_filenames */
static res_T
process_sfc
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  unsigned connection_id;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ASSERT(sz <= UINT_MAX);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  init_sf(stardis->allocator, &desc->d.sf_connect);

  connection_id = (unsigned)sz;
  desc->type = DESC_SOLID_FLUID_CONNECT;

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "solid fluid connection name");
  ERR(str_set(&desc->d.sf_connect.name, tk));
  if(find_description_by_name(stardis, &desc->d.sf_connect.name, NULL)
    != desc)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "emissivity");
  res = cstr_to_double(tk, &desc->d.sf_connect.emissivity);
  if(res != RES_OK
    || desc->d.sf_connect.emissivity < 0
    || desc->d.h_boundary.emissivity > 1)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid emissivity: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "specular fraction");
  res = cstr_to_double(tk, &desc->d.sf_connect.specular_fraction);
  if(res != RES_OK
    || desc->d.sf_connect.specular_fraction < 0
    || desc->d.sf_connect.specular_fraction > 1)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Invalid specular fraction: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "hc");
  res = cstr_to_double(tk, &desc->d.sf_connect.hc);
  if(res != RES_OK
    || desc->d.sf_connect.hc < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid hc: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  ERR(read_sides_and_files(stardis, 1, connection_id, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* SOLID Name lambda rho cp delta Tinit volumic_power STL_filenames */
static res_T
process_solid
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ASSERT(sz <= UINT_MAX);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  init_solid(stardis->allocator, &desc->d.solid);

  desc->d.solid.solid_id = (unsigned)sz;
  desc->type = DESC_MAT_SOLID;
  
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "solid name");
  ERR(str_set(&desc->d.solid.name, tk));
  if(find_description_by_name(stardis, &desc->d.solid.name, NULL)
    != desc)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "lambda");
  res = cstr_to_double(tk, &desc->d.solid.lambda);
  if(res != RES_OK
    || desc->d.solid.lambda <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid lambda: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "rho");
  res = cstr_to_double(tk, &desc->d.solid.rho);
  if(res != RES_OK
    || desc->d.solid.rho <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid rho: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "cp");
  res = cstr_to_double(tk, &desc->d.solid.cp);
  if(res != RES_OK
    || desc->d.solid.cp <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid cp: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "delta");
  res = cstr_to_double(tk, &desc->d.solid.delta);
  if(res != RES_OK
    || desc->d.solid.delta <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid delta: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "Tinit");
  res = cstr_to_double(tk, &desc->d.solid.tinit);
  if(res != RES_OK
    || desc->d.solid.tinit < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid Tinit: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "volumic power");
  res = cstr_to_double(tk, &desc->d.solid.vpower);
  if(res != RES_OK) {
    /* VPower can be < 0 */
    logger_print(stardis->logger, LOG_ERROR, "Invalid volumic power: %s\n", tk);
    goto end;
  }

  ERR(read_sides_and_files(stardis, 0, desc->d.solid.solid_id, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* FLUID Name rho cp Tinit STL_filenames */
static res_T
process_fluid
  (struct stardis* stardis,
   char** tok_ctx)
{
  char* tk = NULL;
  struct description* desc;
  size_t sz;
  res_T res = RES_OK;

  ASSERT(stardis && tok_ctx);

  sz = darray_descriptions_size_get(&stardis->descriptions);
  ASSERT(sz <= UINT_MAX);
  ERR(darray_descriptions_resize(&stardis->descriptions, sz + 1));
  desc = darray_descriptions_data_get(&stardis->descriptions) + sz;
  init_fluid(stardis->allocator, &desc->d.fluid);

  desc->d.fluid.fluid_id = (unsigned)sz;
  desc->type = DESC_MAT_FLUID;
  
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "fluid name");
  ERR(str_set(&desc->d.fluid.name, tk));
  if(find_description_by_name(stardis, &desc->d.fluid.name, NULL)
    != desc)
  {
    logger_print(stardis->logger, LOG_ERROR,
      "Name already used: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "rho");
  res = cstr_to_double(tk, &desc->d.fluid.rho);
  if(res != RES_OK
    || desc->d.fluid.rho <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid rho: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "cp");
  res = cstr_to_double(tk, &desc->d.fluid.cp);
  if(res != RES_OK
    || desc->d.fluid.cp <= 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid cp: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " \t", tok_ctx), "Tinit");
  res = cstr_to_double(tk, &desc->d.fluid.tinit);
  if(res != RES_OK
    || desc->d.fluid.tinit < 0)
  {
    logger_print(stardis->logger, LOG_ERROR, "Invalid Tinit: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }

  ERR(read_sides_and_files(stardis, 0, desc->d.fluid.fluid_id, tok_ctx));

end:
  return res;
error:
  goto end;
}

/* Read medium or boundary line; should be one of:
 * SOLID Name lambda rho cp delta Tinit volumic_power STL_filenames
 * FLUID Name rho cp Tinit STL_filenames
 * H_BOUNDARY_FOR_SOLID Name emissivity specular_fraction hc T_env STL_filenames
 * H_BOUNDARY_FOR_FLUID Name emissivity specular_fraction hc T_env STL_filenames
 * T_BOUNDARY_FOR_SOLID Name T STL_filenames
 * T_BOUNDARY_FOR_FLUID Name T hc STL_filenames
 * F_BOUNDARY_FOR_SOLID Name F STL_filenames
 * SOLID_FLUID_CONNECTION Name emissivity specular_fraction hc STL_filenames
 *
 * STL_filenames = { { FRONT | BACK | BOTH } STL_filename }+
 */
res_T
process_model_line
  (const char* file_name,
   char* line,
   struct stardis* stardis)
{
  res_T res = RES_OK;
  struct str keep;
  char* tk = NULL, * tok_ctx = NULL;

  ASSERT(file_name && line && stardis);

  str_init(stardis->allocator, &keep);
  ERR(str_set(&keep, line));
  CHK_TOK(strtok_r(line, " \t", &tok_ctx), "model line type");
  _strupr(tk);
  
  if(0 == strcmp(tk, "H_BOUNDARY_FOR_SOLID"))
    ERR(process_h(stardis, DESC_BOUND_H_FOR_SOLID, &tok_ctx));
  else if(0 == strcmp(tk, "H_BOUNDARY_FOR_FLUID"))
    ERR(process_h(stardis, DESC_BOUND_H_FOR_FLUID, &tok_ctx));
  else if(0 == strcmp(tk, "T_BOUNDARY_FOR_SOLID"))
    ERR(process_t(stardis, DESC_BOUND_T_FOR_SOLID, &tok_ctx));
  else if(0 == strcmp(tk, "T_BOUNDARY_FOR_FLUID"))
    ERR(process_t(stardis, DESC_BOUND_T_FOR_FLUID, &tok_ctx));
  else if(0 == strcmp(tk, "F_BOUNDARY_FOR_SOLID"))
    ERR(process_flx(stardis, &tok_ctx));
  else if(0 == strcmp(tk, "SOLID_FLUID_CONNECTION"))
    ERR(process_sfc(stardis, &tok_ctx));
  else if(0 == strcmp(tk, "SOLID"))
    ERR(process_solid(stardis, &tok_ctx));
  else if(0 == strcmp(tk, "FLUID"))
    ERR(process_fluid(stardis, &tok_ctx));
  else {
    logger_print(stardis->logger, LOG_ERROR,
      "Unknown description type: %s\n", tk);
    res = RES_BAD_ARG;
    goto error;
  }

end:
  str_release(&keep);
  return res;
error:
  logger_print(stardis->logger, LOG_ERROR,
    "Invalid description line in model file '%s':\n", file_name);
  logger_print(stardis->logger, LOG_ERROR, "%s\n", str_cget(&keep));
  goto end;
}