omxfilereaderdir = $(libdir)/omxilcomponents

omxfilereader_LTLIBRARIES = libomxfilereader.la

libomxfilereader_la_SOURCES = omx_filereader_component.c library_entry_point.c omx_filereader_component.h
libomxfilereader_la_LIBADD = ../../libomxil.la $(LIBS)

AM_CPPFLAGS = -I@top_srcdir@/include -I@top_srcdir@/src -I@top_srcdir@/src/base
