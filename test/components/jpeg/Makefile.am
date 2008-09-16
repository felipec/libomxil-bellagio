check_PROGRAMS = omxjpegdectest omxjpegenctest

bellagio_LDADD = $(top_builddir)/src/libomxil-bellagio.la
bellagio_CFLAGS = -I$(top_srcdir)/include
common_LDADD = ../common/libtestcommon.la
common_CFLAGS = -I$(srcdir)/../common

omxjpegdectest_SOURCES = omxjpegdectest.c omxjpegdectest.h
omxjpegdectest_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxjpegdectest_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)

omxjpegenctest_SOURCES = omxjpegenctest.c omxjpegenctest.h
omxjpegenctest_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxjpegenctest_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)
