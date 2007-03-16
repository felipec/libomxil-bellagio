AUTOMAKE_OPTIONS = foreign
SUBDIRS = src include test
EXTRA_DIST = libomxil.spec doxy_config
libomxildocdir = ${prefix}/doc/omxil
libomxildoc_DATA = \
        README\
        ChangeLog\
        TODO

pkgconfigexecdir        = $(libdir)/pkgconfig
pkgconfigexec_DATA      = libomxil.pc
 
$(pkgconfigexec_DATA): config.status

clean-local:
	sudo make uninstall
	rm -f ~/.omxregistry
