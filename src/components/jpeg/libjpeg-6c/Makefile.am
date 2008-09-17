bin_PROGRAMS = cjpeg djpeg jpegtran rdjpgcom wrjpgcom
#djpeg

cjpeg_SOURCES = cjpeg.c rdppm.c rdgif.c rdtarga.c rdbmp.c rdrle.c rdswitch.c cdjpeg.c \
                cdjpeg.h jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h cderror.h
cjpeg_LDADD = libjpeg.la

djpeg_SOURCES = djpeg.c wrppm.c wrgif.c wrtarga.c wrbmp.c wrrle.c rdcolmap.c cdjpeg.c \
                cdjpeg.h jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h cderror.h
djpeg_LDADD = libjpeg.la

jpegtran_SOURCES = jpegtran.c transupp.c cdjpeg.c rdswitch.c \
                cdjpeg.h jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h cderror.h
jpegtran_LDADD = libjpeg.la

rdjpgcom_SOURCES = rdjpgcom.c
#cjpeg.c rdppm.c cdjpeg.c wrppm.c rdgif.c wrgif.c rdtarga.c wrtarga.c rdbmp.c rdswitch.c 
#                cdjpeg.h jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h cderror.h
rdjpgcom_LDADD = 

wrjpgcom_SOURCES = wrjpgcom.c
#cjpeg.c rdppm.c cdjpeg.c wrppm.c rdgif.c wrgif.c rdtarga.c wrtarga.c rdbmp.c rdswitch.c 
#                cdjpeg.h jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h cderror.h
wrjpgcom_LDADD = 

LIBSOURCES= jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c \
        jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c \
        jcparam.c jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c \
        jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c \
        jdinput.c jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdpostct.c \
        jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c jfdctint.c \
        jidctflt.c jidctfst.c jidctint.c jquant1.c jquant2.c \
        jutils.c jmemmgr.c jaricom.c jcarith.c jdarith.c
# memmgr back ends: compile only one of these into a working library
SYSDEPSOURCES= jmemansi.c 
#jmemname.c jmemnobs.c jmemdos.c jmemmac.c
# source files: cjpeg/djpeg/jpegtran applications, also rdjpgcom/wrjpgcom
APPSOURCES= cjpeg.c djpeg.c jpegtran.c rdjpgcom.c wrjpgcom.c cdjpeg.c \
        rdcolmap.c rdswitch.c transupp.c rdppm.c wrppm.c rdgif.c wrgif.c \
        rdtarga.c wrtarga.c rdbmp.c wrbmp.c rdrle.c wrrle.c

lib_LTLIBRARIES = libjpeg.la

libjpeg_la_SOURCES = $(LIBSOURCES) $(SYSDEPSOURCES)

include_extradir = $(includedir)

include_extra_HEADERS =  jconfig.h jpeglib.h jmorecfg.h jerror.h \
                         jpegint.h jdct.h jversion.h jmemsys.h \
												 transupp.h
