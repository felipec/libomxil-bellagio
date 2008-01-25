check_PROGRAMS = omxvolcontroltest omxaudiomixertest

omxvolcontroltest_SOURCES = omxvolcontroltest.c omxvolcontroltest.h
omxvolcontroltest_LDADD = $(top_builddir)/src/libomxil.la ../common/libtestcommon.la -lpthread
omxvolcontroltest_CFLAGS = -I$(srcdir)/../common -I$(top_srcdir)/include

omxaudiomixertest_SOURCES = omxaudiomixertest.c omxaudiomixertest.h
omxaudiomixertest_LDADD = $(top_builddir)/src/libomxil.la ../common/libtestcommon.la -lpthread
omxaudiomixertest_CFLAGS = -I$(srcdir)/../common -I$(top_srcdir)/include
