omxffmpegdir = $(plugindir)

omxffmpeg_LTLIBRARIES = libomxffmpeg.la

libomxffmpeg_la_SOURCES = omx_audiodec_component.c omx_audiodec_component.h \
                          omx_videodec_component.c omx_videodec_component.h \
                          omx_videoenc_component.c omx_videoenc_component.h \
                          omx_ffmpeg_colorconv_component.c omx_ffmpeg_colorconv_component.h \
                          library_entry_point.c

libomxffmpeg_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la $(FFMPEG_LIBS)
libomxffmpeg_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxffmpeg_la_CFLAGS = $(FFMPEG_CFLAGS) \
                         -I$(top_srcdir)/include \
                         -I$(top_srcdir)/src \
                         -I$(top_srcdir)/src/base
