omxfilereaderdir = $(plugindir)

omxfilereader_LTLIBRARIES = libomxfilereader.la

libomxfilereader_la_SOURCES = omx_filereader_component.c omx_filereader_component.h \
                              library_entry_point.c

libomxfilereader_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la $(FFMPEG_LIBS)
libomxfilereader_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxfilereader_la_CFLAGS = $(FFMPEG_CFLAGS) \
                             -I$(top_srcdir)/include \
                             -I$(top_srcdir)/src \
                             -I$(top_srcdir)/src/base
