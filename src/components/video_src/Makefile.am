omxvideosrcdir = $(libdir)/omxilcomponents

omxvideosrc_LTLIBRARIES = libomxvideosrc.la

libomxvideosrc_la_SOURCES = omx_videosrc_component.c \
			    library_entry_point.c \
			    omx_videosrc_component.h

libomxvideosrc_la_LIBADD = $(top_builddir)/src/libomxil.la 
libomxvideosrc_la_CFLAGS =   -I$(top_srcdir)/include \
			     -I$(top_srcdir)/src \
			     -I$(top_srcdir)/src/base
