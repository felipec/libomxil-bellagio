omxnewchdir = $(libdir)/omxilcomponents

omxnewch_LTLIBRARIES=libomxnewch.la

libomxnewch_la_LIBADD = -lomxil -lasound -lavcodec -lavformat

libomxnewch_la_SOURCES = omx_base_component.c \
						 omx_twoport_component.c \
						 omx_oneport_component.c \
						 omx_alsasink_component.c \
						 omx_volume_component.c \
						 omx_mp3dec_component.c
						 
noinst_HEADERS = omx_base_component.h \
				 omx_twoport_component.h \
				 omx_oneport_component.h \
				 omx_alsasink_component.h \
				 omx_volume_component.h \
				 omx_mp3dec_component.h

INCLUDES = -I../../../include/ -I../../ -I./

