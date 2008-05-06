omxcameradir = $(libdir)/omxilcomponents

omxcamera_LTLIBRARIES=libomxcamera.la

libomxcamera_la_SOURCES = omx_camera_source_component.c library_entry_point.c 
noinst_HEADERS = omx_camera_source_component.h
libomxcamera_la_LIBADD = $(top_builddir)/src/libomxil.la
libomxcamera_la_LDFLAGS = -module

INCLUDES = -I../../../include/ -I../../ -I./ -I../../base/

