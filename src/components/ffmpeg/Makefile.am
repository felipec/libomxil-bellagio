omxffmpegdir = $(libdir)/omxilcomponents

omxffmpeg_LTLIBRARIES=libomxffmpeg.la

libomxffmpeg_la_SOURCES = omx_audiodec_component.c library_entry_point.c omx_videodec_component.c omx_ffmpeg_colorconv_component.c
noinst_HEADERS = omx_audiodec_component.h omx_videodec_component.h omx_ffmpeg_colorconv_component.h
libomxffmpeg_la_LIBADD = -lavcodec -lavformat -lavutil $(top_builddir)/src/libomxil.la

if WITH_FFMPEGCOMPONENTS_2
libomxffmpeg_la_CFLAGS = -DFFMPEG_DECODER_VERSION=2
endif

INCLUDES = -I../../../include/ -I../../ -I./ -I../../base/
