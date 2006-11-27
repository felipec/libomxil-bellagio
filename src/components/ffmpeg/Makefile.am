omxffmpegdir = $(libdir)/omxilcomponents
omxffmpeg_LTLIBRARIES = libomxffmpeg.la
libomxffmpeg_la_SOURCES = omx_mp3dec_component.c
INCLUDES = -I../../../include/ -I../../ -I./ -I../../base/
libomxffmpeg_la_LIBADD = -lavcodec -lavformat $(top_builddir)/src/libomxil.la

noinst_HEADERS = omx_mp3dec_component.h
