# Makefile to build sgxperf
# Usage - 'SGXPERF_SDKDIR=<GraphicsSDK path> make'
# For OGLES2.0 performance validation on Linux
# (c) TEXAS INSTRUMENTS, 2009-2014
# prabu@ti.com

.PHONY: clean

ifeq "$(SGXPERF_SDKDIR)" ""
$(error "ERROR: SGXPERF_SDKDIR not defined (ex. export SGXPERF_SDKDIR=<Path to Graphics SDK>)!")
endif

PLATFORM=release
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

#for native compilation, comment this out
CROSS_COMPILE=arm-linux-gnueabihf-

PLAT_CC  = $(CROSS_COMPILE)gcc
PLAT_CPP = $(CROSS_COMPILE)g++
PLAT_AR  = $(CROSS_COMPILE)ar

LIBDIR_FLAGS = -L$(LIBDIR) -Wl,--rpath-link,$(LIBDIR)

PLAT_CFLAGS   = -Wall -O2

ifeq "$(_ENABLE_BUFFERCLASS)" "1"
PLAT_CFLAGS += -D_ENABLE_BUFFERCLASS 
endif
ifeq "$(_ENABLE_CMEM)" "1"
PLAT_CFLAGS += -D_ENABLE_CMEM
endif

PLAT_OBJPATH = ./$(PLATFORM)/
PLAT_LINK =  $(LIBDIR_FLAGS) -lEGL -lGLESv2 -lm -ldl -lpvr2d

COMMON_INCLUDES=-I$(SGXPERF_SDKDIR)/include/OGLES2 -I$(SGXPERF_SDKDIR)/include/wsegl -I$(SGXPERF_SDKDIR)/include/pvr2d -I$(SGXPERF_SDKDIR)/include/bufferclass_ti -I$(SGXPERF_SDKDIR)/include/OGLES2/GLES2 -I$(SGXPERF_SDKDIR)/include/OGLES2/EGL -I$(SGXPERF_SDKDIR)/GFX_Linux_KM/include4

PLAT_CFLAGS += $(COMMON_INCLUDES)

SRCNAME = $(wildcard *.cpp)
OBJECTS = $(PLAT_OBJPATH)/$(SRCNAME:.cpp=.o)

OUTNAME = sgxperf3

ifeq "$(_ENABLE_CMEM)" "1"
OBJECTS2 = $(CMEM_PATH)/lib/cmem.a470MV
endif

$(PLAT_OBJPATH)/$(OUTNAME): $(SRCNAME)
	mkdir -p $(PLAT_OBJPATH)
	$(PLAT_CPP) -o $@ $^ $(PLAT_CFLAGS) $(LINK) $(PLAT_LINK)

clean:
	rm -rf $(PLAT_OBJPATH) *.o

