ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src include test m4

EXTRA_DIST = libomxil.spec Doxyfile cross-env

libomxildocdir = $(pkgdatadir)/doc/omxil
libomxildoc_DATA = \
        README \
        ChangeLog \
        TODO

pkgconfigdir        = $(libdir)/pkgconfig
pkgconfig_DATA      = libomxil.pc
 
$(pkgconfig_DATA): config.status

uninstall-local:
	rm -f ~/.omxregistry
