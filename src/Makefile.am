SUBDIRS = 
libomxil_la_LIBADD = -lpthread -ldl
lib_LTLIBRARIES = libomxil.la
bin_PROGRAMS = omxregister
omxregister_SOURCES = omxregister.c
omxregister_CFLAGS = -DOMXILCOMPONENTSPATH=\"$(libdir)/omxilcomponents/\"
omxregister_LDADD = -lomxil
includedir = $(prefix)/include
noinst_HEADERS = queue.h tsemaphore.h omxcore.h omx_comp_debug_levels.h user_debug_levels.h
libomxil_la_SOURCES = st_static_component_loader.c omxcore.c tsemaphore.c queue.c 
INCLUDES = -I../include

include_HEADERS = component_loader.h st_static_component_loader.h

#Build order is important
#First base, then core, finally components
SUBDIRS += base . components

# addition of base and n-port classes for component development
libomxil_la_LIBADD += base/libomxbase.la

