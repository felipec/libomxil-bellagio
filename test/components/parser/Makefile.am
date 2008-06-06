check_PROGRAMS = omxparsertest

bellagio_LDADD = $(top_builddir)/src/libomxil-bellagio.la
bellagio_CFLAGS = -I$(top_srcdir)/include
common_LDADD = ../common/libtestcommon.la
common_CFLAGS = -I$(srcdir)/../common

omxparsertest_SOURCES = omxparsertest.c omxparsertest.h
omxparsertest_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxparsertest_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)
