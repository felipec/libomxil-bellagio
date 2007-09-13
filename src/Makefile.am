SUBDIRS = 
lib_LTLIBRARIES = libomxil.la
bin_PROGRAMS = omxregister
omxregister_SOURCES = omxregister.c
omxregister_CFLAGS = -DOMXILCOMPONENTSPATH=\"$(libdir)/omxilcomponents/\"
omxregister_LDADD = libomxil.la $(LIBS)
libomxil_la_SOURCES = st_static_component_loader.c \
                      omxcore.c omxcore.h \
		      omx_comp_debug_levels.h \
		      tsemaphore.c tsemaphore.h \
		      queue.c queue.h \
		      component_loader.h \
		      st_static_component_loader.h
AM_CPPFLAGS = -I@top_srcdir@/include

#Build order is important
#First base, then core, finally components
SUBDIRS += base . components

# addition of base and n-port classes for component development
libomxil_la_LIBADD = base/libomxbase.la

