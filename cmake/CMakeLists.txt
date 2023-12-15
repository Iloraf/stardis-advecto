# Copyright (C) 2018-2023 |Méso|Star> (contact@meso-star.com)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.0)
project(stardis C)

set(SDIS_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../src)
set(SDIS_SGT_DIR ${PROJECT_SOURCE_DIR}/../stardis-green-types)
set(SDIS_PRG_DIR ${PROJECT_SOURCE_DIR}/../stardis-prog-properties)


option(ENABLE_MPI
  "Enable the support of distributed parallelism \
using the Message Passing Interface specification." OFF)

if(CMAKE_HOST_UNIX)
  set(STARDIS_DOC "TROFF" CACHE STRING
    "Type of documentation to generate and install.")
else()
  set(STARDIS_DOC "HTML" CACHE STRING
    "Type of documentation to generate and install.")
endif()

set_property(CACHE STARDIS_DOC PROPERTY STRINGS
  "HTML"
  "TROFF"
  "TROFF & HTML"
  "NONE")

###############################################################################
# Generate files
###############################################################################
set(STARDIS_ARGS_DEFAULT_TRAD "300")
set(STARDIS_ARGS_DEFAULT_TRAD_REFERENCE "300")
set(STARDIS_ARGS_DEFAULT_COMPUTE_TIME "INF")
set(STARDIS_ARGS_DEFAULT_PICARD_ORDER "1")
set(STARDIS_ARGS_DEFAULT_RENDERING_FOV "70") # degrees
set(STARDIS_ARGS_DEFAULT_RENDERING_IMG_HEIGHT "480")
set(STARDIS_ARGS_DEFAULT_RENDERING_IMG_WIDTH "640")
set(STARDIS_ARGS_DEFAULT_RENDERING_OUTPUT_FILE_FMT "HT") # VTK or HT
set(STARDIS_ARGS_DEFAULT_RENDERING_POS "1, 1, 1")
set(STARDIS_ARGS_DEFAULT_RENDERING_SPP "4")
set(STARDIS_ARGS_DEFAULT_RENDERING_TGT "0, 0, 0")
set(STARDIS_ARGS_DEFAULT_RENDERING_TIME "INF, INF")
set(STARDIS_ARGS_DEFAULT_RENDERING_UP "0, 0, 1")
set(STARDIS_ARGS_DEFAULT_SAMPLES_COUNT "10000")
set(STARDIS_ARGS_DEFAULT_SCALE_FACTOR "1")
set(STARDIS_ARGS_DEFAULT_VERBOSE_LEVEL "1")

configure_file(${SDIS_SOURCE_DIR}/../doc/stardis.1.txt.in
  ${CMAKE_CURRENT_BINARY_DIR}/doc/stardis.1.txt @ONLY)

set(SDIS_VERSION_MAJOR 0)
set(SDIS_VERSION_MINOR 9)
set(SDIS_VERSION_PATCH 0)
set(SDIS_VERSION ${SDIS_VERSION_MAJOR}.${SDIS_VERSION_MINOR}.${SDIS_VERSION_PATCH})

configure_file(${SDIS_SOURCE_DIR}/stardis-default.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/stardis-default.h @ONLY)

configure_file(${SDIS_SOURCE_DIR}/stardis-version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/stardis-version.h @ONLY)

set(STARDIS_GREEN_TYPES_VERSION "4")
set(STARDIS_PROG_PROPERTIES_VERSION "1")

configure_file(${SDIS_SOURCE_DIR}/stardis-green-types.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/stardis-green-types/stardis-green-types.h @ONLY)
configure_file(${SDIS_SOURCE_DIR}/stardis-prog-properties.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/stardis-prog-properties/stardis-prog-properties.h @ONLY)

configure_file(${SDIS_SGT_DIR}/stardis-green-types-config-version.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/stardis-green-types/stardis-green-types-config-version.cmake @ONLY)
configure_file(${SDIS_PRG_DIR}/stardis-prog-properties-config-version.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/stardis-prog-properties/stardis-prog-properties-config-version.cmake @ONLY)

###############################################################################
# Check dependencies
###############################################################################
include(FindPkgConfig)

find_package(RCMake 0.4 REQUIRED)

pkg_check_modules(RSys REQUIRED rsys>=0.14)
pkg_check_modules(Star3D REQUIRED s3d>=0.10)
pkg_check_modules(StarEnc3D REQUIRED senc3d>=0.6)
pkg_check_modules(StarGeom3D REQUIRED sg3d>=0.1)
pkg_check_modules(Stardis REQUIRED sdis>=0.15)
pkg_check_modules(StarSTL REQUIRED sstl>=0.5)
pkg_check_modules(StarSP REQUIRED star-sp>=0.14)

if(MSVC)
  find_package(MuslGetopt REQUIRED)
endif()
if(ENABLE_MPI)
  find_package(MPI 2 REQUIRED)
  set(CMAKE_C_COMPILER ${MPI_C_COMPILER})
  include_directories(${MPI_INCLUDE_PATH})
endif()


include_directories(
  ${StarGeom3D_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/stardis-green-types
  ${CMAKE_CURRENT_BINARY_DIR}/stardis-prog-properties)
if(MSVC)
  include_directories(${MuslGetopt_INCLUDE_DIR})
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${RCMAKE_SOURCE_DIR})
include(rcmake)
include(rcmake_runtime)

###############################################################################
# Build subprojects
###############################################################################
if(NOT STARDIS_DOC STREQUAL "NONE")
  add_subdirectory(doc)
endif()

add_subdirectory(stardis-green-types)
add_subdirectory(stardis-prog-properties)

###############################################################################
# Configure and define targets
###############################################################################
set(SDIS_FILES_SRC
  stardis-app.c
  stardis-args.c
  stardis-compute.c
  stardis-description.c
  stardis-fluid.c
  stardis-fluid-prog.c
  stardis-fbound.c
  stardis-fbound-prog.c
  stardis-hbound.c
  stardis-hbound-prog.c
  stardis-hfbound.c
  stardis-hfbound-prog.c
  stardis-intface.c
  stardis-main.c
  stardis-output.c
  stardis-parsing.c
  stardis-program.c
  stardis-sfconnect.c
  stardis-sfconnect-prog.c
  stardis-ssconnect.c
  stardis-ssconnect-prog.c
  stardis-solid.c
  stardis-solid-prog.c
  stardis-tbound.c
  stardis-tbound-prog.c)

set(SDIS_FILES_INC
  stardis-app.h
  stardis-args.h
  stardis-compute.h
  stardis-description.h
  stardis-default.h.in
  stardis-fluid.h
  stardis-fluid-prog.h
  stardis-fbound.h
  stardis-fbound-prog.h
  stardis-green-types.h.in
  stardis-hbound.h
  stardis-hbound-prog.h
  stardis-hfbound.h
  stardis-hfbound-prog.h
  stardis-intface.h
  stardis-output.h
  stardis-parsing.h
  stardis-program.h
  stardis-prog-properties.h.in
  stardis-sfconnect.h
  stardis-sfconnect-prog.h
  stardis-ssconnect.h
  stardis-ssconnect-prog.h
  stardis-solid.h
  stardis-solid-prog.h
  stardis-tbound.h
  stardis-tbound-prog.h
  stardis-version.h.in)

set(SDIS_FILES_DOC COPYING README.md)

# Prepend each file by `SDIS_SOURCE_DIR'
rcmake_prepend_path(SDIS_FILES_SRC ${SDIS_SOURCE_DIR})
rcmake_prepend_path(SDIS_FILES_INC ${SDIS_SOURCE_DIR})
rcmake_prepend_path(SDIS_FILES_DOC ${PROJECT_SOURCE_DIR}/../)

add_executable(stardis
  ${SDIS_FILES_SRC}
  ${SDIS_FILES_INC})

if(CMAKE_COMPILER_IS_GNUCC)
  set(MATH_LIB m)
elseif(MSVC)
  set(GETOPT_LIB MuslGetopt)
endif()

set_target_properties(stardis
  PROPERTIES VERSION ${SDIS_VERSION})

target_compile_options(stardis PUBLIC
  ${RSys_CFLAGS}
  ${Star3D_CFLAGS}
  ${StarEnc3D_CFLAGS}
  ${StarGeom3D_CFLAGS}
  ${Stardis_CFLAGS}
  ${StarSTL_CFLAGS}
  ${StarSP_CFLAGS})

target_link_libraries(stardis
  ${StarGeom3D_LDFLAGS}
  ${RSys_LDFLAGS}
  ${Star3D_LDFLAGS}
  ${StarEnc3D_LDFLAGS}
  ${StarGeom3D_LDFLAGS}
  ${Stardis_LDFLAGS}
  ${StarSTL_LDFLAGS}
  ${StarSP_LDFLAGS}
  ${GETOPT_LIB}
  ${MATH_LIB})

if(ENABLE_MPI)
  set_target_properties(stardis PROPERTIES COMPILE_DEFINITIONS "STARDIS_ENABLE_MPI")
endif()

###############################################################################
# Define output & install directories
###############################################################################
install(TARGETS stardis
  ARCHIVE DESTINATION bin
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(FILES ${SDIS_FILES_DOC} DESTINATION share/doc/stardis)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/stardis-version.h
	DESTINATION include/stardis)

rcmake_copy_runtime_libraries(stardis)
