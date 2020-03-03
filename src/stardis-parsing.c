#define _GNU_SOURCE 1
#include "stardis-parsing.h"
#include "stardis-app.h"
#include "stardis-version.h"

#include <rsys/cstr.h>
#include <rsys/double3.h>
#include <sdis_version.h>

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
  size_t count = 0;
  char* tmp = a_str;
  char* last_comma = 0;
  char delim[2];
  delim[0] = a_delim;
  delim[1] = 0;

  ASSERT(a_str);

  /* Count how many elements will be extracted. */
  while (*tmp)
  {
    if(a_delim == *tmp)
    {
      count++;
      last_comma = tmp;
    }
    tmp++;
  }

  /* Add space for trailing token. */
  count += last_comma < (a_str + strlen(a_str) - 1);

  /* Add space for terminating null string so caller
     knows where the list of returned strings ends. */
  count++;

  result = malloc(sizeof(char*) * count);

  if(result)
  {
    size_t idx = 0;
    char* token = strtok(a_str, delim);

    while (token)
    {
      ASSERT(idx < count);
#ifdef COMPILER_CL
      *(result + idx++) = _strdup(token);
#else
      *(result + idx++) = strdup(token);
#endif
      token = strtok(0, delim);
    }
    ASSERT(idx == count - 1);
    *(result + idx) = 0;
  }

  return result;
}

static void
print_version
  ()
{
  printf("Stardis version %i.%i.%i built on stardis solver version %i.%i.%i\n",
    StardisApp_VERSION_MAJOR, StardisApp_VERSION_MINOR, StardisApp_VERSION_PATCH,
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
  ERR(sstl_create(NULL, stardis->allocator, 0, &sstl));
  for(;;) {
    struct str name;
    if(side_is_intface) {
      add_geom_ctx.properties[SG3D_FRONT] = SG3D_UNSPECIFIED_PROPERTY;
      add_geom_ctx.properties[SG3D_BACK] = SG3D_UNSPECIFIED_PROPERTY;
      add_geom_ctx.properties[SG3D_INTFACE] = id;
    } else {
      tk = strtok_r(NULL, " ", tok_ctx);
      if(!tk) {
        if(file_count == 0) {
          /* At least 1 side */
          fprintf(stderr, "Invalid data (missing token 'side')\n");
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
        fprintf(stderr, "Invalid side specifier: %s\n", tk);
        res = RES_BAD_ARG;
        goto error;
      }
    }
    tk = strtok_r(NULL, " ", tok_ctx);
    if(!tk) {
      if(!side_is_intface /* Has read a side */
        || !file_count) /* Need at least 1 file */
      {
        fprintf(stderr, "Invalid data (missing token 'file name')\n");
        res = RES_BAD_ARG;
        goto error;
      }
      else break;
    }
    file_count++;
    res = sstl_load(sstl, tk);
    if(res != RES_OK) {
      fprintf(stderr, "Cannot read STL file: %s\n", tk);
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
      fprintf(stderr, "Cannot add file content: %s\n", tk);
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
init_args(struct mem_allocator* alloc, struct args** out_args)
{
  res_T res = RES_OK;
  struct args* args = NULL;
  ASSERT(alloc && out_args);

  args = calloc(sizeof(struct args), 1);
  if(!args) {
    res = RES_MEM_ERR;
    goto error;
  }

  args->allocator = alloc;
  darray_str_init(alloc, &args->model_files);
  /* Some fields have non-zero default values */
  args->samples = 10000;
  args->nthreads = DEFAULT_NTHREADS;
  args->probe[3] = INF; /* Probe time */
  args->scale_factor = 1;
  args->radiative_temp[0] = args->radiative_temp[1] = 300;

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
  ASSERT(args && args->allocator);
  args->allocator = NULL;
  darray_str_release(&args->model_files);
}

char
mode_option
  (const enum stardis_mode m)
{
  int found = 0;
  char res = '?';
  if(m & PROBE_COMPUTE) { found++; res = 'p'; }
  if(m & PROBE_COMPUTE_ON_INTERFACE) { found++; res = 'P'; }
  if(m & MEDIUM_COMPUTE) { found++; res = 'm'; }
  if(m & BOUNDARY_COMPUTE) { found++; res = 's'; }
  if(m & IR_COMPUTE) { found++; res = 'R'; }
  if(m & MAP_COMPUTE) { found++; res = 'S'; }
  if (m & DUMP_VTK) { found++; res = 'd'; }
  if (m & FLUX_BOUNDARY_COMPUTE) { found++; res = 'F'; }
  ASSERT(found == 1);
  return res;
}

void print_multiple_modes
  (FILE* stream,
   const int modes)
{
  int b = 0, fst = 1;
  int m = UNDEF_MODE;
  ASSERT(stream);
  do {
    m = BIT(b++);
    if(m & modes) {
      fprintf(stream, (fst ? " -%c" : ", -%c"), mode_option(m));
      fst = 0;
    }
  } while (m <= modes);
}

void
print_help
  (const char* prog)
{
  char* name;
#ifdef COMPILER_GCC
  name = 1 + strrchr(prog, '/');
#else
  name = 1 + strrchr(prog, '\\');
#endif
  ASSERT(prog);
  print_version();
  printf("\nUsage: \n");
  printf("------\n");
  printf("\n%s {-M MODEL_FILE_NAME}+\n", name);
  printf("  [ -p X,Y,Z,TIME | -P X,Y,Z,TIME | -m MEDIUM_NAME,TIME\n");
  printf("    | -s STL_FILE_NAME,TIME | -S STL_FILE_NAME,TIME | -F STL_FILE_NAME,TIME\n");
  printf("    | -R t=TIME:spp=SPP:fov=FOV:up=XUP,YUP,ZUP:pos=X,Y,Z:tgt=XT,YT,ZT:img=WxH ]\n");
  printf("  [-d] [-g]  [-f SCALE_FACTOR] [-D {all|error|success}] [-n REALIZATIONS_COUNT]\n");
  printf("  [-t NUM_OF_THREADS] [-r AMBIENT_RAD_TEMP:REFERENCE_TEMP]\n");
  printf("\n%s -h: print this help.\n", name);
  printf("\n%s -v: print stardis-app version.\n", name);
  printf("\nMandatory arguments:\n");
  printf("--------------------\n");
  printf("\n  -M <MODEL_FILE_NAME>:\n");
  printf("     Add a text file that contains (partial) description of the model.\n");
  printf("     Can include both media and boundary conditions, in any order.\n");
  printf("     Can be used more than once if description is split across different files.\n");
  printf("\nExclusive arguments:\n");
  printf("--------------------\n");
  printf("\n -p X,Y,Z:TIME:\n");
  printf("    Compute the temperature at the given probe.\n");
  printf("    The probe must be in a medium.\n");
  printf("\n -P X,Y,Z:TIME:\n");
  printf("    Compute the temperature at the given probe on an interface.\n");
  printf("    The probe is suposed to be on an interface and is adjusted to the\n");
  printf("    closest point of the closest interface.\n");
  printf("\n -m MEDIUM_NAME,TIME:\n");
  printf("    Compute the mean temperature in a given medium at a given time. The medium\n");
  printf("    doesn't need to be connex.\n");
  printf("\n -s STL_FILE_NAME,TIME:\n");
  printf("    Compute the mean temperature on a given 2D region at a given time, the\n");
  printf("    region being defined as the front sides of the triangles in the provided\n");
  printf("    file. These triangles are not added to the geometry, but must be already\n");
  printf("    part of it. The region doesn't need to be connex.\n");
  printf("\n -S STL_FILE_NAME,TIME:\n");
  printf("    Compute the by-triangle mean temperature on a given 2D region at a given\n");
  printf("    time, the region being defined as the front sides of the triangles in the\n");
  printf("    provided file. These triangles are not added to the geometry, but must be\n");
  printf("    already part of it. The region doesn't need to be connex.\n");
  printf("\n -F STL_FILE_NAME,TIME:\n");
  printf("    Compute the mean flux on a given 2D region at a given time, the region\n");
  printf("    being defined as the front sides of the triangles in the provided file.\n");
  printf("    Flux is accounted positive when going from the front side to the back side,\n");
  printf("    at a single-triangle scale. These triangles are not added to the geometry,\n");
  printf("    but must be already part of it. The region doesn't need to be connex.\n");
  printf("\n -R t=TIME:spp=SPP:fov=FOV:up=XUP,YUP,ZUP:pos=X,Y,Z:tgt=XT,YT,ZT:img=WxH:\n");
  printf("    The infra-red rendering mode.\n");
  printf("    t is the rendering time (default: INF).\n");
  printf("    spp is the number of samples per pixel (default: 4).\n");
  printf("    fov is the field of view (default: 70 degrees).\n");
  printf("    up is the up direction (default: 0,0,1).\n");
  printf("    pos is the position of the camera (default: 1,1,1).\n");
  printf("    tgt is the target of the camera (default: 0,0,0).\n");
  printf("    img is the image resolution (default: 640x480).\n");
  printf("\nOptionnal arguments:\n");
  printf("--------------------\n");
  printf("\n -d:\n");
  printf("    Dump the geometry in VTK format with various properties, including possible\n");
  printf("    errors. If this option is used, no computation occurs. Including both this\n");
  printf("    option and one of the options specifying a compute region (");
  print_multiple_modes(stdout, REGION_COMPUTE_MODES); printf(") has\n");
  printf("    the effect to include the region in the dump.\n");
  printf("\n -g:\n");
  printf("    Compute the green function at t=INF.\n");
  printf("    Can only be used in conjonction with one these options:");
  print_multiple_modes(stdout, GREEN_COMPATIBLE_MODES); printf("\n");
  printf("    Provided TIME is silently ignored.\n");
  printf("\n -f SCALE_FACTOR.\n");
  printf("    Default value: 1.0.\n");
  printf("\n -D { all|error|success }:\n");
  printf("    Dump paths in VTK format.\n");
  printf("\n -n NUM_OF_REALIZATIONS:\n");
  printf("    Number of Monte-Carlo realizations.\n");
  printf("    Default value: 10000.\n");
  printf("\n -t NUM_OF_THREADS:\n");
  printf("    Number of threads; default value is all threads available.\n");
  printf("\n -r AMBIENT_RAD_TEMP,REFERENCE_TEMP:\n");
  printf("    Parameters for the radiative transfer.\n");
  printf("    AMBIENT_RAD_TEMP is the ambient radiative temperature .\n");
  printf("    REFERENCE_TEMP is the temperature used for the linearization\n");
  printf("    of the radiative transfer.\n");
  printf("    Default values: 300 and 300.\n");
  printf("\n -V:\n");
  printf("    Verbose mode.\n");
}

res_T
parse_args
  (const int argc,
   char** argv,
   struct args* args)
{
  int opt = 0, n_used = 0;
  size_t len = 0;
  res_T res = RES_OK;

  ASSERT(argv && args);

  if(argc == 1) {
    print_help(argv[0]);
    res = RES_BAD_ARG;
    goto error;
  }

  opterr = 0; /* No default error messages */
  while ((opt = getopt(argc, argv, "hvgn:t:B:M:m:p:P:dD:s:S:F:f:r:R:V")) != -1) {
    switch (opt) {
    case '?': /* Unreconised option */
      res = RES_BAD_ARG;
      fprintf(stderr, "Invalid option -%c\n", optopt);
      goto error;

    case 'h':
      print_help(argv[0]);
      args->just_help = 1;
      goto end;

    case 'v':
      print_version();
      args->just_help = 1;
      goto end;

    case 'g':
      args->mode |= GREEN_MODE;
      break;

    case 'n': {
      unsigned long n;
      res = cstr_to_ulong(optarg, &n);
      if(res != RES_OK
        || n == 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      args->samples = n;
      n_used = 1;
      break;
    }

    case 'M': {
      struct str name;
      str_init(args->allocator, &name);
      str_set(&name, optarg);
      ERR(darray_str_push_back(&args->model_files, &name));
      str_release(&name);
      break;
    }

    case 't':
      res = cstr_to_uint(optarg, &args->nthreads);
      if(res != RES_OK
        || args->nthreads <= 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 'p':
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= PROBE_COMPUTE;
      cstr_to_list_double(optarg, ',', args->probe, &len, 4);
      if(len != 4
        || res != RES_OK
        || args->probe[3] < 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 'P':
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= PROBE_COMPUTE_ON_INTERFACE;
      cstr_to_list_double(optarg, ',', args->probe, &len, 4);
      if(len != 4
        || res != RES_OK
        || args->probe[3] < 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 's':
    case 'S':
    case 'F': {
      int err = 0;
      char *ptr;
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      switch (opt) {
      case 's':
        args->mode |= BOUNDARY_COMPUTE;
        break;
      case 'S':
        args->mode |= MAP_COMPUTE;
        break;
      case 'F':
        args->mode |= FLUX_BOUNDARY_COMPUTE;
        break;
      }
      ptr = strrchr(optarg, ',');
      /* Single ',' allowed */
      if(!ptr || ptr != strchr(optarg, ','))
        err = 1;
      else {
        *ptr = '\0';
        ptr++;
        args->solve_boundary_filename = optarg;
        err = (RES_OK != cstr_to_double(ptr, args->probe + 3) || args->probe[3] < 0);
      }
      if(err) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;
    }

    case 'm': {
      char* ptr;
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= MEDIUM_COMPUTE;
      ptr = strpbrk(optarg, ",");
      if(!ptr || ptr == optarg)
        res = RES_BAD_ARG;
      else res = cstr_to_double(ptr + 1, args->probe + 3);
      if(res != RES_OK) {
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      *ptr = '\0';
      args->medium_name = optarg;
      break;
    }

    case 'd':
      args->mode |= DUMP_VTK;
      break;

    case 'D':
      if(0 == strcmp(optarg, "all")) {
        args->dump_paths = DUMP_ALL;
      }
      else if(0 == strcmp(optarg, "error")) {
        args->dump_paths = DUMP_ERROR;
      }
      else if(0 == strcmp(optarg, "success")) {
        args->dump_paths = DUMP_SUCCESS;
      } else {
        res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 'f':
      cstr_to_double(optarg, &args->scale_factor);
      if(res != RES_OK
        || args->scale_factor <= 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 'r':
      cstr_to_list_double(optarg, ',', args->radiative_temp, &len, 2);
      if(res != RES_OK
        || len != 2
        || args->radiative_temp[0] <= 0
        || args->radiative_temp[1] <= 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 'R':
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= IR_COMPUTE;
      args->camera = optarg;
      break;

    case 'V':
      args->verbose = 1;
      break;
    }
  }
  
  if(args->mode & GREEN_MODE
    && (args->mode != (args->mode & (GREEN_MODE | GREEN_COMPATIBLE_MODES))))
  {
    fprintf(stderr, "Option -g can only be used in conjunction with:");
    print_multiple_modes(stderr, GREEN_COMPATIBLE_MODES);
    fprintf(stderr, "\n");
    res = RES_BAD_ARG;
    goto error;
  }

  if(!darray_str_size_get(&args->model_files)) {
    fprintf(stderr, "Missing mandatory argument: -M <model_file_name>\n");
    res = RES_BAD_ARG;
    goto error;
  }

  if(args->mode & IR_COMPUTE && n_used) {
    fprintf(stderr, "The -n option has no effect in rendering mode;"
      " use rendering's SPP suboption instead.\n");
    res = RES_BAD_ARG;
    goto error;
  }

end:
  return res;
error:
  fprintf(stderr, "Use the -h option to get help.\n");
  goto end;
}

res_T
parse_camera
  (char* cam_param,
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
      }
      else if(strcmp(opt[0], "POS") == 0) {
        ERR(cstr_to_list_double(opt[1], ',', cam->pos, &len, 3));
      }
      else if(strcmp(opt[0], "IMG") == 0) {
        ERR(cstr_to_list_uint(opt[1], 'x', cam->img, &len, 2));
      }
      else if(strcmp(opt[0], "SPP") == 0) {
        ERR(cstr_to_uint(opt[1], &cam->spp));
      } else {
        fprintf(stderr, "Warning: unexpected option for rendering mode: %s.\n",
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
  fprintf(stderr, "Use the -h option to get help.\n");
  goto end;
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

  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "h boundary name");
  ERR(str_set(&desc->d.h_boundary.name, tk));

  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "emissivity");
  res = cstr_to_double(tk, &desc->d.h_boundary.emissivity);
  if(res != RES_OK
    || desc->d.h_boundary.emissivity < 0
    || desc->d.h_boundary.emissivity > 1)
  {
    fprintf(stderr, "Invalid emissivity: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "specular fraction");
  res = cstr_to_double(tk, &desc->d.h_boundary.specular_fraction);
  if(res != RES_OK
    || desc->d.h_boundary.specular_fraction < 0
    || desc->d.h_boundary.specular_fraction > 1)
  {
    fprintf(stderr, "Invalid specular fraction: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "hc");
  res = cstr_to_double(tk, &desc->d.h_boundary.hc);
  if(res != RES_OK
    || desc->d.h_boundary.hc < 0)
  {
    fprintf(stderr, "Invalid hc: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "temperature");
  res = cstr_to_double(tk, &desc->d.h_boundary.imposed_temperature);
  if(res != RES_OK
    || desc->d.h_boundary.imposed_temperature < 0)
  {
    fprintf(stderr, "Invalid temperature: %s\n", tk);
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
 * T_BOUNDARY_FOR_FLUID Name T hc STL_filenames */
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

  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "temperature boundary name");
  ERR(str_set(&desc->d.t_boundary.name, tk));

  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "temperature");
  res = cstr_to_double(tk, &desc->d.t_boundary.imposed_temperature);
  if(res != RES_OK
    || desc->d.t_boundary.imposed_temperature < 0)
  {
    fprintf(stderr, "Invalid temperature: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  if(type == DESC_BOUND_T_FOR_FLUID) {
    CHK_TOK(strtok_r(NULL, " ", tok_ctx), "hc");
    res = cstr_to_double(tk, &desc->d.t_boundary.hc);
    if(res != RES_OK
      || desc->d.t_boundary.hc < 0)
    {
      fprintf(stderr, "Invalid hc: %s\n", tk);
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

  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "flux boundary name");
  ERR(str_set(&desc->d.f_boundary.name, tk));

  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "flux");
  res = cstr_to_double(tk, &desc->d.f_boundary.imposed_flux);
  if(res != RES_OK) {
    /* Flux can be < 0 */
    fprintf(stderr, "Invalid flux: %s\n", tk);
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

  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "connection name");
  ERR(str_set(&desc->d.sf_connect.name, tk));

  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "emissivity");
  res = cstr_to_double(tk, &desc->d.sf_connect.emissivity);
  if(res != RES_OK
    || desc->d.sf_connect.emissivity < 0
    || desc->d.h_boundary.emissivity > 1)
  {
    fprintf(stderr, "Invalid emissivity: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "specular fraction");
  res = cstr_to_double(tk, &desc->d.sf_connect.specular_fraction);
  if(res != RES_OK
    || desc->d.sf_connect.specular_fraction < 0
    || desc->d.sf_connect.specular_fraction > 1)
  {
    fprintf(stderr, "Invalid specular fraction: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "hc");
  res = cstr_to_double(tk, &desc->d.sf_connect.hc);
  if(res != RES_OK
    || desc->d.sf_connect.hc <= 0)
  {
    fprintf(stderr, "Invalid hc: %s\n", tk);
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
  
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "solid name");
  ERR(str_set(&desc->d.solid.name, tk));

  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "lambda");
  res = cstr_to_double(tk, &desc->d.solid.lambda);
  if(res != RES_OK
    || desc->d.solid.lambda <= 0)
  {
    fprintf(stderr, "Invalid lambda: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "rho");
  res = cstr_to_double(tk, &desc->d.solid.rho);
  if(res != RES_OK
    || desc->d.solid.rho <= 0)
  {
    fprintf(stderr, "Invalid rho: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "cp");
  res = cstr_to_double(tk, &desc->d.solid.cp);
  if(res != RES_OK
    || desc->d.solid.cp <= 0)
  {
    fprintf(stderr, "Invalid cp: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "delta");
  res = cstr_to_double(tk, &desc->d.solid.delta);
  if(res != RES_OK
    || desc->d.solid.delta <= 0)
  {
    fprintf(stderr, "Invalid delta: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "Tinit");
  res = cstr_to_double(tk, &desc->d.solid.tinit);
  if(res != RES_OK
    || desc->d.solid.tinit < 0)
  {
    fprintf(stderr, "Invalid Tinit: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "volumic power");
  res = cstr_to_double(tk, &desc->d.solid.vpower);
  if(res != RES_OK) {
    /* VPower can be < 0 */
    fprintf(stderr, "Invalid volumic power: %s\n", tk);
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
  
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "fluid name");
  ERR(str_set(&desc->d.fluid.name, tk));

  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "rho");
  res = cstr_to_double(tk, &desc->d.fluid.rho);
  if(res != RES_OK
    || desc->d.fluid.rho <= 0)
  {
    fprintf(stderr, "Invalid rho: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "cp");
  res = cstr_to_double(tk, &desc->d.fluid.cp);
  if(res != RES_OK
    || desc->d.fluid.cp <= 0)
  {
    fprintf(stderr, "Invalid cp: %s\n", tk);
    if(res == RES_OK) res = RES_BAD_ARG;
    goto end;
  }
  CHK_TOK(strtok_r(NULL, " ", tok_ctx), "Tinit");
  res = cstr_to_double(tk, &desc->d.fluid.tinit);
  if(res != RES_OK
    || desc->d.fluid.tinit < 0)
  {
    fprintf(stderr, "Invalid Tinit: %s\n", tk);
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
  (char* line,
   struct stardis* stardis)
{
  res_T res = RES_OK;
  char* tk = NULL, * tok_ctx = NULL;

  ASSERT(line && stardis);

  CHK_TOK(strtok_r(line, " ", &tok_ctx), "model line type");
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
    fprintf(stderr, "Unknown description type: %s\n", tk);
    res = RES_BAD_ARG;
    goto error;
  }

end:
  return res;
error:
  goto end;
}