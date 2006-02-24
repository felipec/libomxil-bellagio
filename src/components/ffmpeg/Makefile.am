omxffmpegdir = $(libdir)/omxilcomponents

omxffmpeg_LTLIBRARIES=libomxffmpeg.la

libomxffmpeg_la_SOURCES = omx_ffmpeg_mp3dec.c
noinst_HEADERS = omx_ffmpeg_mp3dec.h
libomxffmpeg_la_LIBADD = -lpthread -lavcodec -lavformat

INCLUDES = -I../../../include/ -I../../ -I./
