#define _GNU_SOURCE 1
#include "stardis-parsing.h"
#include "stardis-app.h"
#include "stardis-version.h"

#include <rsys/cstr.h>
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
  for (ptr = s; *ptr; ++ptr) {
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
    if (a_delim == *tmp)
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

  if (result)
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
  printf("stardis-app version %i.%i.%i built on stardis library version %i.%i.%i\n",
    StardisApp_VERSION_MAJOR, StardisApp_VERSION_MINOR, StardisApp_VERSION_PATCH,
    Stardis_VERSION_MAJOR, Stardis_VERSION_MINOR, Stardis_VERSION_PATCH);
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

char
mode_option
  (const enum stardis_mode m)
{
  int found = 0;
  char res = '?';
  if (m & PROBE_COMPUTE) { found++; res = 'p'; }
  if (m & PROBE_COMPUTE_ON_INTERFACE) { found++; res = 'P'; }
  if (m & MEDIUM_COMPUTE) { found++; res = 'm'; }
  if (m & BOUNDARY_COMPUTE) { found++; res = 's'; }
  if (m & IR_COMPUTE) { found++; res = 'R'; }
  if (m & MAP_COMPUTE) { found++; res = 'S'; }
  if (m & DUMP_VTK) { found++; res = 'd'; }
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
    if (m & modes) {
      fprintf(stream, (fst ? " -%c" : ", -%c"), mode_option(m));
      fst = 0;
    }
  } while (m <= modes);
}

void
print_help
  (char* prog)
{
  ASSERT(prog);
  print_version();
  printf("\nUsage: \n%s -M MEDIUM.txt -B BOUNDARY.txt\n", prog);
  printf("[ -p X,Y,Z,TIME | -m MEDIUM_NAME,TIME | -d \n");
  printf("  | -s SURFACE.vtk,TIME | -S SURFACE.vtk,TIME\n");
  printf("  | -R t=TIME:spp=SPP:fov=FOV:up=XUP,YUP,ZUP:pos=X,Y,Z:tgt=XT,YT,ZT:img=WxH ]\n");
  printf("[ -g] [-f SCALE_FACTOR] [-D {all | error | success}] -d\n");
  printf("[-n NUM_OF_REALIZATIONS] [-t NUM_OF_THREADS]\n");
  printf("[-r AMBIENT_RAD_TEMP:REFERENCE_TEMP]\n");
  printf("\n  -h: print this help.\n");
  printf("  -v: print stardis-app version.\n");
  printf("\nMandatory arguments\n");
  printf("-------------------\n");
  printf("\n  -M MEDIUM.txt:\n");
  printf("     MEDIUM.txt is a text file which contains the description of the\n");
  printf("     media.\n");
  printf("\n  -B BOUNDARY.txt:\n");
  printf("     BOUNDARY.txt is a text file which contains the description of the\n");
  printf("     boundary conditions.\n");
  printf("\nExclusive arguments\n");
  printf("-------------------\n");
  printf("\n -p X,Y,Z:TIME:\n");
  printf("    Compute the temperature at the given probe.\n");
  printf("    The probe must be in a medium.\n");
  printf("    If some boundary conditions are time-dependant, TIME cannot be INF.\n");
  printf("\n -P X,Y,Z:TIME:\n");
  printf("    Compute the temperature at the given probe on an interface.\n");
  printf("    The probe must be in a medium and is moved to the closest interface.\n");
  printf("    If some boundary conditions are time-dependant, TIME cannot be INF.\n");
  printf("\n -m MEDIUM_NAME,TIME:\n");
  printf("    Compute the mean temperature in a given medium at a given time.\n");
  printf("    If some boundary conditions are time-dependant, TIME cannot be INF.\n");
  printf("\n -s SURFACE.stl,TIME:\n");
  printf("    Compute the mean temperature on a given 2D shape at a given time.\n");
  printf("    Mean temperature is computed on the front sides of the primitives\n");
  printf("    listed in SURFACE.stl. These primitives are not added to the geometry,\n");
  printf("    but must be already known. The shape doesn't need to be connex.\n");
  printf("    If some boundary conditions are time-dependant, TIME cannot be INF.\n");
  printf("\n -S SURFACE.stl,TIME:\n");
  printf("    Compute by-primitives mean temperature on a given 2D shape at a given time.\n");
  printf("    Mean temperature is computed on the front sides of the primitives\n");
  printf("    listed in SURFACE.stl. These primitives are not added to the geometry,\n");
  printf("    but must be already known. The shape doesn't need to be connex.\n");
  printf("    If some boundary conditions are time-dependant, TIME cannot be INF.\n");
  printf("\n -F SURFACE.stl,TIME:\n");
  printf("    Compute the mean flux on a given 2D shape at a given time.\n");
  printf("    Mean flux is computed on the primitives listed in SURFACE.stl and is\n");
  printf("    accounted positive, at the primitive scale, when going from the front\n");
  printf("    side to the back side. These primitives are not added to the geometry,\n");
  printf("    but must be already known. The shape doesn't need to be connex.\n");
  printf("    If some boundary conditions are time-dependant, TIME cannot be INF.\n");
  printf("\n -R t=TIME:spp=SPP:fov=FOV:up=XUP,YUP,ZUP:pos=X,Y,Z:tgt=XT,YT,ZT:img=WxH:\n");
  printf("    The infra-red rendering mode.\n");
  printf("    t is the rendering time (default: INF).\n");
  printf("    spp is the number of samples per pixel (default: 4).\n");
  printf("    fov is the field of view (default: 70 degrees).\n");
  printf("    up is the up direction (default: 0,0,1).\n");
  printf("    pos is the position of camera (default: 1,1,1).\n");
  printf("    tgt is the target (default: 0,0,0).\n");
  printf("    img is the resolution (default: 640x480).\n");
  printf("\n -d:\n");
  printf("    Dump the geometry in VTK format with medium front/back id and\n");
  printf("    boundary id.\n");
  printf("\nOptionnal arguments\n");
  printf("-------------------\n");
  printf("\n -g:\n");
  printf("    Compute the green function.\n");
  printf("    Can only be used in conjonction with one these options:");
  print_multiple_modes(stdout, GREEN_COMPATIBLE_MODES); printf("\n");
  printf("    Provided TIME is silently ignored.\n");
  printf("\n -f SCALE_FACTOR.\n");
  printf("    Default value: 1.0.\n");
  printf("\n -D { all | error | success }:\n");
  printf("    dump paths in VTK format.\n");
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

  if (argc == 1) {
    print_help(argv[0]);
    res = RES_BAD_ARG;
    goto error;
  }

  opterr = 0; /* No default error messages */
  while ((opt = getopt(argc, argv, "hvgn:t:B:M:m:p:P:dD:s:S:F:f:r:R:")) != -1) {
    switch (opt) {
    case '?': /* Unreconised option */
      res = RES_BAD_ARG;
      fprintf(stderr, "Invalid option -%c\n", optopt);
      goto error;

    case 'h':
      print_help(argv[0]);
      args->just_help = 1;
      goto exit;

    case 'v':
      print_version();
      args->just_help = 1;
      goto exit;

    case 'g':
      args->mode |= GREEN_MODE;
      break;

    case 'n': {
      unsigned long n;
      res = cstr_to_ulong(optarg, &n);
      if (res != RES_OK || n == 0) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      args->N = n;
      n_used = 1;
      break;
    }

    case 'B':
      args->bc_filename = optarg;
      break;

    case 'M':
      args->medium_filename = optarg;
      break;

    case 't':
      res = cstr_to_uint(optarg, &args->nthreads);
      if (res != RES_OK
        || args->nthreads <= 0) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 'p':
      if (args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= PROBE_COMPUTE;
      cstr_to_list_double(optarg, ',', args->u.probe, &len, 4);
      if (len != 4
        || res != RES_OK
        || args->u.probe[3] < 0)
      {
        res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 'P':
      if (args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= PROBE_COMPUTE_ON_INTERFACE;
      cstr_to_list_double(optarg, ',', args->u.probe, &len, 4);
      if (len != 4
        || res != RES_OK
        || args->u.probe[3] < 0)
      {
        res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 's':
    case 'S':
    case 'F': {
      int err = 0;
      char *ptr;
      if (args->mode & EXCLUSIVE_MODES) {
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
      if (!ptr || ptr != strchr(optarg, ','))
        err = 1;
      else {
        *ptr = '\0';
        ptr++;
        args->solve_boundary_filename = optarg;
        err = (RES_OK != cstr_to_double(ptr, args->u.probe + 3) || args->u.probe[3] < 0);
      }
      if (err) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;
    }

    case 'm': {
      char* ptr;
      if (args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= MEDIUM_COMPUTE;
      ptr = strpbrk(optarg, ",");
      if (!ptr || ptr == optarg)
        res = RES_BAD_ARG;
      else res = cstr_to_double(ptr + 1, args->u.probe + 3);
      if (res != RES_OK) {
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      *ptr = '\0';
      args->medium_name = optarg;
      break;
    }

    case 'd':
      if (args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= DUMP_VTK;
      break;

    case 'D':
      if (0 == strcmp(optarg, "all")) {
        args->dump_paths = DUMP_ALL;
      }
      else if (0 == strcmp(optarg, "error")) {
        args->dump_paths = DUMP_ERROR;
      }
      else if (0 == strcmp(optarg, "success")) {
        args->dump_paths = DUMP_SUCCESS;
      } else {
        res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 'f':
      cstr_to_double(optarg, &args->scale_factor);
      if (res != RES_OK
        || args->scale_factor <= 0) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 'r':
      cstr_to_list_double(optarg, ',', args->radiative_temp, &len, 2);
      if (res != RES_OK
        || len != 2
        || args->radiative_temp[0] <= 0
        || args->radiative_temp[1] <= 0) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Invalid argument for option -%c: %s\n", opt, optarg);
        goto error;
      }
      break;

    case 'R':
      if (args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        fprintf(stderr, "Modes -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= IR_COMPUTE;
      args->camera = optarg;
      break;
    }
  }
  
  if (args->mode & GREEN_MODE
    && (args->mode != (args->mode & (GREEN_MODE | GREEN_COMPATIBLE_MODES))))
  {
    fprintf(stderr, "Option -g can only be used in conjunction with:");
    print_multiple_modes(stderr, GREEN_COMPATIBLE_MODES);
    fprintf(stderr, "\n");
    res = RES_BAD_ARG;
    goto error;
  }

  if (!args->medium_filename) {
    fprintf(stderr, "Missing mandatory argument: -M MEDIUM.txt\n");
    res = RES_BAD_ARG;
    goto error;
  }

  if (!args->bc_filename) {
    fprintf(stderr, "Missing mandatory argument: -B BOUNDARY.txt\n");
    res = RES_BAD_ARG;
    goto error;
  }

  if (args->mode & IR_COMPUTE && n_used) {
    fprintf(stderr, "The -n option has no effect in rendering mode;"
      " use rendering's SPP suboption instead.\n");
    res = RES_BAD_ARG;
    goto error;
  }

exit:
  return res;
error:
  fprintf(stderr, "Use the -h option to get help.\n");
  goto exit;
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

  if (line)
  {
    int i = 0;
    for (i = 0; *(line + i); i++)
    {
      size_t len = 0;
      opt = split_line(line[i], '=');
      _strupr(opt[0]);
      if (strcmp(opt[0], "T") == 0) {
        res = cstr_to_double(opt[1], &cam->u.time);
        if (res != RES_OK) goto error;
      }
      else if (strcmp(opt[0], "FOV") == 0) {
        res = cstr_to_double(opt[1], &cam->fov);
        if (res != RES_OK) goto error;
      }
      else if (strcmp(opt[0], "UP") == 0) {
        res = cstr_to_list_double(opt[1], ',', cam->up, &len, 3);
        if (res != RES_OK || len != 3) goto error;
      }
      else if (strcmp(opt[0], "TGT") == 0) {
        res = cstr_to_list_double(opt[1], ',', cam->tgt, &len, 3);
        if (res != RES_OK || len != 3) goto error;
      }
      else if (strcmp(opt[0], "POS") == 0) {
        res = cstr_to_list_double(opt[1], ',', cam->pos, &len, 3);
        if (res != RES_OK || len != 3) goto error;
      }
      else if (strcmp(opt[0], "IMG") == 0) {
        res = cstr_to_list_uint(opt[1], 'x', cam->img, &len, 2);
        if (res != RES_OK || len != 2) goto error;
      }
      else if (strcmp(opt[0], "SPP") == 0) {
        res = cstr_to_uint(opt[1], &cam->spp);
        if (res != RES_OK) goto error;
      } else {
        fprintf(stderr, "Warning: unexpected option for rendering mode: %s.\n",
          opt[0]);
        res = RES_BAD_ARG;
        goto error;
      }
    }
  }

exit:
  FREE_AARRAY(line)
    FREE_AARRAY(opt)

    return res;
error:
  fprintf(stderr, "Use the -h option to get help.\n");
  goto exit;
}

/* Read medium line; should be one of:
 * SOLID medium_name STL_filename lambda rho cp delta "Tinit(x,y,z)" [ "volumic_power(x,y,z,t)" ]
 * FLUID medium_name STL_filename rho cp "Tinit(x,y,z)"
*/
res_T
parse_medium_line
  (char* line,
   char** stl_filename,
   struct description* desc)
{
  char* tk = NULL;
  res_T res = RES_OK;

  ASSERT(line && stl_filename && desc);

#define CHK_TOK(x, Name) if ((tk = (x)) == NULL) {\
   fprintf(stderr, "Invalid data (missing token " Name ")\n");\
   res = RES_BAD_ARG;\
   goto exit;\
  } (void)0
  CHK_TOK(_strupr(strtok(line, " ")), "medium type");
  if (strcmp(tk, "SOLID") == 0) {
    desc->type = DESC_MAT_SOLID;
    desc->d.solid = NULL_SOLID;

    CHK_TOK(strtok(NULL, " "), "medium name");
    if (strlen(tk) >= sizeof(desc->d.solid.name)) {
      fprintf(stderr, "Medium name is too long: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    strncpy(desc->d.solid.name, tk, sizeof(desc->d.solid.name));

    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    CHK_TOK(strtok(NULL, " "), "lambda");
    res = cstr_to_double(tk, &desc->d.solid.lambda);
    if (res != RES_OK || desc->d.solid.lambda <= 0) {
      fprintf(stderr, "Invalid lambda: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "rho");
    res = cstr_to_double(tk, &desc->d.solid.rho);
    if (res != RES_OK || desc->d.solid.rho <= 0) {
      fprintf(stderr, "Invalid rho: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "cp");
    res = cstr_to_double(tk, &desc->d.solid.cp);
    if (res != RES_OK || desc->d.solid.cp <= 0) {
      fprintf(stderr, "Invalid cp: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "delta");
    res = cstr_to_double(tk, &desc->d.solid.delta);
    if (res != RES_OK || desc->d.solid.delta <= 0) {
      fprintf(stderr, "Invalid delta: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    /* Depending of the number of spaces before '"' strtok should
     * be called once or twice */
    CHK_TOK(strtok(NULL, "\""), "Tinit");
    if (*(tk + strspn(tk, " \t")) == '\0') {
      CHK_TOK(strtok(NULL, "\""), "Tinit");
    }
    desc->d.solid.Tinit = malloc(strlen(tk) + 1);
    strcpy(desc->d.solid.Tinit, tk);
    /* Closing " fot Tinit; can return NULL if line ends just after "Tinit" */
    tk = strtok(NULL, "\"");

    /* Volumic Power is optional */
    if (tk && *(tk + strspn(tk, " \t")) == '\0') {
      /* Depending of the number of spaces before '"' strtok should
       * be called once or twice */
      tk = strtok(NULL, "\"");
    }
    if (tk) {
      desc->d.solid.power = malloc(strlen(tk) + 1);
      strcpy(desc->d.solid.power, tk);
      /* Can be changed aftwerwards if compiled to constant 0  */
      desc->d.solid.has_power = 1;
    }
  }
  else if (strcmp(tk, "FLUID") == 0) {
    desc->type = DESC_MAT_FLUID;
    desc->d.fluid = NULL_FLUID;

    CHK_TOK(strtok(NULL, " "), "medium name");
    if (strlen(tk) >= sizeof(desc->d.fluid.name)) {
      fprintf(stderr, "Medium name is too long: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    strncpy(desc->d.fluid.name, tk, sizeof(desc->d.fluid.name));

    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    CHK_TOK(strtok(NULL, " "), "rho");
    res = cstr_to_double(tk, &desc->d.fluid.rho);
    if (res != RES_OK || desc->d.fluid.rho <= 0) {
      fprintf(stderr, "Invalid rho: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "cp");
    res = cstr_to_double(tk, &desc->d.fluid.cp);
    if (res != RES_OK || desc->d.fluid.cp <= 0) {
      fprintf(stderr, "Invalid cp: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    /* Depending of the number of spaces before '"' strtok should
     * be called once or twice */
    CHK_TOK(strtok(NULL, "\""), "Tinit");
    if (*(tk + strspn(tk, " \t")) == '\0') {
      CHK_TOK(strtok(NULL, "\""), "Tinit");
    }
    desc->d.fluid.Tinit = malloc(strlen(tk) + 1);
    strcpy(desc->d.fluid.Tinit, tk);
  } else {
    res = RES_BAD_ARG;
    fprintf(stderr, "Invalid medium type: %s\n", tk);
  }

#undef CHK_TOK
  exit :
       return res;
}

/* Read boundary line; should be one of:
#   H_BOUNDARY_FOR_SOLID Boundary_name STL_filename emissivity specular_fraction hc hc_max "T_env(x,y,z,t)"
# | H_BOUNDARY_FOR_FLUID Boundary_name STL_filename emissivity specular_fraction hc hc_max "T_env(x,y,z,t)"
# | T_BOUNDARY_FOR_SOLID Boundary_name STL_filename "T_value(x,y,z,t)"
# | T_BOUNDARY_FOR_FLUID Boundary_name STL_filename "T_value(x,y,z,t)" hc hc_max
# | F_BOUNDARY_FOR_SOLID Boundary_name STL_filename "F_value(x,y,z,t)"
# ; no F_BOUNDARY_FOR_FLUID
# | SOLID_FLUID_CONNECTION Connection_name STL_filename emissivity specular_fraction hc hc_max
 */
res_T
parse_boundary_line
  (char* line,
   char** stl_filename,
   struct description* desc)
{
  char* tk = NULL;
  int h_solid = 0, h_fluid = 0;
  int t_solid = 0, t_fluid = 0;
  int f_solid = 0;
  int sf_connect = 0;
  res_T res = RES_OK;

  ASSERT(line && stl_filename && desc);

#define CHK_TOK(x, Name) if ((tk = (x)) == NULL) {\
   fprintf(stderr, "Invalid data (missing token " Name ")\n");\
   res = RES_BAD_ARG;\
   goto exit;\
  } (void)0
  CHK_TOK(strtok(line, " "), "boundary type");
  h_solid = (0 == strcmp(tk, "H_BOUNDARY_FOR_SOLID"));
  if (!h_solid) {
    h_fluid = (0 == strcmp(tk, "H_BOUNDARY_FOR_FLUID"));
    if (!h_fluid) {
      t_solid = (0 == strcmp(tk, "T_BOUNDARY_FOR_SOLID"));
      if (!t_solid) {
        t_fluid = (0 == strcmp(tk, "T_BOUNDARY_FOR_FLUID"));
        if (!t_fluid) {
          f_solid = (0 == strcmp(tk, "F_BOUNDARY_FOR_SOLID"));
          if (!f_solid) {
            sf_connect = (0 == strcmp(tk, "SOLID_FLUID_CONNECTION"));
          }
        }
      }
    }
  }

  if (h_solid || h_fluid) {
    desc->type = h_solid ? DESC_BOUND_H_FOR_SOLID : DESC_BOUND_H_FOR_FLUID;
    desc->d.h_boundary = NULL_HBOUND;

    CHK_TOK(strtok(NULL, " "), "boundary name");
    if (strlen(tk) >= sizeof(desc->d.h_boundary.name)) {
      fprintf(stderr, "Boundary name is too long: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    strncpy(desc->d.h_boundary.name, tk, sizeof(desc->d.h_boundary.name));

    /* The description is parsed the same way for both types */
    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    CHK_TOK(strtok(NULL, " "), "emissivity");
    res = cstr_to_double(tk, &desc->d.h_boundary.emissivity);
    if (res != RES_OK ||
      desc->d.h_boundary.emissivity < 0 || desc->d.h_boundary.emissivity > 1) {
      fprintf(stderr, "Invalid emissivity: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    desc->d.h_boundary.has_emissivity = (desc->d.h_boundary.emissivity > 0);
    CHK_TOK(strtok(NULL, " "), "specular fraction");
    res = cstr_to_double(tk, &desc->d.h_boundary.specular_fraction);
    if (res != RES_OK
      || desc->d.h_boundary.specular_fraction < 0.0
      || desc->d.h_boundary.specular_fraction > 1.0) {
      fprintf(stderr, "Invalid specular_fraction: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "hc");
    res = cstr_to_double(tk, &desc->d.h_boundary.hc);
    if (res != RES_OK || desc->d.h_boundary.hc < 0) {
      fprintf(stderr, "Invalid hc value: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    desc->d.h_boundary.has_hc = (desc->d.h_boundary.hc > 0);
    CHK_TOK(strtok(NULL, " "), "hc max");
    res = cstr_to_double(tk, &desc->d.h_boundary.hc_max);
    if (res != RES_OK
      || desc->d.h_boundary.hc_max < desc->d.h_boundary.hc
      || desc->d.h_boundary.hc_max < 0) {
      fprintf(stderr, "Invalid hc_max value: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    /* Depending of the number of spaces before '"' strtok should
     * be called once or twice */
    CHK_TOK(strtok(NULL, "\""), "temperature");
    if (*(tk + strspn(tk, " \t")) == '\0') {
      CHK_TOK(strtok(NULL, "\""), "temperature");
    }
    desc->d.h_boundary.T = malloc(strlen(tk) + 1);
    strcpy(desc->d.h_boundary.T, tk);
  }
  else if (t_solid || t_fluid) {
    desc->type = t_solid ? DESC_BOUND_T_FOR_SOLID : DESC_BOUND_T_FOR_FLUID;
    desc->d.t_boundary = NULL_TBOUND;

    CHK_TOK(strtok(NULL, " "), "boundary name");
    if (strlen(tk) >= sizeof(desc->d.t_boundary.name)) {
      fprintf(stderr, "Boundary name is too long: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    strncpy(desc->d.t_boundary.name, tk, sizeof(desc->d.t_boundary.name));

    /* This part of the description is parsed the same way for both types */
    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    /* Depending of the number of spaces before '"' strtok should
     * be called once or twice */
    CHK_TOK(strtok(NULL, "\""), "temperature");
    if (*(tk + strspn(tk, " \t")) == '\0') {
      CHK_TOK(strtok(NULL, "\""), "temperature");
    }
    desc->d.t_boundary.T = malloc(strlen(tk) + 1);
    strcpy(desc->d.t_boundary.T, tk);

    /* Parse hc + hc_max only if fluid */
    if (t_fluid) {
      CHK_TOK(strtok(NULL, " "), "hc");
      res = cstr_to_double(tk, &desc->d.t_boundary.hc);
      if (res != RES_OK || desc->d.t_boundary.hc < 0) {
        fprintf(stderr, "Invalid hc value: %s\n", tk);
        res = RES_BAD_ARG;
        goto exit;
      }
      desc->d.t_boundary.has_hc = (desc->d.t_boundary.hc > 0);
      CHK_TOK(strtok(NULL, " "), "hc max");
      res = cstr_to_double(tk, &desc->d.t_boundary.hc_max);
      if (res != RES_OK
        || desc->d.t_boundary.hc_max < desc->d.t_boundary.hc
        || desc->d.t_boundary.hc_max < 0) {
        fprintf(stderr, "Invalid hc_max value: %s\n", tk);
        res = RES_BAD_ARG;
        goto exit;
      }
    }
    else desc->d.t_boundary.has_hc = 0;
  }
  else if (f_solid) {
    desc->type = DESC_BOUND_F_FOR_SOLID;
    desc->d.f_boundary = NULL_FBOUND;

    CHK_TOK(strtok(NULL, " "), "boundary name");
    if (strlen(tk) >= sizeof(desc->d.f_boundary.name)) {
      fprintf(stderr, "Boundary name is too long: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    strncpy(desc->d.f_boundary.name, tk, sizeof(desc->d.f_boundary.name));

    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    /* Depending of the number of spaces before '"' strtok should
     * be called once or twice */
    CHK_TOK(strtok(NULL, "\""), "flux");
    if (*(tk + strspn(tk, " \t")) == '\0') {
      CHK_TOK(strtok(NULL, "\""), "flux");
    }
    desc->d.f_boundary.flux = malloc(strlen(tk) + 1);
    strcpy(desc->d.f_boundary.flux, tk);
  }
  else if (sf_connect) {
    desc->type = DESC_SOLID_FLUID_CONNECT;
    desc->d.sf_connect = NULL_SFCONNECT;

    CHK_TOK(strtok(NULL, " "), "boundary name");
    if (strlen(tk) >= sizeof(desc->d.sf_connect.name)) {
      fprintf(stderr, "Boundary name is too long: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    strncpy(desc->d.sf_connect.name, tk, sizeof(desc->d.sf_connect.name));

    CHK_TOK(strtok(NULL, " "), "file name");
    *stl_filename = malloc(strlen(tk) + 1);
    strcpy(*stl_filename, tk);

    CHK_TOK(strtok(NULL, " "), "emissivity");
    res = cstr_to_double(tk, &desc->d.sf_connect.emissivity);
    if (res != RES_OK ||
      desc->d.sf_connect.emissivity < 0 || desc->d.sf_connect.emissivity > 1) {
      fprintf(stderr, "Invalid emissivity: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    desc->d.sf_connect.has_emissivity = (desc->d.sf_connect.emissivity > 0);
    CHK_TOK(strtok(NULL, " "), "specular fraction");
    res = cstr_to_double(tk, &desc->d.sf_connect.specular_fraction);
    if (res != RES_OK
      || desc->d.sf_connect.specular_fraction < 0.0
      || desc->d.sf_connect.specular_fraction > 1.0) {
      fprintf(stderr, "Invalid specular_fraction: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    CHK_TOK(strtok(NULL, " "), "hc");
    res = cstr_to_double(tk, &desc->d.sf_connect.hc);
    if (res != RES_OK || desc->d.sf_connect.hc < 0) {
      fprintf(stderr, "Invalid hc value: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
    desc->d.sf_connect.has_hc = (desc->d.sf_connect.hc > 0);
    CHK_TOK(strtok(NULL, " "), "hc max");
    res = cstr_to_double(tk, &desc->d.sf_connect.hc_max);
    if (res != RES_OK
      || desc->d.sf_connect.hc_max < desc->d.sf_connect.hc
      || desc->d.sf_connect.hc_max < 0) {
      fprintf(stderr, "Invalid hc_max value: %s\n", tk);
      res = RES_BAD_ARG;
      goto exit;
    }
  } else {
    fprintf(stderr, "Invalid boundary type: %s", tk);
    res = RES_BAD_ARG;
    goto exit;
  }

#undef CHK_TOK
  exit :
       return res;
}
