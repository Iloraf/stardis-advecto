# Copyright (C) 2016-2018 |Meso|Star>
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

cmake_minimum_required(VERSION 3.8.2)
project(stardis-app C)

set(SDIS_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../src)

################################################################################
# Check dependencies
################################################################################
find_package(RCMake 0.4 REQUIRED)
find_package(RSys 0.6.1 REQUIRED)
find_package(Stardis 0.5 REQUIRED)
find_package(StarSTL 0.3 REQUIRED)

set(TINYEXPR_SOURCE_DIR  "${PROJECT_SOURCE_DIR}/../tinyexpr" CACHE PATH "Directory for TinyExpr source files")

include_directories(
  ${RSys_INCLUDE_DIR}
  ${Stardis_INCLUDE_DIR}
  ${StarSTL_INCLUDE_DIR}
  ${TINYEXPR_SOURCE_DIR})
if(MSVC)
  find_package(MuslGetopt REQUIRED)
  include_directories(${MuslGetopt_INCLUDE_DIR})
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${RCMAKE_SOURCE_DIR})
include(rcmake)
include(rcmake_runtime)

rcmake_append_runtime_dirs(_runtime_dirs RSys Stardis StarSTL)

################################################################################
# Configure and define targets
################################################################################
set(VERSION_MAJOR 0)
set(VERSION_MINOR 4)
set(VERSION_PATCH 0)
set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(SDIS_FILES_SRC
  stardis-app.c
  stardis-compute.c
  main.c)

set(SDIS_FILES_INC
  stardis-app.h
  args.h)

set(SDIS_FILES_DOC COPYING README.md)

# Prepend each file by `SDIS_SOURCE_DIR'
rcmake_prepend_path(SDIS_FILES_SRC ${SDIS_SOURCE_DIR})
rcmake_prepend_path(SDIS_FILES_INC ${SDIS_SOURCE_DIR})
rcmake_prepend_path(SDIS_FILES_DOC ${PROJECT_SOURCE_DIR}/../)

if(CMAKE_COMPILER_IS_GNUCC)
  set(MATH_LIB m)
endif()

ADD_LIBRARY(tinyexpr STATIC
  ${TINYEXPR_SOURCE_DIR}/tinyexpr.c ${TINYEXPR_SOURCE_DIR}/tinyexpr.h)

add_executable(sdis-app
  ${SDIS_FILES_SRC}
  ${SDIS_FILES_INC})
target_compile_features(sdis-app PUBLIC c_std_11)
target_link_libraries(sdis-app Stardis StarSTL RSys tinyexpr ${MATH_LIB})
if(MSVC)
  target_link_libraries(sdis-app MuslGetopt)
endif()

set_target_properties(sdis-app PROPERTIES
  VERSION ${VERSION})
rcmake_copy_runtime_libraries(sdis-app)

################################################################################
# Define output & install directories
################################################################################
install(TARGETS sdis-app
  ARCHIVE DESTINATION bin
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(FILES ${SDIS_FILES_DOC} DESTINATION share/doc/stardis-app)

