check_PROGRAMS = omxcameratest

bellagio_LDADD = $(top_builddir)/src/libomxil-bellagio.la
bellagio_CFLAGS = -I$(top_srcdir)/include
common_LDADD = ../common/libtestcommon.la
common_CFLAGS = -I$(srcdir)/../common

omxcameratest_SOURCES = omxcameratest.c omxcameratest.h
omxcameratest_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxcameratest_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)
