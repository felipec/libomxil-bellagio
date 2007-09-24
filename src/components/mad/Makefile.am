omxmaddir = $(libdir)/omxilcomponents

omxmad_LTLIBRARIES = libomxmad.la

libomxmad_la_SOURCES = omx_maddec_component.c library_entry_point.c omx_maddec_component.h
libomxmad_la_LIBADD = ../../libomxil.la $(LIBS) -lmad -lid3tag
                                                                                                                             
AM_CPPFLAGS = -I@top_srcdir@/include -I@top_srcdir@/src -I@top_srcdir@/src/base
