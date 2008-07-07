# Build order is important
# First base, then core, finally components
SUBDIRS = base . components

bin_PROGRAMS = omxregister-bellagio
omxregister_bellagio_SOURCES = omxregister.c common.c common.h
omxregister_bellagio_CFLAGS = -DOMXILCOMPONENTSPATH=\"$(plugindir)/\" \
			      -I$(top_srcdir)/include

lib_LTLIBRARIES = libomxil-bellagio.la
libomxil_bellagio_la_SOURCES = component_loader.h \
			       st_static_component_loader.c st_static_component_loader.h \
			       omxcore.c omxcore.h \
			       omx_create_loaders_linux.c omx_create_loaders.h \
			       omx_comp_debug_levels.h \
			       tsemaphore.c tsemaphore.h \
			       queue.c queue.h \
			       common.c common.h

libomxil_bellagio_la_CFLAGS = -I$(top_srcdir)/include -I$(srcdir)/base
libomxil_bellagio_la_LIBADD = base/libomxbase.la
libomxil_bellagio_la_LDFLAGS = -version-info @SHARED_VERSION_INFO@

include_extradir = $(includedir)/bellagio

include_extra_HEADERS = $(srcdir)/omxcore.h \
			$(srcdir)/queue.h \
			$(srcdir)/component_loader.h \
			$(srcdir)/st_static_component_loader.h \
			$(srcdir)/tsemaphore.h \
			$(srcdir)/omx_comp_debug_levels.h \
			$(srcdir)/base/omx_base_component.h \
			$(srcdir)/base/omx_base_port.h \
			$(srcdir)/base/omx_base_audio_port.h \
			$(srcdir)/base/omx_base_video_port.h \
			$(srcdir)/base/omx_base_filter.h \
			$(srcdir)/base/omx_base_sink.h \
			$(srcdir)/base/omx_base_source.h \
			$(srcdir)/base/omx_classmagic.h
