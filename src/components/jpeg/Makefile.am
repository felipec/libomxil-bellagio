omximage_jpegdir = $(plugindir)

omximage_jpeg_LTLIBRARIES = libomximage_jpeg.la

libomximage_jpeg_la_SOURCES = omx_jpegdec_component.c \
                              library_entry_point.c \
                              wrbmp.c \
                              omx_jpegdec_component.h \
                              cderror.h \
                              cdjpeg.h \
                              jinclude.h \
                              jversion.h

libomximage_jpeg_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la -ljpeg
libomximage_jpeg_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomximage_jpeg_la_CFLAGS = -I$(top_srcdir)/include \
				-I$(top_srcdir)/src \
				-I$(top_srcdir)/src/base
