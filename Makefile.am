AUTOMAKE_OPTIONS = foreign
SUBDIRS = src include test
EXTRA_DIST = libomxil.spec Doxyfile
libomxildocdir = ${prefix}/doc/omxil
libomxildoc_DATA = \
        README\
        ChangeLog\
        TODO

pkgconfigexecdir        = $(libdir)/pkgconfig
pkgconfigexec_DATA      = libomxil.pc
 
$(pkgconfigexec_DATA): config.status

uninstall-local:
	rm -f ~/.omxregistry
