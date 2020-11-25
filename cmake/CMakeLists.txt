# Copyright (C) 2018-2020 |Meso|Star>
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
set(STARDIS_ARGS_DEFAULT_AMBIENT_TEMP "300")
set(STARDIS_ARGS_DEFAULT_COMPUTE_TIME "INF, INF")
set(STARDIS_ARGS_DEFAULT_RENDERING_FOV "70") # degrees
set(STARDIS_ARGS_DEFAULT_RENDERING_IMG_HEIGHT "480")
set(STARDIS_ARGS_DEFAULT_RENDERING_IMG_WIDTH "640")
set(STARDIS_ARGS_DEFAULT_RENDERING_OUTPUT_FILE_FMT "HT") # VTK or HT
set(STARDIS_ARGS_DEFAULT_RENDERING_POS "1, 1, 1")
set(STARDIS_ARGS_DEFAULT_RENDERING_SPP "4")
set(STARDIS_ARGS_DEFAULT_RENDERING_TGT "0, 0, 0")
set(STARDIS_ARGS_DEFAULT_RENDERING_TIME "INF, INF")
set(STARDIS_ARGS_DEFAULT_RENDERING_UP "0, 0, 1")
set(STARDIS_ARGS_DEFAULT_REFERENCE_TEMP "300")
set(STARDIS_ARGS_DEFAULT_SAMPLES_COUNT "10000")
set(STARDIS_ARGS_DEFAULT_SCALE_FACTOR "1")
set(STARDIS_ARGS_DEFAULT_VERBOSE_LEVEL "1")

configure_file(${SDIS_SOURCE_DIR}/../doc/stardis.1.txt.in
  ${CMAKE_CURRENT_BINARY_DIR}/doc/stardis.1.txt @ONLY)

set(SDIS_VERSION_MAJOR 0)
set(SDIS_VERSION_MINOR 5)
set(SDIS_VERSION_PATCH 1)
set(SDIS_VERSION ${SDIS_VERSION_MAJOR}.${SDIS_VERSION_MINOR}.${SDIS_VERSION_PATCH})

configure_file(${SDIS_SOURCE_DIR}/stardis-default.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/stardis-default.h @ONLY)
  
configure_file(${SDIS_SOURCE_DIR}/stardis-version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/stardis-version.h @ONLY)

###############################################################################
# Check dependencies
###############################################################################
find_package(RCMake 0.4 REQUIRED)
find_package(RSys 0.11 REQUIRED)
find_package(StarGeom3D 0.1 REQUIRED)
find_package(Star3D 0.7.3 REQUIRED)
find_package(StarEnc3D 0.4.2 REQUIRED)
find_package(Stardis 0.11 REQUIRED)
find_package(StarSTL 0.3 REQUIRED)
if(MSVC)
  find_package(MuslGetopt REQUIRED)
endif()

include_directories(
  ${RSys_INCLUDE_DIR}
  ${Star3D_INCLUDE_DIR}
  ${StarGeom3D_INCLUDE_DIR}
  ${StarEnc3D_INCLUDE_DIR}
  ${Stardis_INCLUDE_DIR}
  ${StarSTL_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR})
if(MSVC)
  include_directories(${MuslGetopt_INCLUDE_DIR})
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${RCMAKE_SOURCE_DIR})
include(rcmake)
include(rcmake_runtime)

if(CMAKE_COMPILER_IS_GNUCC)
  rcmake_append_runtime_dirs(_runtime_dirs
    RSys Stardis Star3D StarGeom3D StarEnc3D StarSTL)
endif()
if(MSVC)
  rcmake_append_runtime_dirs(_runtime_dirs
    RSys MuslGetopt Stardis Star3D StarGeom3D StarEnc3D StarSTL)
endif()

###############################################################################
# Build subprojects
###############################################################################
if(NOT STARDIS_DOC STREQUAL "NONE")
  add_subdirectory(doc)
endif()

###############################################################################
# Configure and define targets
###############################################################################
set(SDIS_FILES_SRC
  stardis-app.c
  stardis-compute.c
  stardis-fluid.c
  stardis-intface.c
  stardis-main.c
  stardis-output.c
  stardis-parsing.c
  stardis-solid.c)

set(SDIS_FILES_INC
  stardis-app.h
  stardis-compute.h
  stardis-default.h.in
  stardis-fluid.h
  stardis-intface.h
  stardis-output.h
  stardis-parsing.h
  stardis-solid.h
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

target_link_libraries(stardis
  Stardis Star3D StarGeom3D StarEnc3D StarSTL RSys ${GETOPT_LIB} ${MATH_LIB})

###############################################################################
# Define output & install directories
###############################################################################
install(TARGETS stardis
  ARCHIVE DESTINATION bin
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(FILES ${SDIS_FILES_DOC} DESTINATION share/doc/stardis)

rcmake_copy_runtime_libraries(stardis)
