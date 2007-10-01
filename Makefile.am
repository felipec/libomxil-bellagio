ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src include test m4

EXTRA_DIST = libomxil-B.spec Doxyfile 

libomxildocdir = $(pkgdatadir)/doc/omxil
libomxildoc_DATA = \
        README \
        ChangeLog \
        TODO

pkgconfigdir        = $(libdir)/pkgconfig
pkgconfig_DATA      = libomxil-B.pc
 
$(pkgconfig_DATA): config.status

uninstall-local:
	rm -f ~/.omxregistry
	rm -rf $(libdir)/omxilcomponents/

