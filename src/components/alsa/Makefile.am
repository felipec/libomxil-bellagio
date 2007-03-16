omxalsadir = $(libdir)/omxilcomponents
omxalsa_LTLIBRARIES = libomxalsa.la
libomxalsa_la_SOURCES = omx_alsasink_component.c omx_alsasink_library_entry_point.c
INCLUDES = -I../../../include/ -I../../ -I./ -I../../base/
libomxalsa_la_LIBADD = -lasound $(top_builddir)/src/libomxil.la
noinst_HEADERS = omx_alsasink_component.h
