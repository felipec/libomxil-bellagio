omxcameradir = $(plugindir)

omxcamera_LTLIBRARIES=libomxcamera.la

libomxcamera_la_SOURCES = omx_camera_source_component.c library_entry_point.c 
noinst_HEADERS = omx_camera_source_component.h
libomxcamera_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la
libomxcamera_la_LDFLAGS = $(PLUGIN_LDFLAGS)

INCLUDES = -I$(top_srcdir)/include/ -I$(top_srcdir)/src -I$(srcdir) -I$(top_srcdir)/src/base/
