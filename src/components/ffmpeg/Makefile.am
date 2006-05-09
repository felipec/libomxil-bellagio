omxffmpegdir = $(libdir)/omxilcomponents

omxffmpeg_LTLIBRARIES=libomxffmpeg.la

libomxffmpeg_la_SOURCES = omx_mp3dec_component.c
noinst_HEADERS = omx_mp3dec_component.h
libomxffmpeg_la_LIBADD = -lavcodec -lavformat

INCLUDES = -I../../../include/ -I../../ -I./ -I../../base/
