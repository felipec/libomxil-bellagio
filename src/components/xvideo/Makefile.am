omxxvideodir = $(plugindir)

omxxvideo_LTLIBRARIES = libomxxvideo.la

libomxxvideo_la_SOURCES = omx_xvideo_sink_component.c omx_xvideo_sink_component.h \
			 library_entry_point.c

libomxxvideo_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la
libomxxvideo_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(X11_LIBS)
libomxxvideo_la_CFLAGS = -I$(top_srcdir)/include \
			-I$(top_srcdir)/src \
			-I$(top_srcdir)/src/base
