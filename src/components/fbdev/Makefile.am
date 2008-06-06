omxfbdevdir = $(plugindir)

omxfbdev_LTLIBRARIES = libomxfbdev.la

libomxfbdev_la_SOURCES = omx_fbdev_sink_component.c omx_fbdev_sink_component.h \
			 library_entry_point.c

libomxfbdev_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la
libomxfbdev_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxfbdev_la_CFLAGS = -I$(top_srcdir)/include \
			-I$(top_srcdir)/src \
			-I$(top_srcdir)/src/base
