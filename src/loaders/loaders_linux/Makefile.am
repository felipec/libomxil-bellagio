stbaseloaderdir = $(plugindir)

stbaseloader_LTLIBRARIES = libstbaseloader.la

libstbaseloader_la_SOURCES = st_base_component_loader.c st_base_component_loader.h

libstbaseloader_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la
libstbaseloader_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libstbaseloader_la_CFLAGS = -I$(top_srcdir)/include \
		       -I$(top_srcdir)/src \
		       -I$(top_srcdir)/src/base

