# Copyright (C) 2018-2025 |Méso|Star> (contact@meso-star.com)
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
 src/stardis-extern-source.c\
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
 src/stardis-radiative-env.c\
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
	@$(MAKE) -fMakefile $$(for i in $(DEP); do echo -f "$${i}"; done) stardis

stardis $(DEP) $(OBJ): config.mk

stardis: $(OBJ)
	$(CC) $(CFLAGS) $(INCS) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

.config: config.mk
	if [ "$(DISTRIB_PARALLELISM)" = "MPI" ]; then \
	  $(PKG_CONFIG) --atleast-version $(MPI_VERSION) $(MPI_PC); \
	fi
	$(PKG_CONFIG) --atleast-version $(RSYS_VERSION) rsys
	$(PKG_CONFIG) --atleast-version $(S3D_VERSION) s3d
	$(PKG_CONFIG) --atleast-version $(SDIS_VERSION) sdis
	$(PKG_CONFIG) --atleast-version $(SENC3D_VERSION) senc3d
	$(PKG_CONFIG) --atleast-version $(SG3D_VERSION) sg3d
	$(PKG_CONFIG) --atleast-version $(SSP_VERSION) star-sp
	$(PKG_CONFIG) --atleast-version $(SSTL_VERSION) sstl
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
	@$(CC) $(CFLAGS) $(INCS) $($(DISTRIB_PARALLELISM)_DEF) -MM -MT \
	"$(@:.d=.o) $@" $< -MF $@

.c.o:
	$(CC) $(CFLAGS) $(INCS) $($(DISTRIB_PARALLELISM)_DEF) -c $< -o $@

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
pkg:
	sed -e 's#@PREFIX@#$(PREFIX)#g'\
	    -e 's#@VERSION@#$(VERSION)#g'\
	    stardis.pc.in > stardis.pc

install: all pkg
	install() { mode="$$1"; prefix="$$2"; shift 2; \
	  mkdir -p "$${prefix}"; \
	  cp "$$@" "$${prefix}"; \
	  for i in "$$@"; do \
	    chmod "$${mode}" "$${prefix}/$$(basename "$${i}")";\
	  done; \
	}; \
	install 755 "$(DESTDIR)$(BINPREFIX)" stardis; \
	install 644 "$(DESTDIR)$(INCPREFIX)/stardis" src/stardis-green-types.h; \
	install 644 "$(DESTDIR)$(INCPREFIX)/stardis" src/stardis-prog-properties.h; \
	install 644 "$(DESTDIR)$(LIBPREFIX)/pkgconfig" stardis.pc; \
	install 644 "$(DESTDIR)$(PREFIX)/share/doc/stardis" COPYING README.md; \
	install 644 "$(DESTDIR)$(MANPREFIX)/man1" doc/stardis.1; \
	install 644 "$(DESTDIR)$(MANPREFIX)/man5" doc/stardis-input.5; \
	install 644 "$(DESTDIR)$(MANPREFIX)/man5" doc/stardis-output.5

uninstall:
	rm -f "$(DESTDIR)$(BINPREFIX)/stardis"
	rm -f "$(DESTDIR)$(INCPREFIX)/stardis/stardis-green-types.h"
	rm -f "$(DESTDIR)$(INCPREFIX)/stardis/stardis-prog-properties.h"
	rm -f "$(DESTDIR)$(LIBPREFIX)/pkgconfig/stardis.pc"
	rm -f "$(DESTDIR)$(PREFIX)/share/doc/stardis/COPYING"
	rm -f "$(DESTDIR)$(PREFIX)/share/doc/stardis/README.md"
	rm -f "$(DESTDIR)$(MANPREFIX)/man1/stardis.1"
	rm -f "$(DESTDIR)$(MANPREFIX)/man5/stardis-input.5"
	rm -f "$(DESTDIR)$(MANPREFIX)/man5/stardis-output.5"

################################################################################
# Miscellaneous targets
################################################################################
clean:
	rm -f $(HDR) $(OBJ) .config stardis stardis.pc
	rm -f $(DEP)
	rm -f doc/stardis.1 doc/stardis-input.5

lint: man
	shellcheck -o all make.sh
	mandoc -Tlint -Wall doc/stardis.1 || [ $$? -le 1 ]
	mandoc -Tlint -Wall doc/stardis-input.5 || [ $$? -le 1 ]
	mandoc -Tlint -Wall doc/stardis-output.5 || [ $$? -le 1 ]
