omxmaddir = $(libdir)/omxilcomponents

omxmad_LTLIBRARIES = libomxmad.la

libomxmad_la_SOURCES = omx_maddec_component.c library_entry_point.c omx_maddec_component.h
libomxmad_la_LIBADD = $(top_builddir)/src/libomxil.la $(MAD_LIBS)
                                                                                                                             
libomxmad_la_CFLAGS = $(MAD_CFLAGS) \
		      -I$(top_srcdir)/include \
		      -I$(top_srcdir)/src \
		      -I$(top_srcdir)/src/base
