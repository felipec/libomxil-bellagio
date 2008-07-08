check_PROGRAMS = omxclocksrctest

bellagio_LDADD = $(top_builddir)/src/libomxil-bellagio.la
bellagio_CFLAGS = -I$(top_srcdir)/include
common_LDADD = ../common/libtestcommon.la
common_CFLAGS = -I$(srcdir)/../common

omxclocksrctest_SOURCES = omxclocksrctest.c omxclocksrctest.h
omxclocksrctest_LDADD   = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxclocksrctest_CFLAGS  = $(bellagio_CFLAGS) $(common_CFLAGS)
