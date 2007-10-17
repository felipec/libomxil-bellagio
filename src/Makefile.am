# Build order is important
# First base, then core, finally components
SUBDIRS = base . components

bin_PROGRAMS = omxregister
omxregister_SOURCES = omxregister.c
omxregister_CFLAGS = -DOMXILCOMPONENTSPATH=\"$(libdir)/omxilcomponents/\" \
		     -I$(top_srcdir)/include

lib_LTLIBRARIES = libomxil.la
libomxil_la_SOURCES = st_static_component_loader.c \
                      omxcore.c omxcore.h \
                      omx_comp_debug_levels.h \
                      tsemaphore.c tsemaphore.h \
                      queue.c queue.h \
                      component_loader.h \
                      st_static_component_loader.h

libomxil_la_CFLAGS = -I$(top_srcdir)/include  -I$(srcdir)/base
libomxil_la_LIBADD = base/libomxbase.la
