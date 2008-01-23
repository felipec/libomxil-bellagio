omxparser3gpdir = $(libdir)/omxilcomponents

omxparser3gp_LTLIBRARIES = libomxparser3gp.la

libomxparser3gp_la_SOURCES = omx_parser3gp_component.c library_entry_point.c omx_parser3gp_component.h
libomxparser3gp_la_LIBADD = ../../libomxil.la $(LIBS) -lavformat

AM_CPPFLAGS = -I@top_srcdir@/include -I@top_srcdir@/src -I@top_srcdir@/src/base
