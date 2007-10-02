SUBDIRS = src include test m4

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = libomxil-B.spec Doxyfile 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libomxil-B.pc

docdir = $(pkgdatadir)/doc/omxil
doc_DATA = README \
   ChangeLog \
   TODO
	
uninstall-local:
	rm -f ~/.omxregistry
	rm -rf $(libdir)/omxilcomponents/
