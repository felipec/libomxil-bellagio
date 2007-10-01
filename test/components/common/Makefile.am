EXTRA_LTLIBRARIES = libtestcommon.la

libtestcommon_la_SOURCES =  @top_srcdir@/test/components/common/tsemaphore.c \
 @top_srcdir@/test/components/common/tsemaphore.h \
 @top_srcdir@/test/components/common/user_debug_levels.h

check-local: libtestcommon.la

clean-local:
	rm -f libtestcommon.la
 