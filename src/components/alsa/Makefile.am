omxalsadir = $(plugindir)

omxalsa_LTLIBRARIES = libomxalsa.la

libomxalsa_la_SOURCES = omx_alsasink_component.c omx_alsasink_component.h \
                        omx_alsasrc_component.c omx_alsasrc_component.h \
                        library_entry_point.c

libomxalsa_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la $(ALSA_LIBS)
libomxalsa_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxalsa_la_CFLAGS = -I$(top_srcdir)/include \
		       -I$(top_srcdir)/src \
		       -I$(top_srcdir)/src/base \
                       $(ALSA_CFLAGS)
