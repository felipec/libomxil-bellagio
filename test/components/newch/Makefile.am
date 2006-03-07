INCLUDES= -I../../../include/ -I../../../src
check_PROGRAMS = omxnewchtest

#if using ../src/libomxil.la there is no need to
#install the library and everything is resolved
#by means of a shell script.
#if using -lomxil the libomxil library must be
#installed
#omxdummytest_LDADD=../src/libomxil.la -lpthread
omxnewchtest_LDADD = -lomxil -lpthread
EXTRA_DIST=omxnewchtest.h
