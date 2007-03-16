omxffmpegdir = $(libdir)/omxilcomponents

omxffmpeg_LTLIBRARIES=libomxffmpeg.la

libomxffmpeg_la_SOURCES = omx_audiodec_component.c library_entry_point.c
noinst_HEADERS = omx_audiodec_component.h
libomxffmpeg_la_LIBADD = -lavcodec -lavformat $(top_builddir)/src/libomxil.la

INCLUDES = -I../../../include/ -I../../ -I./ -I../../base/
