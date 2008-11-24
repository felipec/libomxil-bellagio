omxmuxdir = $(plugindir)

omxmux_LTLIBRARIES = libomxmux.la

libomxmux_la_SOURCES = omx_mux_component.c omx_mux_component.h \
                           library_entry_point.c

libomxmux_la_LIBADD  = $(top_builddir)/src/libomxil-bellagio.la $(FFMPEG_LIBS)
libomxmux_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxmux_la_CFLAGS  = -I$(top_srcdir)/include \
                           -I$(top_srcdir)/src \
                           -I$(top_srcdir)/src/base \
                             $(FFMPEG_CFLAGS)
