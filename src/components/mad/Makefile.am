omxmaddir = $(libdir)/omxilcomponents

omxmad_LTLIBRARIES=libomxmad.la

libomxmad_la_SOURCES = omx_maddec_component.c
noinst_HEADERS = omx_maddec_component.h
libomxmad_la_LIBADD = -lmad $(top_builddir)/src/libomxil.la

INCLUDES = -I../../../include/ -I../../ -I./ -I../../base/
