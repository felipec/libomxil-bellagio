omxfilereaderdir = $(libdir)/omxilcomponents

omxfilereader_LTLIBRARIES=libomxfilereader.la

libomxfilereader_la_SOURCES = omx_filereader_component.c library_entry_point.c
noinst_HEADERS = omx_filereader_component.h
libomxfilereader_la_LIBADD = -lavformat $(top_builddir)/src/libomxil.la

INCLUDES = -I../../../include/ -I../../ -I./ -I../../base/

