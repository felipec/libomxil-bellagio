omxaudio_effectsdir = $(plugindir)

omxaudio_effects_LTLIBRARIES = libomxaudio_effects.la

libomxaudio_effects_la_SOURCES = omx_volume_component.c omx_volume_component.h \
                                 omx_audiomixer_component.c omx_audiomixer_component.h \
                                 library_entry_point.c

libomxaudio_effects_la_LIBADD = $(top_builddir)/src/libomxil-bellagio.la
libomxaudio_effects_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libomxaudio_effects_la_CFLAGS = -I$(top_srcdir)/include \
				-I$(top_srcdir)/src \
				-I$(top_srcdir)/src/base
