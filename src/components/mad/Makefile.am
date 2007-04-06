omxmaddir = $(libdir)/omxilcomponents
                                                                                                                             
omxmad_LTLIBRARIES=libomxmad.la
                                                                                                                             
libomxmad_la_SOURCES = omx_maddec_component.c library_entry_point.c
noinst_HEADERS = omx_maddec_component.h
libomxmad_la_LIBADD = -lmad -lid3tag $(top_builddir)/src/libomxil.la
                                                                                                                             
INCLUDES = -I../../../include/ -I../../ -I./ -I../../base/

