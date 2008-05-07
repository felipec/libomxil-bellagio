omxalsadir = $(libdir)/omxilcomponents
omxalsa_LTLIBRARIES = libomxalsa.la
libomxalsa_la_SOURCES = omx_alsasink_component.c \
                        library_entry_point.c \
                        omx_alsasink_component.h \
                        omx_alsasrc_component.c \
                        omx_alsasrc_component.h
AM_CPPFLAGS = -I@top_srcdir@/include -I@top_srcdir@/src -I@top_srcdir@/src/base
libomxalsa_la_LIBADD = ../../../src/libomxil.la $(LIBS) -lasound
libomxalsa_la_LDFLAGS = $(PLUGIN_LDFLAGS)

