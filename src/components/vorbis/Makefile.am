omxvorbisdir = $(plugindir)

omxvorbis_LTLIBRARIES = libomxvorbis.la

libomxvorbis_la_SOURCES = omx_vorbisdec_component.c omx_vorbisdec_component.h \
			  library_entry_point.c

libomxvorbis_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la $(VORBIS_LIBS)
libomxvorbis_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxvorbis_la_CFLAGS = $(VORBIS_CFLAGS) \
			 -I$(top_srcdir)/include \
			 -I$(top_srcdir)/src \
			 -I$(top_srcdir)/src/base
