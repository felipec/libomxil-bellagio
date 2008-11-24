check_PROGRAMS = omxmuxtest

bellagio_LDADD = $(top_builddir)/src/libomxil-bellagio.la
bellagio_CFLAGS = -I$(top_srcdir)/include
common_LDADD = ../common/libtestcommon.la
common_CFLAGS = -I$(srcdir)/../common

omxmuxtest_SOURCES = omxmuxtest.c omxmuxtest.h
omxmuxtest_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxmuxtest_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)
