check_PROGRAMS = omxvideodectest omxvideocapnplay omxvideoenctest

omxvideodectest_SOURCES = omxvideodectest.c omxvideodectest.h
omxvideodectest_LDADD = $(top_builddir)/src/libomxil-bellagio.la ../common/libtestcommon.la -lpthread
omxvideodectest_CFLAGS = -I$(srcdir)/../common -I$(top_srcdir)/include

omxvideocapnplay_SOURCES = omxvideocapnplay.c omxvideocapnplay.h
omxvideocapnplay_LDADD = $(top_builddir)/src/libomxil-bellagio.la ../common/libtestcommon.la -lpthread
omxvideocapnplay_CFLAGS = -I$(srcdir)/../common -I$(top_srcdir)/include

omxvideoenctest_SOURCES = omxvideoenctest.c omxvideoenctest.h
omxvideoenctest_LDADD = $(top_builddir)/src/libomxil-bellagio.la ../common/libtestcommon.la -lpthread
omxvideoenctest_CFLAGS = -I$(srcdir)/../common -I$(top_srcdir)/include
