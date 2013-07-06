# Makefile to build sgxperf
# For OGLES2.0 and OVG performance validation on Linux
# View README and INSTALL for usage and installation
# (c) TEXAS INSTRUMENTS, 2013
# prabu@ti.com

.PHONY: clean

ifeq "$(SGXPERF_SDKDIR)" ""
$(error "ERROR: SGXPERF_SDKDIR not defined (ex. export SGXPERF_SDKDIR=<Path to Graphics SDK>)!")
endif

PLATFORM=335x-release
LIBDIR=$(SGXPERF_SDKDIR)/gfx_rel_es8.x

#################################################
# ENABLE CMEM and BUFFERCLASS if needed
#################################################
_ENABLE_BUFFERCLASS = 0
_ENABLE_CMEM = 0
#################################################
# Change this to suit CMEM path if CMEM needed
#################################################
#CMEM_PATH = /home/prabu/cmem

CROSS_COMPILE=arm-linux-gnueabi-

PLAT_CC  = $(CROSS_COMPILE)gcc
PLAT_CPP = $(CROSS_COMPILE)g++
PLAT_AR  = $(CROSS_COMPILE)ar

LIBDIR_FLAGS = -L$(LIBDIR) -Wl,--rpath-link,$(LIBDIR)

PLAT_CFLAGS   = -DBUILD_OGLES2 -Wall -DRELEASE -O2 \
				-DAPIENTRY=GL_APIENTRY            \
				-DUSE_TRIGONOMETRIC_LOOKUP_TABLES 
ifeq "$(_ENABLE_BUFFERCLASS)" "1"
PLAT_CFLAGS += -D_ENABLE_BUFFERCLASS 
endif
ifeq "$(_ENABLE_CMEM)" "1"
PLAT_CFLAGS += -D_ENABLE_CMEM
endif

PLAT_OBJPATH = ./$(PLATFORM)/
PLAT_LINK =  $(LIBDIR_FLAGS) -lEGL -lGLESv2 -lm -ldl

SRCNAME = sgxperf_gles20_vg

OUTNAME = sgxperf2

OBJECTS = $(PLAT_OBJPATH)/$(SRCNAME).o

OBJECTS2=
ifeq "$(_ENABLE_CMEM)" "1"
OBJECTS2 = $(CMEM_PATH)/lib/cmem.a470MV
endif

COMMON_INCLUDES=-I$(SGXPERF_SDKDIR)/include/OGLES2 -I$(SGXPERF_SDKDIR)/include/wsegl -I$(SGXPERF_SDKDIR)/include/pvr2d -I$(SGXPERF_SDKDIR)/include/bufferclass_ti -I$(SGXPERF_SDKDIR)/include/OGLES2/GLES2 -I$(SGXPERF_SDKDIR)/include/OGLES2/EGL

PVR2D_LINK = -lpvr2d

$(PLAT_OBJPATH)/$(OUTNAME) : $(OBJECTS) 
	mkdir -p $(PLAT_OBJPATH)
	$(PLAT_CPP) -o $(PLAT_OBJPATH)/$(OUTNAME) $(OBJECTS) $(OBJECTS1) $(OBJECTS2) $(LINK) $(PLAT_LINK) $(VG_LINK) $(PVR2D_LINK)

$(PLAT_OBJPATH)/%.o: %.cpp 
	mkdir -p $(PLAT_OBJPATH)
	$(PLAT_CC) $(PLAT_CFLAGS) -c $(COMMON_INCLUDES) $(INCLUDES) $^ -o $@

$(PLAT_OBJPATH)/%.o: %.c 
	mkdir -p $(PLAT_OBJPATH)
	$(PLAT_CC) $(PLAT_CFLAGS) -c $(COMMON_INCLUDES) $(INCLUDES) $^ -o $@

clean:
	rm -rf $(PLAT_OBJPATH) *.o


