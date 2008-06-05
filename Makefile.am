SUBDIRS = src include test m4 doc

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = libomxil-bellagio.spec

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libomxil-bellagio.pc

docdir = $(DESTDIR)$(prefix)/share/doc/@PACKAGE@
doc_DATA = README \
	   ChangeLog \
	   TODO

uninstall-local:
	rm -rf $(plugindir)
