omxfbdevdir = $(plugindir)

omxfbdev_LTLIBRARIES = libomxfbdev.la

libomxfbdev_la_SOURCES = omx_fbdev_sink_component.c library_entry_point.c omx_fbdev_sink_component.h
libomxfbdev_la_LIBADD = ../../libomxil.la
libomxfbdev_la_LDFLAGS = $(PLUGIN_LDFLAGS)

AM_CPPFLAGS = -I@top_srcdir@/include -I@top_srcdir@/src -I@top_srcdir@/src/base
