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

.POSIX:
.SUFFIXES: # Clean up default inference rules

include config.mk

MPI_DEF = -DSTARDIS_ENABLE_MPI

# Default target
all: build_executable man

################################################################################
# Program building
################################################################################
SRC =\
 src/stardis-app.c\
 src/stardis-args.c\
 src/stardis-compute.c\
 src/stardis-compute-probe-boundary.c\
 src/stardis-description.c\
 src/stardis-fluid.c\
 src/stardis-fluid-prog.c\
 src/stardis-fbound.c\
 src/stardis-fbound-prog.c\
 src/stardis-hbound.c\
 src/stardis-hbound-prog.c\
 src/stardis-hfbound.c\
 src/stardis-hfbound-prog.c\
 src/stardis-intface.c\
 src/stardis-main.c\
 src/stardis-output.c\
 src/stardis-parsing.c\
 src/stardis-program.c\
 src/stardis-sfconnect.c\
 src/stardis-sfconnect-prog.c\
 src/stardis-ssconnect.c\
 src/stardis-ssconnect-prog.c\
 src/stardis-solid.c\
 src/stardis-solid-prog.c\
 src/stardis-tbound.c\
 src/stardis-tbound-prog.c

# Headers to configure
HDR=\
 src/stardis-args.h\
 src/stardis-default.h\
 src/stardis-green-types.h\
 src/stardis-prog-properties.h\
 src/stardis-version.h

OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

build_executable: .config $(HDR) $(DEP)
	@$(MAKE) -fMakefile $$(for i in $(DEP); do echo -f $${i}; done) stardis

$(DEP) $(OBJ): config.mk

stardis: $(OBJ)
	$(CC) $(CFLAGS) $(DPDC_CFLAGS) -o $@ $(OBJ) $(LDFLAGS) $(DPDC_LIBS)

.config: config.mk
	@if [ "$(DISTRIB_PARALLELISM)" = "MPI" ]; then \
	  if ! $(PKG_CONFIG) --atleast-version $(MPI_VERSION) $(MPI_PC); then \
	    echo "$(MPI_PC) $(MPI_VERSION) not found" >&2; exit 1; fi; fi
	@if ! $(PKG_CONFIG) --atleast-version $(RSYS_VERSION) rsys; then \
	  echo "rsys $(RSYS_VERSION) not found" >&2; exit 1; fi
	@if ! $(PKG_CONFIG) --atleast-version $(S3D_VERSION) s3d; then \
	  echo "s3d $(S3D_VERSION) not found" >&2; exit 1; fi
	@if ! $(PKG_CONFIG) --atleast-version $(SDIS_VERSION) sdis; then \
	  echo "sdis $(SDIS_VERSION) not found" >&2; exit 1; fi
	@if ! $(PKG_CONFIG) --atleast-version $(SENC3D_VERSION) senc3d; then \
	  echo "senc3d $(SENC3D_VERSION) not found" >&2; exit 1; fi
	@if ! $(PKG_CONFIG) --atleast-version $(SG3D_VERSION) sg3d; then \
	  echo "sg3d $(SG3D_VERSION) not found" >&2; exit 1; fi
	@if ! $(PKG_CONFIG) --atleast-version $(SSP_VERSION) star-sp; then \
	  echo "star-sp $(SSP_VERSION) not found" >&2; exit 1; fi
	@if ! $(PKG_CONFIG) --atleast-version $(SSTL_VERSION) sstl; then \
	  echo "sstl $(SSTL_VERSION) not found" >&2; exit 1; fi
	@echo "config done" > $@

src/stardis-default.h: config.mk src/stardis-default.h.in
	sed -e 's/@STARDIS_ARGS_DEFAULT_TRAD@/$(STARDIS_ARGS_DEFAULT_TRAD)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_TRAD_REFERENCE@/$(STARDIS_ARGS_DEFAULT_TRAD_REFERENCE)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_COMPUTE_TIME@/$(STARDIS_ARGS_DEFAULT_COMPUTE_TIME)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_PICARD_ORDER@/$(STARDIS_ARGS_DEFAULT_PICARD_ORDER)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_FOV@/$(STARDIS_ARGS_DEFAULT_RENDERING_FOV)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_IMG_HEIGHT@/$(STARDIS_ARGS_DEFAULT_RENDERING_IMG_HEIGHT)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_IMG_WIDTH@/$(STARDIS_ARGS_DEFAULT_RENDERING_IMG_WIDTH)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_OUTPUT_FILE_FMT@/$(STARDIS_ARGS_DEFAULT_RENDERING_OUTPUT_FILE_FMT)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_POS@/$(STARDIS_ARGS_DEFAULT_RENDERING_POS)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_SPP@/$(STARDIS_ARGS_DEFAULT_RENDERING_SPP)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_TGT@/$(STARDIS_ARGS_DEFAULT_RENDERING_TGT)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_TIME@/$(STARDIS_ARGS_DEFAULT_RENDERING_TIME)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_UP@/$(STARDIS_ARGS_DEFAULT_RENDERING_UP)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_SAMPLES_COUNT@/$(STARDIS_ARGS_DEFAULT_SAMPLES_COUNT)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_SCALE_FACTOR@/$(STARDIS_ARGS_DEFAULT_SCALE_FACTOR)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_VERBOSE_LEVEL@/$(STARDIS_ARGS_DEFAULT_VERBOSE_LEVEL)/' \
	    $@.in > $@

src/stardis-args.h: config.mk src/stardis-args.h.in
	sed -e 's/@STARDIS_MAX_NAME_LENGTH@/$(STARDIS_MAX_NAME_LENGTH)/' \
	    $@.in > $@

src/stardis-version.h: config.mk src/stardis-version.h.in
	sed -e 's/@STARDIS_APP_VERSION_MAJOR@/$(VERSION_MAJOR)/' \
	    -e 's/@STARDIS_APP_VERSION_MINOR@/$(VERSION_MINOR)/' \
	    -e 's/@STARDIS_APP_VERSION_PATCH@/$(VERSION_PATCH)/' \
	    $@.in > $@

src/stardis-green-types.h: config.mk src/stardis-green-types.h.in
	sed -e 's/@STARDIS_GREEN_TYPES_VERSION@/$(GREEN_TYPES_VERSION)/' \
	    -e "s/@STARDIS_MAX_NAME_LENGTH@/$$(($(STARDIS_MAX_NAME_LENGTH)-1))/" \
	    $@.in > $@

src/stardis-prog-properties.h: config.mk src/stardis-prog-properties.h.in
	sed -e 's/@STARDIS_PROG_PROPERTIES_VERSION@/$(PROG_PROPERTIES_VERSION)/' \
	    $@.in > $@

.SUFFIXES: .c .d .o
.c.d:
	@$(CC) $(CFLAGS) $(DPDC_CFLAGS) $($(DISTRIB_PARALLELISM)_DEF) -MM -MT \
	"$(@:.d=.o) $@" $< -MF $@

.c.o:
	$(CC) $(CFLAGS) $(DPDC_CFLAGS) $($(DISTRIB_PARALLELISM)_DEF) -c $< -o $@

################################################################################
# Man pages
################################################################################
man: doc/stardis.1 doc/stardis-input.5

doc/stardis.1: doc/stardis.1.in
	sed -e 's/@STARDIS_ARGS_DEFAULT_COMPUTE_TIME@/$(STARDIS_ARGS_DEFAULT_COMPUTE_TIME)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_PICARD_ORDER@/$(STARDIS_ARGS_DEFAULT_PICARD_ORDER)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_FOV@/$(STARDIS_ARGS_DEFAULT_RENDERING_FOV)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_IMG_HEIGHT@/$(STARDIS_ARGS_DEFAULT_RENDERING_IMG_HEIGHT)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_IMG_WIDTH@/$(STARDIS_ARGS_DEFAULT_RENDERING_IMG_WIDTH)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_OUTPUT_FILE_FMT@/$(STARDIS_ARGS_DEFAULT_RENDERING_OUTPUT_FILE_FMT)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_POS@/$(STARDIS_ARGS_DEFAULT_RENDERING_POS)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_SPP@/$(STARDIS_ARGS_DEFAULT_RENDERING_SPP)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_TGT@/$(STARDIS_ARGS_DEFAULT_RENDERING_TGT)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_TIME@/$(STARDIS_ARGS_DEFAULT_RENDERING_TIME)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_RENDERING_UP@/$(STARDIS_ARGS_DEFAULT_RENDERING_UP)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_SAMPLES_COUNT@/$(STARDIS_ARGS_DEFAULT_SAMPLES_COUNT)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_SCALE_FACTOR@/$(STARDIS_ARGS_DEFAULT_SCALE_FACTOR)/' \
	    -e 's/@STARDIS_ARGS_DEFAULT_VERBOSE_LEVEL@/$(STARDIS_ARGS_DEFAULT_VERBOSE_LEVEL)/' \
	    $@.in > $@

doc/stardis-input.5: doc/stardis-input.5.in
	sed -e "s/@STARDIS_MAX_NAME_LENGTH@/$$(($(STARDIS_MAX_NAME_LENGTH)-1))/" \
	    $@.in > $@

################################################################################
# Installation
################################################################################
install: all
	@$(SHELL) make.sh install "$(DESTDIR)$(PREFIX)/bin" stardis
	@$(SHELL) make.sh install "$(DESTDIR)$(PREFIX)/include/stardis" src/stardis-green-types.h
	@$(SHELL) make.sh install "$(DESTDIR)$(PREFIX)/include/stardis" src/stardis-prog-properties.h
	@$(SHELL) make.sh install "$(DESTDIR)$(PREFIX)/share/doc/stardis" COPYING README.md
	@$(SHELL) make.sh install "$(DESTDIR)$(PREFIX)/share/man/man1" doc/stardis.1
	@$(SHELL) make.sh install "$(DESTDIR)$(PREFIX)/share/man/man5" doc/stardis-input.5
	@$(SHELL) make.sh install "$(DESTDIR)$(PREFIX)/share/man/man5" doc/stardis-output.5

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/stardis"
	rm -f "$(DESTDIR)$(PREFIX)/include/stardis/stardis-green-types.h"
	rm -f "$(DESTDIR)$(PREFIX)/include/stardis/stardis-prog-properties.h"
	rm -f "$(DESTDIR)$(PREFIX)/share/doc/stardis/COPYING"
	rm -f "$(DESTDIR)$(PREFIX)/share/doc/stardis/README.md"
	rm -f "$(DESTDIR)$(PREFIX)/share/man/man1/stardis.1"
	rm -f "$(DESTDIR)$(PREFIX)/share/man/man5/stardis-input.5"
	rm -f "$(DESTDIR)$(PREFIX)/share/man/man5/stardis-output.5"

################################################################################
# Miscellaneous targets
################################################################################
clean:
	rm -f $(HDR) $(OBJ) .config stardis doc/stardis.1 doc/stardis-input.5

distclean: clean
	rm -f $(DEP)

lint: man
	shellcheck -o all make.sh
	mandoc -Tlint -Wall doc/stardis.1 || [ $$? -le 1 ]
	mandoc -Tlint -Wall doc/stardis-input.5 || [ $$? -le 1 ]
	mandoc -Tlint -Wall doc/stardis-output.5 || [ $$? -le 1 ]
