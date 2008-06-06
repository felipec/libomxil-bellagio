omxcameradir = $(plugindir)

omxcamera_LTLIBRARIES = libomxcamera.la

libomxcamera_la_SOURCES = omx_camera_source_component.c omx_camera_source_component.h \
			  library_entry_point.c

libomxcamera_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la
libomxcamera_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxcamera_la_CFLAGS = -I$(top_srcdir)/include \
			 -I$(top_srcdir)/src \
			 -I$(top_srcdir)/src/base
