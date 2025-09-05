VERSION_MAJOR = 0
VERSION_MINOR = 12
VERSION_PATCH = 0
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)
PREFIX = /usr/local

GREEN_TYPES_VERSION = 4
PROG_PROPERTIES_VERSION = 2

LIB_TYPE = SHARED
#LIB_TYPE = STATIC

BUILD_TYPE = RELEASE
#BUILD_TYPE = DEBUG

# Defines whether distributed parallelism  is supported. Any value other
# than MPI disables its supports. So, simply comment the macro to
# deactivate it.
DISTRIB_PARALLELISM = MPI

# MPI pkg-config file
MPI_PC = ompi

################################################################################
# Configuration values
################################################################################
STARDIS_ARGS_DEFAULT_TRAD = 300
STARDIS_ARGS_DEFAULT_TRAD_REFERENCE = 300
STARDIS_ARGS_DEFAULT_COMPUTE_TIME = INF
STARDIS_ARGS_DEFAULT_PICARD_ORDER = 1
STARDIS_ARGS_DEFAULT_RENDERING_FOV = 70# Degrees
STARDIS_ARGS_DEFAULT_RENDERING_IMG_HEIGHT = 480
STARDIS_ARGS_DEFAULT_RENDERING_IMG_WIDTH = 640
STARDIS_ARGS_DEFAULT_RENDERING_OUTPUT_FILE_FMT = HT
#STARDIS_ARGS_DEFAULT_RENDERING_OUTPUT_FILE_FMT = VTK
STARDIS_ARGS_DEFAULT_RENDERING_POS = 1,1,1
STARDIS_ARGS_DEFAULT_RENDERING_SPP = 4
STARDIS_ARGS_DEFAULT_RENDERING_TGT = 0,0,0
STARDIS_ARGS_DEFAULT_RENDERING_TIME = INF,INF
STARDIS_ARGS_DEFAULT_RENDERING_UP = 0,0,1
STARDIS_ARGS_DEFAULT_SAMPLES_COUNT = 10000
STARDIS_ARGS_DEFAULT_SCALE_FACTOR = 1
STARDIS_ARGS_DEFAULT_VERBOSE_LEVEL = 1

# Including NULL char
STARDIS_MAX_NAME_LENGTH = 64

################################################################################
# Tools
################################################################################
AR = ar
CC = cc
LD = ld
OBJCOPY = objcopy
PKG_CONFIG = pkg-config
RANLIB = ranlib

################################################################################
# Dependencies
################################################################################
PCFLAGS_SHARED =
PCFLAGS_STATIC = --static
PCFLAGS = $(PCFLAGS_$(LIB_TYPE))

MPI_VERSION = 2
MPI_CFLAGS = $$($(PKG_CONFIG) $(PCFLAGS) --cflags $(MPI_PC))
MPI_LIBS = $$($(PKG_CONFIG) $(PCFLAGS) --libs $(MPI_PC))

RSYS_VERSION = 0.14
RSYS_CFLAGS = $$($(PKG_CONFIG) $(PCFLAGS) --cflags rsys)
RSYS_LIBS = $$($(PKG_CONFIG) $(PCFLAGS) --libs rsys)

S3D_VERSION = 0.10
S3D_CFLAGS = $$($(PKG_CONFIG) $(PCFLAGS) --cflags s3d)
S3D_LIBS = $$($(PKG_CONFIG) $(PCFLAGS) --libs s3d)

SDIS_VERSION = 0.16
SDIS_CFLAGS = $$($(PKG_CONFIG) $(PCFLAGS) --cflags sdis)
SDIS_LIBS = $$($(PKG_CONFIG) $(PCFLAGS) --libs sdis)

SENC3D_VERSION = 0.7.2
SENC3D_CFLAGS = $$($(PKG_CONFIG) $(PCFLAGS) --cflags senc3d)
SENC3D_LIBS = $$($(PKG_CONFIG) $(PCFLAGS) --libs senc3d)

SG3D_VERSION = 0.2
SG3D_CFLAGS = $$($(PKG_CONFIG) $(PCFLAGS) --cflags sg3d)
SG3D_LIBS = $$($(PKG_CONFIG) $(PCFLAGS) --libs sg3d)

SSP_VERSION = 0.14
SSP_CFLAGS = $$($(PKG_CONFIG) $(PCFLAGS) --cflags star-sp)
SSP_LIBS = $$($(PKG_CONFIG) $(PCFLAGS) --libs star-sp)

SSTL_VERSION = 0.5
SSTL_CFLAGS = $$($(PKG_CONFIG) $(PCFLAGS) --cflags sstl)
SSTL_LIBS = $$($(PKG_CONFIG) $(PCFLAGS) --libs sstl)

DPDC_CFLAGS =\
 $(RSYS_CFLAGS)\
 $(S3D_CFLAGS)\
 $(SDIS_CFLAGS)\
 $(SENC3D_CFLAGS)\
 $(SG3D_CFLAGS)\
 $(SSP_CFLAGS)\
 $(SSTL_CFLAGS)\
 $($(DISTRIB_PARALLELISM)_CFLAGS)
DPDC_LIBS =\
 $(RSYS_LIBS)\
 $(S3D_LIBS)\
 $(SDIS_LIBS)\
 $(SENC3D_LIBS)\
 $(SG3D_LIBS)\
 $(SSP_LIBS)\
 $(SSTL_LIBS)\
 $($(DISTRIB_PARALLELISM)_LIBS)\
 -lm

################################################################################
# Compilation options
################################################################################
WFLAGS =\
 -Wall\
 -Wcast-align\
 -Wconversion\
 -Wextra\
 -Wmissing-declarations\
 -Wmissing-prototypes\
 -Wshadow

CFLAGS_HARDENED =\
 -D_FORTIFY_SOURCES=2\
 -fcf-protection=full\
 -fstack-clash-protection\
 -fstack-protector-strong

CFLAGS_COMMON =\
 -std=c89\
 -pedantic\
 -fvisibility=hidden\
 -fstrict-aliasing\
 $(CFLAGS_HARDENED)\
 $(WFLAGS)

CFLAGS_RELEASE = -O3 -DNDEBUG $(CFLAGS_COMMON)
CFLAGS_DEBUG = -g $(CFLAGS_COMMON)
CFLAGS = $(CFLAGS_$(BUILD_TYPE)) -fPIE

################################################################################
# Linker options
################################################################################
LDFLAGS_HARDENED = -Wl,-z,relro,-z,now
LDFLAGS_DEBUG = $(LDFLAGS_HARDENED)
LDFLAGS_RELEASE = -s $(LDFLAGS_HARDENED)
LDFLAGS = $(LDFLAGS_$(BUILD_TYPE)) -pie

OCPFLAGS_DEBUG = --localize-hidden
OCPFLAGS_RELEASE = --localize-hidden --strip-unneeded
OCPFLAGS = $(OCPFLAGS_$(BUILD_TYPE))
