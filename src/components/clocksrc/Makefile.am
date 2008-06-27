omxclocksrcdir = $(plugindir)

omxclocksrc_LTLIBRARIES = libomxclocksrc.la

libomxclocksrc_la_SOURCES = omx_clocksrc_component.c omx_clocksrc_component.h \
                              library_entry_point.c

libomxclocksrc_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la $(FFMPEG_LIBS)
libomxclocksrc_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxclocksrc_la_CFLAGS = $(FFMPEG_CFLAGS) \
                             -I$(top_srcdir)/include \
                             -I$(top_srcdir)/src \
                             -I$(top_srcdir)/src/base
