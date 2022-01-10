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

#define _POSIX_C_SOURCE 200809L /* strdup */
#include "stardis-args.h"
#include "stardis-parsing.h"
#include "stardis-app.h"
#include "stardis-default.h"
#include "stardis-version.h"

#include <rsys/cstr.h>
#include <rsys/double2.h>
#include <rsys/double3.h>
#include <sdis_version.h>
#include <rsys/logger.h>

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef COMPILER_GCC
#include <strings.h> /* strcasecmp */
#else
#define strcasecmp(s1, s2) _stricmp((s1), (s2))
#endif

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

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

static char
mode_option
  (const int m)
{
  int found = 0;
  char res = '?';
  if(m & MODE_DUMP_C_CHUNKS) { found++; res = 'c'; }
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

static void
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

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

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
  /* Set non-zero default values */
  args->samples = STARDIS_DEFAULT_SAMPLES_COUNT;
  args->nthreads = SDIS_NTHREADS_DEFAULT;
  args->picard_order = STARDIS_DEFAULT_PICARD_ORDER;
  d2(args->pos_and_time+3,
    STARDIS_DEFAULT_COMPUTE_TIME, STARDIS_DEFAULT_COMPUTE_TIME);
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

  fprintf(stream, "\nMandatory options\n");
  fprintf(stream,   "-----------------\n");

  fprintf(stream, "\n  -M <FILE>\n");
  fprintf(stream, "     Read a text file that contains (partial) description of the model.\n");

  fprintf(stream, "\nExclusive options\n");
  fprintf(stream,   "-----------------\n");

  fprintf(stream, "\n  -F STL_FILE[,TIME-RANGE]\n");
  fprintf(stream, "     Compute the mean flux on a given 2D region at a given time.\n");

  fprintf(stream, "\n  -m MEDIUM_NAME[,TIME-RANGE]\n");
  fprintf(stream, "     Compute the mean temperature in a given medium at a given time.\n");

  fprintf(stream, "\n  -p X,Y,Z[,TIME-RANGE]\n");
  fprintf(stream, "     Compute the temperature at the given probe.\n");

  fprintf(stream, "\n  -P X,Y,Z[,TIME-RANGE]\n");
  fprintf(stream, "     Compute the temperature at the given probe on an interface.\n");

  fprintf(stream, "\n  -R [RENDERING_OPTIONS]\n");
  fprintf(stream, "     Compute an infra-red image of the model.\n");

  fprintf(stream, "\n  -s STL_FILE[,TIME-RANGE]\n");
  fprintf(stream, "     Compute the mean temperature on a given 2D region.\n");

  fprintf(stream, "\n  -S STL_FILE[,TIME-RANGE]\n");
  fprintf(stream, "     Compute the by-triangle mean temperature on a given 2D region.\n");

  fprintf(stream, "\nOther options\n");
  fprintf(stream,   "-------------\n");

  fprintf(stream, "\n  -c NAMES_PREFIX\n");
  fprintf(stream, "     Dump the geometry and property ids to stdout as C chunks.\n");

  fprintf(stream, "\n  -d\n");
  fprintf(stream, "     Dump the geometry to stdout in VTK format along with various properties.\n");

  fprintf(stream, "\n  -D TYPE,FILE_NAMES_PREFIX\n");
  fprintf(stream, "     Write thermal paths of the given TYPE in VTK format.\n");

  fprintf(stream, "\n  -e\n");
  fprintf(stream, "     Use extended format to output Monte-Carlo results.\n");

  fprintf(stream, "\n  -g\n");
  fprintf(stream, "     Change the computation to produce the green function.\n");

  fprintf(stream, "\n  -G BIN_FILE_NAME[,CSV_FILE_NAME]\n");
  fprintf(stream, "     Change the computation to produce the green function and possibly end of paths information.\n");

  fprintf(stream, "\n  -h\n");
  fprintf(stream, "     Print this help and exit.\n");

  fprintf(stream, "\n  -n SAMPLE_COUNT\n");
  fprintf(stream, "     Set the number of Monte-Carlo samples.\n");

  fprintf(stream, "\n  -o\n");
  fprintf(stream, "     Set the order for the Picard linearization of radiative transfer.\n");

  fprintf(stream, "\n  -t NUM_OF_THREADS\n");
  fprintf(stream, "     Hint on the number of threads.\n");

  fprintf(stream, "\n  -v\n");
  fprintf(stream, "     Print version information and exit.\n");

  fprintf(stream, "\n  -V LEVEL\n");
  fprintf(stream, "     Set the verbosity level.\n");

  fprintf(stream, "\n  -x <FILE>\n");
  fprintf(stream, "     Use a random generator's state read from a file.\n");

  fprintf(stream, "\n  -X <FILE>\n");
  fprintf(stream, "     Save the final random generator's state in a file.\n");

  fprintf(stream,
"\nCopyright (C) 2018-2021 |Meso|Star> <contact@meso-star.com>.\n"
"stardis is free software released under the GNU GPL license, version 3 or later.\n"
"You are free to change or redistribute it under certain conditions\n"
"<http://gnu.org/licenses/gpl.html>.\n");
}

#define FREE_AARRAY(ARRAY) \
if(ARRAY) {\
  int i__ = 0; \
  for(i__=0; *((ARRAY)+i__);i__++){\
    free((ARRAY)[i__]);\
  }\
  free(ARRAY);\
  (ARRAY) = NULL;\
}

/* Workaround for a gcc warning when GET_OPTIONAL_TIME_RANGE used with Rank=0 */
static FINLINE int is_less(size_t a, size_t b) { return a < b; }

/* Get a time range from a coma-separated list of doubles
 * The first Rank values are mandatory, followed by an optional time range
 * that can be a single time */
#define GET_OPTIONAL_TIME_RANGE(Src, Rank, Dst, Logger, OptionString, Option, FullSrc) \
  res = cstr_to_list_double((Src), ',', (Dst), &len, (Rank)+2); \
  if(res != RES_OK \
     || is_less(len, (Rank)) \
     || (len == (Rank)+1 && (Dst)[(Rank)] < 0) \
     || (len == (Rank)+2  && ((Dst)[0] < 0 || (Dst)[(Rank)] > (Dst)[(Rank)+1])) \
     || len > (Rank)+2) \
  { \
    if(res == RES_OK) res = RES_BAD_ARG; \
      logger_print((Logger), LOG_ERROR, \
      "Invalid argument for option "OptionString": %s\n", \
      (Option), (FullSrc)); \
    goto error; \
  } else { \
    if(len == (Rank)+1) (Dst)[(Rank)+1] = (Dst)[(Rank)];\
  }

/* Get a string followed by an optional time range */
#define GET_STR_AND_OPTIONAL_TIME_RANGE(Str, Time) \
  ptr = strchr(optarg, ','); /* First ',' */ \
  if(ptr) { /* Time range provided */ \
    GET_OPTIONAL_TIME_RANGE(ptr+1, 0, (Time), args->logger, "-%c", opt, optarg); \
    *ptr = '\0'; \
  } \
  (Str) = optarg;

/* Get a position followed by an optional time range */
#define GET_POS_AND_OPTIONAL_TIME_RANGE(Src, Dst, FullSrc) \
  GET_OPTIONAL_TIME_RANGE((Src), 3, (Dst), args->logger, "-%c", opt, (FullSrc));

res_T
parse_args
  (const int argc,
   char** argv,
   struct args* args,
   struct mem_allocator* allocator)
{
  int opt = 0, n_used = 0, o_used = 0;
  size_t len = 0;
  const char option_list[] = "c:dD:eF:gG:hm:M:n:o:p:P:R:s:S:t:vV:x:X:";
  char buf[128];
  struct str keep;
  char** line = NULL;
  res_T res = RES_OK;

  ASSERT(argv && args);

  str_init(allocator, &keep);
  opterr = 0; /* No default error messages */
  while((opt = getopt(argc, argv, option_list)) != -1) {
    switch (opt) {

    case '?': /* Unreconised option */
    {
      char* ptr = strchr(option_list, optopt);
      res = RES_BAD_ARG;
      if(ptr && ptr[1] == ':') {
        logger_print(args->logger, LOG_ERROR,
          "Missing argument for option -%c\n",
          optopt);
      } else {
        logger_print(args->logger, LOG_ERROR, "Invalid option -%c\n", optopt);
      }
      goto error;
    }

    case 'c':
      if(args->mode & USE_STDOUT_MODES) {
        res = RES_BAD_ARG;
        print_multiple_modes(buf, sizeof(buf), USE_STDOUT_MODES, MODE_DUMP_C_CHUNKS);
        logger_print(args->logger, LOG_ERROR,
          "Option -%c cannot be used in conjunction with other dump options (%s).\n",
          (char)opt, buf);
        goto error;
      }
      args->chunks_prefix = optarg;
      args->mode |= MODE_DUMP_C_CHUNKS;
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

    case 'D': {
      char* ptr = strrchr(optarg, ',');
      if(!ptr || ptr != strchr(optarg, ','))
        res = RES_BAD_ARG; /* Single ',' expected */
      else {
        args->paths_filename = ptr + 1;
        *ptr = '\0';
      }
      if(res == RES_OK) {
        if(0 == strcasecmp(optarg, "all")) {
          args->dump_paths = DUMP_ALL;
        }
        else if(0 == strcasecmp(optarg, "error")) {
          args->dump_paths = DUMP_ERROR;
        }
        else if(0 == strcasecmp(optarg, "success")) {
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

    case 'e':
      args->mode |= MODE_EXTENDED_RESULTS;
      break;

    /*case 'F': see 's' */

    case 'g':
      if(args->mode & MODE_BIN_GREEN) {
        res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Options -%c and -%c are exclusive.\n",
          (char)opt, mode_option(MODE_BIN_GREEN));
        goto error;
      }
      args->mode |= MODE_GREEN;
      break;

    case 'G': {
      char* ptr = strrchr(optarg, ',');
      if(ptr && ptr != strchr(optarg, ','))
        res = RES_BAD_ARG; /* Expecting 1 or 0 ',' */
      if(args->mode & (MODE_BIN_GREEN | MODE_GREEN)) {
        res = RES_BAD_ARG;
        if(args->mode & MODE_BIN_GREEN)
          logger_print(args->logger, LOG_ERROR,
            "Option -%c cannot be used twice.\n",
            (char)opt);
        else
          logger_print(args->logger, LOG_ERROR,
            "Options -%c and -%c are exclusive.\n",
            (char)opt, mode_option(MODE_GREEN));
        goto error;
      }
      args->mode |= MODE_BIN_GREEN;
      if(ptr) {
        args->end_paths_filename = ptr + 1;
        *ptr = '\0';
      }
      args->bin_green_filename = optarg;
      break;
    }

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
          "Options -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= MODE_MEDIUM_COMPUTE;
      GET_STR_AND_OPTIONAL_TIME_RANGE(args->medium_name, args->pos_and_time + 3);
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
      long n;
      res = cstr_to_long(optarg, &n);
      if(res != RES_OK
        || n <= 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      args->samples = (unsigned long)n;
      n_used = 1;
      break;
    }

    case 'o': {
      int order;
      res = cstr_to_int(optarg, &order);
      if(res != RES_OK
        || order <= 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      args->picard_order = (unsigned)order;
      o_used = 1;
      break;
    }

    case 'p':
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Options -%c and -%c are exclusive.\n",
          (char)opt, mode_option(args->mode));
        goto error;
      }
      args->mode |= MODE_PROBE_COMPUTE;
      GET_POS_AND_OPTIONAL_TIME_RANGE(optarg, args->pos_and_time, optarg);
      break;

    case 'P':
        if(args->mode & EXCLUSIVE_MODES) {
          res = RES_BAD_ARG;
          logger_print(args->logger, LOG_ERROR,
              "Options -%c and -%c are exclusive.\n",
              (char)opt, mode_option(args->mode));
          goto error;
        }
        args->mode |= MODE_PROBE_COMPUTE_ON_INTERFACE;

        ERR(str_set(&keep, optarg));
        line = split_line(optarg, ':');
        if(!line) {
          res = RES_MEM_ERR;
          str_release(&keep);
          goto error;
        }

        /* We expect 1 or 2 parts in line */
        if(!line[0] || (line[1] && line[2])) {
          logger_print((args->logger), LOG_ERROR,
            "Invalid argument for option ""-%c"": %s\n",
            opt, str_cget(&keep));
          str_release(&keep);
          res = RES_BAD_ARG;
          goto error;
        }

        /* First part is pos and optional time, optional second part is a
         * medium name (OK if NULL) */
        GET_POS_AND_OPTIONAL_TIME_RANGE(line[0], args->pos_and_time,
          str_cget(&keep));
        if(line[1])
          args->medium_name = optarg + strlen(line[0]) + 1;

        break;

    case 'R':
      if(args->mode & EXCLUSIVE_MODES) {
        res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Options -%c and -%c are exclusive.\n",
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
          "Options -%c and -%c are exclusive.\n",
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
      GET_STR_AND_OPTIONAL_TIME_RANGE(args->solve_filename, args->pos_and_time + 3);
      break;
    }

    case 't': {
      int nt;
      res = cstr_to_int(optarg, &nt);
      if(res != RES_OK
        || args->nthreads <= 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print(args->logger, LOG_ERROR,
          "Invalid argument for option -%c: %s\n",
          opt, optarg);
        goto error;
      }
      args->nthreads = (unsigned)nt;
      break;
    }

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

    case 'x':
      if(!(args->mode & RANDOM_RW_MODES)) {
        res = RES_BAD_ARG;
        print_multiple_modes(buf, sizeof(buf), RANDOM_RW_MODES, 0);
        logger_print(args->logger, LOG_ERROR,
          "Option -%c can only be used in conjunction with one of the following options: %s.\n",
          (char)opt, buf);
        goto error;
      }
      args->rndgen_state_in_filename = optarg;
      break;

    case 'X':
      if(!(args->mode & RANDOM_RW_MODES)) {
        res = RES_BAD_ARG;
        print_multiple_modes(buf, sizeof(buf), RANDOM_RW_MODES, 0);
        logger_print(args->logger, LOG_ERROR,
          "Option -%c can only be used in conjunction with one of the following options: %s.\n",
          (char)opt, buf);
        goto error;
      }
      args->rndgen_state_out_filename = optarg;
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
      "Option -%c can only be used in conjunction with: %s\n",
      mode_option(args->mode & (MODE_BIN_GREEN | MODE_GREEN)), buf);
    res = RES_BAD_ARG;
    goto error;
  }

  if(args->mode & (MODE_BIN_GREEN | MODE_GREEN)
    && o_used && args->picard_order > 1)
  {
    logger_print(args->logger, LOG_ERROR,
      "Option -%c cannot be used if Picard order is not 1 (here order is %u)\n",
      mode_option(args->mode & (MODE_BIN_GREEN | MODE_GREEN)),
        args->picard_order);
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
    if(!(args->mode & EXT_COMPATIBLE_MODES)) {
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

  if(args->rndgen_state_in_filename && !(args->mode & COMPUTE_MODES)) {
    res = RES_BAD_ARG;
    print_multiple_modes(buf, sizeof(buf), COMPUTE_MODES, 0);
    logger_print(args->logger, LOG_ERROR,
      "Option -x can only be used in conjunction with an option"
      " that launch a MC computation (%s).\n",
      buf);
    goto error;
  }

  if(args->rndgen_state_out_filename && !(args->mode & COMPUTE_MODES)) {
    res = RES_BAD_ARG;
    print_multiple_modes(buf, sizeof(buf), COMPUTE_MODES, 0);
    logger_print(args->logger, LOG_ERROR,
      "Option -X can only be used in conjunction with an option"
      " that launch a MC computation (%s).\n",
      buf);
    goto error;
  }

end:
  FREE_AARRAY(line);
  str_release(&keep);
  return res;
error:
  logger_print(args->logger, LOG_ERROR, "Use option -h to print help.\n");
  goto end;
}

res_T
parse_camera
  (struct logger* logger,
   char* cam_param,
   struct stardis* stardis)
{
  char** line = NULL;
  char** opt = NULL;
  struct camera* cam;
  struct str keep;
  int i = 0;
  res_T res = RES_OK;

  ASSERT(cam_param && stardis);
  cam = &stardis->camera;
  line = split_line(cam_param, ':');
  if(!line) {
    res = RES_MEM_ERR;
    goto error;
  }

  str_init(stardis->allocator, &keep);
  for(i = 0; *(line + i); i++) {
    size_t len = 0;
    ERR(str_set(&keep, line[i]));
    opt = split_line(line[i], '=');
    if(!opt[0] || !opt[1] || opt[2]) { /* We expect 2 parts */
      if(res == RES_OK) res = RES_BAD_ARG;
      logger_print((logger), LOG_ERROR,
        "Invalid option syntax: %s\n", str_cget(&keep));
      goto error;
    }
    if(strcasecmp(opt[0], "T") == 0) {
      GET_OPTIONAL_TIME_RANGE(opt[1], 0, cam->time_range, logger, "%s", opt[0],
        str_cget(&keep));
    }
    else if(strcasecmp(opt[0], "FILE") == 0) {
      ERR(str_set(&cam->file_name, opt[1]));
    }
    else if(strcasecmp(opt[0], "FMT") == 0) {
      if(strcasecmp(opt[1], "VTK") == 0)
        cam->fmt = STARDIS_RENDERING_OUTPUT_FILE_FMT_VTK;
      else if(strcasecmp(opt[1], "HT") == 0)
        cam->fmt = STARDIS_RENDERING_OUTPUT_FILE_FMT_HT;
      else {
        logger_print(logger, LOG_ERROR,
          "Unexpected value for rendering option %s: %s.\n",
          opt[0], opt[1]);
        res = RES_BAD_ARG;
        goto error;
      }
    }
    else if(strcasecmp(opt[0], "FOV") == 0) {
      res = cstr_to_double(opt[1], &cam->fov);
      if(res != RES_OK
        || cam->fov <= 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print((logger), LOG_ERROR,
          "Invalid %s option: %s\n", opt[0], opt[1]);
        goto error;
      }
    }
    else if(strcasecmp(opt[0], "UP") == 0) {
      ERR(cstr_to_list_double(opt[1], ',', cam->up, &len, 3));
    }
    else if(strcasecmp(opt[0], "TGT") == 0) {
      ERR(cstr_to_list_double(opt[1], ',', cam->tgt, &len, 3));
      cam->auto_look_at = 0;
    }
    else if(strcasecmp(opt[0], "POS") == 0) {
      ERR(cstr_to_list_double(opt[1], ',', cam->pos, &len, 3));
      cam->auto_look_at = 0;
    }
    else if(strcasecmp(opt[0], "IMG") == 0) {
      unsigned img_sz[2];
      res = cstr_to_list_uint(opt[1], 'x', img_sz, &len, 2);
      if(res != RES_OK
        /* mimic cstr_to_list_int() possible behaviour; but it doesnt exist */
        || img_sz[0] == 0 || img_sz[0] > INT_MAX
        || img_sz[1] == 0 || img_sz[1] > INT_MAX)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print((logger), LOG_ERROR,
          "Invalid %s option: %s\n", opt[0], opt[1]);
        goto error;
      }
      cam->img_width = img_sz[0];
      cam->img_height = img_sz[1];
    }
    else if(strcasecmp(opt[0], "SPP") == 0) {
      int ssp;
      res = cstr_to_int(opt[1], &ssp);
      if(res != RES_OK
        || ssp <= 0)
      {
        if(res == RES_OK) res = RES_BAD_ARG;
        logger_print((logger), LOG_ERROR,
          "Invalid %s option: %s\n", opt[0], opt[1]);
        goto error;
      }
      cam->spp = (unsigned)ssp;
    } else {
      logger_print(logger, LOG_ERROR,
        "Unexpected option for rendering mode: %s.\n",
        opt[0]);
      res = RES_BAD_ARG;
      goto error;
    }
    FREE_AARRAY(opt);
  }

end:
  FREE_AARRAY(line);
  FREE_AARRAY(opt);
#undef FREE_AARRAY

  str_release(&keep);
  return res;
error:
  logger_print(logger, LOG_ERROR, "Error parsing camera options.\n");
  logger_print(logger, LOG_ERROR, "Use the -h option to get help.\n");
  goto end;
}

#undef GET_STR_AND_OPTIONAL_TIME_RANGE
#undef GET_POS_AND_OPTIONAL_TIME_RANGE
#undef GET_OPTIONAL_TIME_RANGE
