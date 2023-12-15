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

cmake_minimum_required(VERSION 3.5)

set(STARDIS_SGT_DIR ${PROJECT_SOURCE_DIR}/../stardis-green-types)

################################################################################
# Copy stardis-green-types CMake files
################################################################################
set(SGT_NAMES
  stardis-green-types-config)

set(SGT_FILES)
foreach(_name IN LISTS SGT_NAMES)
  set(_src ${STARDIS_SGT_DIR}/${_name}.cmake)
  set(_dst ${CMAKE_CURRENT_BINARY_DIR}/${_name}.cmake)
  add_custom_command(
    OUTPUT ${_dst}
    COMMAND ${CMAKE_COMMAND} -E copy ${_src} ${_dst}
    DEPENDS ${_src}
    COMMENT "Copy the CMake file ${_src}"
    VERBATIM)
  list(APPEND SGT_FILES ${_dst})
endforeach()
add_custom_target(sgt-cmake ALL DEPENDS ${SGT_FILES})

################################################################################
# Install stardis-green-types CMake Files
################################################################################
list(APPEND SGT_FILES ${CMAKE_CURRENT_BINARY_DIR}/stardis-green-types-config-version.cmake)

install(FILES ${SGT_FILES}
	DESTINATION lib/cmake/stardis-green-types)

################################################################################
# Install stardis-green-types header Files
################################################################################
install(FILES
	  ${CMAKE_CURRENT_BINARY_DIR}/stardis-green-types.h
	DESTINATION include/stardis/)
