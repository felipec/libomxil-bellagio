check_PROGRAMS = omxaudiodectest omxaudiocapnplay

omxaudiodectest_SOURCES = omxaudiodectest.c omxaudiodectest.h
omxaudiodectest_LDADD = $(top_builddir)/src/libomxil.la ../common/libtestcommon.la -lpthread
omxaudiodectest_CFLAGS= -I$(srcdir)/../common -I$(top_srcdir)/include

omxaudiocapnplay_SOURCES = omxaudiocapnplay.c omxaudiocapnplay.h
omxaudiocapnplay_LDADD = $(top_builddir)/src/libomxil.la ../common/libtestcommon.la -lpthread
omxaudiocapnplay_CFLAGS= -I$(srcdir)/../common -I$(top_srcdir)/include
