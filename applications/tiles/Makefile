include ../../config.mak

vpath %.c $(SRC_PATH)/applications/tiles

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include"

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

LINKFLAGS=-L../../bin/gcc
ifeq ($(CONFIG_WIN32),yes)
EXE=.exe
PROG=TILES$(EXE)
else
EXT=
PROG=TILES
endif

ifeq ($(STATICBUILD),yes)
##include static modules and other deps for libgpac
include ../../static.mak

#FIXME we have to disable AAC+bifs support in mp42ts since it reuses things from aac_in already in libgpac ...
ifeq ($(STATIC_MODULES), yes)
CFLAGS+=-DGPAC_DISABLE_PLAYER
endif

LINKFLAGS+=-lgpac_static
LINKFLAGS+= $(GPAC_SH_FLAGS)
LINKFLAGS+=$(EXTRALIBS)
else
LINKFLAGS+=-lgpac
endif

#common objs - insert after ../static if any to overwrite list of objects
OBJS= main.o
OBJS+= helper.o

SRCS := $(OBJS:.o=.c) 

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o ../../../project/$@ $(OBJS) $(LINKFLAGS) $(LDFLAGS)

clean: 
	rm -f $(OBJS) ../../../project/$(PROG)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend