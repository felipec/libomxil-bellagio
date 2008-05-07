omxaudio_effectsdir = $(libdir)/omxilcomponents

omxaudio_effects_LTLIBRARIES = libomxaudio_effects.la

libomxaudio_effects_la_SOURCES = omx_volume_component.c \
                                 omx_audiomixer_component.c \
                                 library_entry_point.c \
                                 omx_volume_component.h \
                                 omx_audiomixer_component.h

libomxaudio_effects_la_LIBADD = ../../../src/libomxil.la
libomxaudio_effects_la_LDFLAGS = $(PLUGIN_LDFLAGS)

AM_CPPFLAGS = -I@top_srcdir@/include -I@top_srcdir@/src -I@top_srcdir@/src/base
