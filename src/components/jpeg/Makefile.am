SUBDIRS = libjpeg-6c

omximagejpegdir = $(plugindir)

omximagejpeg_LTLIBRARIES = libomximagejpeg.la

libomximagejpeg_la_SOURCES = omx_jpegdec_component.c \
                              omx_jpegenc_component.c \
                              library_entry_point.c \
                              wrbmp.c \
                              rdbmp.c \
                              omx_jpegdec_component.h \
                              omx_jpegenc_component.h

libomximagejpeg_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la libjpeg-6c/libjpeg.la
libomximagejpeg_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomximagejpeg_la_CFLAGS = -I$(top_srcdir)/include \
                             -I$(top_srcdir)/src \
                             -I$(top_srcdir)/src/base  \
                             -I./libjpeg-6c 
