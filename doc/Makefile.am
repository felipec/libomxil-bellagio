man_MANS = omxregister-bellagio.1

EXTRA_DIST = Doxyfile.in footer_SF.html.in

# ensure doxygen-build.stamp is included, or else the documentation will
# not be built or distributed
noinst_DATA = doxygen-build.stamp

if HAVE_DOXYGEN
#doxygen-build.stamp: Doxyfile $(top_srcdir)
doxygen-build.stamp: Doxyfile $(top_srcdir)/include/*.h $(top_srcdir)/src/*.h $(top_srcdir)/src/base/*.h
	doxygen
	touch doxygen-build.stamp
else
doxygen-build.stamp:
	echo "*** Warning: Doxygen not found; documentation will not be built."
	touch doxygen-build.stamp
endif

dist_docdir = $(distdir)/$(PACKAGE)

dist-hook:
	if test -d $(PACKAGE); then \
	  mkdir $(dist_docdir); \
	  for dir in $(PACKAGE)/*; do \
	    b=`basename $$dir`; \
	    if test -d $$dir; then \
	      mkdir $(dist_docdir)/$$b; \
	      for f in $$dir/*; do \
	        cp -p $$f $(dist_docdir)/$$b; \
	      done \
	    fi \
	  done \
	fi


install-data-local: doxygen-build.stamp
	$(mkinstalldirs) $(DESTDIR)$(docdir)
	if test -d $(PACKAGE); then \
	  for dir in $(PACKAGE)/*; do \
	    if test -d $$dir; then \
	      b=`basename $$dir`; \
	      $(mkinstalldirs) $(DESTDIR)$(docdir)/$$b; \
	      for f in $$dir/*; do \
	        $(INSTALL_DATA) $$f $(DESTDIR)$(docdir)/$$b; \
	      done \
	    fi \
	  done \
	fi

uninstall-local:
	rm -rf $(DESTDIR)$(docdir)

clean-local:
	if test -d $(PACKAGE); then rm -rf $(PACKAGE); fi
	if test -f doxygen-build.stamp; then rm -f doxygen-build.stamp; fi

