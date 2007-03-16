INCLUDES= -I../../../include/ -I../../../src
check_PROGRAMS = omxmp3dectest omxmp3convertest omx11audiodectest omxnewaudiodectest

#if using ../src/libomxil.la there is no need to
#install the library and everything is resolved
#by means of a shell script.
#if using -lomxil the libomxil library must be
#installed
#omxdummytest_LDADD=../src/libomxil.la -lpthread
omx11audiodectest_LDADD = -lomxil -lpthread -lasound
omxnewaudiodectest_LDADD = -lomxil -lpthread -lasound
omxmp3dectest_LDADD = -lomxil -lpthread -lasound
omxmp3convertest_LDADD = -lomxil -lpthread -lasound

#EXTRA_DIST=omxmp3dectest.h omxmp3convertest.h omx11audiodectest.h omxnewaudiodectest.h

noinst_HEADERS= omxmp3dectest.h omxmp3convertest.h omx11audiodectest.h omxnewaudiodectest.h
