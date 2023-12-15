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

set(STARDIS_SPROG_DIR ${PROJECT_SOURCE_DIR}/../stardis-prog-properties)

################################################################################
# Copy stardis-prog-properties CMake files
################################################################################
set(SPROG_NAMES
  stardis-prog-properties-config)

set(SPROG_FILES)
foreach(_name IN LISTS SPROG_NAMES)
  set(_src ${STARDIS_SPROG_DIR}/${_name}.cmake)
  set(_dst ${CMAKE_CURRENT_BINARY_DIR}/${_name}.cmake)
  add_custom_command(
    OUTPUT ${_dst}
    COMMAND ${CMAKE_COMMAND} -E copy ${_src} ${_dst}
    DEPENDS ${_src}
    COMMENT "Copy the CMake file ${_src}"
    VERBATIM)
  list(APPEND SPROG_FILES ${_dst})
endforeach()
add_custom_target(stardis-prog-cmake ALL DEPENDS ${SPROG_FILES})

################################################################################
# Install stardis-prog-properties CMake Files
################################################################################
list(APPEND SPROG_FILES ${CMAKE_CURRENT_BINARY_DIR}/stardis-prog-properties-config-version.cmake)

install(FILES ${SPROG_FILES}
	DESTINATION lib/cmake/stardis-prog-properties)

################################################################################
# Install stardis-prog-properties header Files
################################################################################
install(FILES
	  ${CMAKE_CURRENT_BINARY_DIR}/stardis-prog-properties.h
	DESTINATION include/stardis/)
