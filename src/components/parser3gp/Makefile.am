omxparser3gpdir = $(plugindir)

omxparser3gp_LTLIBRARIES = libomxparser3gp.la

libomxparser3gp_la_SOURCES = omx_parser3gp_component.c omx_parser3gp_component.h \
			     library_entry_point.c

libomxparser3gp_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la $(FFMPEG_LIBS)
libomxparser3gp_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxparser3gp_la_CFLAGS = -I$(top_srcdir)/include \
			    -I$(top_srcdir)/src \
			    -I$(top_srcdir)/src/base \
                            $(FFMPEG_CFLAGS)
