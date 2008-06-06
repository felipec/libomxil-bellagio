check_PROGRAMS = omxvolcontroltest omxaudiomixertest

bellagio_LDADD = $(top_builddir)/src/libomxil-bellagio.la
bellagio_CFLAGS = -I$(top_srcdir)/include
common_LDADD = ../common/libtestcommon.la -lpthread
common_CFLAGS = -I$(srcdir)/../common

omxvolcontroltest_SOURCES = omxvolcontroltest.c omxvolcontroltest.h
omxvolcontroltest_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxvolcontroltest_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)

omxaudiomixertest_SOURCES = omxaudiomixertest.c omxaudiomixertest.h
omxaudiomixertest_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxaudiomixertest_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)
