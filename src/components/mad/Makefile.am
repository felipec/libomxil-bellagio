omxmaddir = $(plugindir)

omxmad_LTLIBRARIES = libomxmad.la

libomxmad_la_SOURCES = omx_maddec_component.c omx_maddec_component.h \
		       library_entry_point.c 

libomxmad_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la $(MAD_LIBS)
libomxmad_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxmad_la_CFLAGS = $(MAD_CFLAGS) \
		      -I$(top_srcdir)/include \
		      -I$(top_srcdir)/src \
		      -I$(top_srcdir)/src/base
