check_PROGRAMS = omxaudiodectest audio_use_case audio_use_case_tunnel omxaudiocapnplay omxvoicexchange

bellagio_LDADD = $(top_builddir)/src/libomxil-bellagio.la
bellagio_CFLAGS = -I$(top_srcdir)/include
common_LDADD = ../common/libtestcommon.la
common_CFLAGS = -I$(srcdir)/../common

omxaudiodectest_SOURCES = omxaudiodectest.c omxaudiodectest.h
omxaudiodectest_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxaudiodectest_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)

omxaudiocapnplay_SOURCES = omxaudiocapnplay.c omxaudiocapnplay.h
omxaudiocapnplay_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
omxaudiocapnplay_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)

audio_use_case_SOURCES = audio_use_case.c omxaudiodectest.h
audio_use_case_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
audio_use_case_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)

audio_use_case_tunnel_SOURCES = audio_use_case_tunnel.c omxaudiodectest.h
audio_use_case_tunnel_LDADD = $(bellagio_LDADD) $(common_LDADD) -lpthread
audio_use_case_tunnel_CFLAGS = $(bellagio_CFLAGS) $(common_CFLAGS)

omxvoicexchange_SOURCES = omxvoicexchange.c omxvoicexchange.h 
omxvoicexchange_LDADD =  $(bellagio_LDADD) $(common_LDADD) -lpthread 
omxvoicexchange_CFLAGS= $(bellagio_CFLAGS) $(common_CFLAGS)

