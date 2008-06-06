omxvideosrcdir = $(plugindir)

omxvideosrc_LTLIBRARIES = libomxvideosrc.la

libomxvideosrc_la_SOURCES = omx_videosrc_component.c omx_videosrc_component.h \
			    library_entry_point.c

libomxvideosrc_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la
libomxvideosrc_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxvideosrc_la_CFLAGS = -I$(top_srcdir)/include \
			   -I$(top_srcdir)/src \
			   -I$(top_srcdir)/src/base
