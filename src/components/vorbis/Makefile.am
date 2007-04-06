omxvorbisdir = $(libdir)/omxilcomponents

omxvorbis_LTLIBRARIES=libomxvorbis.la

libomxvorbis_la_SOURCES = omx_vorbisdec_component.c library_entry_point.c
noinst_HEADERS = omx_vorbisdec_component.h
libomxvorbis_la_LIBADD = -lvorbis $(top_builddir)/src/libomxil.la

INCLUDES = -I../../../include/ -I../../ -I./ -I../../base/
