check_PROGRAMS = omxaudiodectest omxaudiocapnplay

bellagio_LDADD = $(top_builddir)/src/libomxil-bellagio.la
bellagio_CFLAGS = -I$(top_srcdir)/include
common_LDADD = $(builddir)/../common/libtestcommon.la
common_CFLAGS = -I$(srcdir)/../common

omxaudiodectest_SOURCES = omxaudiodectest.c omxaudiodectest.h
omxaudiodectest_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxaudiodectest_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)

omxaudiocapnplay_SOURCES = omxaudiocapnplay.c omxaudiocapnplay.h
omxaudiocapnplay_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxaudiocapnplay_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)
