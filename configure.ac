AC_INIT([OpenMAX Bellagio], [0.3.5-svn], [], [libomxil-bellagio])
AM_INIT_AUTOMAKE

# Prerequisite autoconf version
AC_PREREQ([2.59])

AC_CONFIG_HEADERS([config.h])

# Check if the OMX_Core.h file is present
AC_CONFIG_SRCDIR([include/OMX_Core.h])

# Set to 'm4' the directory where the extra autoconf macros are stored
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_FILES([
    Doxyfile
    Makefile
    footer_SF.html
    libomxil-bellagio.pc
    libomxil-bellagio.spec
    doc/omxregister-bellagio.1
    doc/Makefile
    include/Makefile
    src/Makefile
    src/base/Makefile
    src/components/Makefile
    src/components/alsa/Makefile
    src/components/ffmpeg/Makefile
    src/components/mad/Makefile
    src/components/vorbis/Makefile
    src/components/filereader/Makefile
    src/components/fbdev/Makefile
    src/components/audio_effects/Makefile
    src/components/video_src/Makefile
    src/components/parser3gp/Makefile
    src/components/camera/Makefile
    m4/Makefile
    test/Makefile
    test/components/Makefile
    test/components/common/Makefile
    test/components/audio/Makefile
    test/components/video/Makefile
    test/components/audio_effects/Makefile
    test/components/parser/Makefile
    test/components/camera/Makefile
])
################################################################################
# Define the extra arguments the user can pass to the configure script         #
################################################################################

# Check whether ffmpeg components have been requested
AC_ARG_ENABLE(
    [ffmpegcomponents],
    [AC_HELP_STRING(
        [--disable-ffmpegcomponents],
        [whether to disable ffmpeg components compilation])],
    [with_ffmpeg=$enableval],
    [with_ffmpeg=yes])

# Check whether mad components have been requested
AC_ARG_ENABLE(
    [madcomponents],
    [AC_HELP_STRING(
        [--disable-madcomponents],
        [whether to disable mad components compilation])],
    [with_mad=$enableval],
    [with_mad=yes])

# Check whether vorbis components have been requested
AC_ARG_ENABLE(
    [vorbiscomponents],
    [AC_HELP_STRING(
        [--disable-vorbiscomponents],
        [whether to disable vorbis components compilation])],
    [with_vorbis=$enableval],
    [with_vorbis=yes])

# Check whether volume component has been requested
AC_ARG_ENABLE(
    [audioeffects],
    [AC_HELP_STRING(
        [--disable-audioeffects],
        [whether to disable the audio effects components compilation])],
    [with_audioeffects=$enableval],
    [with_audioeffects=yes])

#Check whether fbdev_videosink component has been requested
AC_ARG_ENABLE(
    [fbvideosink],
    [AC_HELP_STRING(
        [--enable-fbvideosink],
        [whether to enable the fbdev video sink])],
    [with_fbdev_videosink=$enableval],
    [with_fbdev_videosink=no])

#Check whether ALSA components have been requested
AC_ARG_ENABLE(
    [alsa],
    [AC_HELP_STRING(
        [--disable-alsa],
        [whether to disable the alsa components])],
    [with_alsa=$enableval],
    [with_alsa=yes])

#Check whether video src/camera component has been requested
AC_ARG_ENABLE(
    [videosrc],
    [AC_HELP_STRING(
        [--enable-videosrc],
        [whether to enable the video src/camera])],
    [with_videosrc=$enableval],
    [with_videosrc=no])

#Check whether to disable all components
AC_ARG_ENABLE(
    [components],
    [AC_HELP_STRING(
        [--disable-components],
        [whether to disable all the components])],
    [with_components=$enableval],
    [with_components=yes])

################################################################################
# Check for programs                                                           #
################################################################################

# Check for a working C compiler
AC_PROG_CC
AM_PROG_CC_C_O

# Check for libtool
AM_PROG_LIBTOOL

################################################################################
# Check for libraries                                                          #
################################################################################

# Check for POSIX thread support
ACX_PTHREAD([
                LIBS="$LIBS $PTHREAD_LIBS"
                CFLAGS="$CFLAGS $PTHREAD_CFLAGS -g -Wall"
                CC="$PTHREAD_CC"
                AC_SUBST([LIBS])
                AC_SUBST([CFLAGS])
                AC_SUBST([CC])
            ],
            [AC_MSG_ERROR([POSIX threads support is required])])

# Check for libdl
AC_SEARCH_LIBS([dlopen], [dl], [], [AC_MSG_ERROR([libdl is required])])

if test "x$with_components" = "xno"; then
	with_alsa=no
	with_vorbis=no
	with_mad=no
	with_ffmpeg=no
	with_fbdev_videosink=no
	with_audioeffects=no
	with_videosrc=no
fi

# Define components default ldflags (man ld)
PLUGIN_LDFLAGS="-module -avoid-version -no-undefined -as-needed"
AC_SUBST(PLUGIN_LDFLAGS)

AX_SET_PLUGINDIR

################################################################################
# Check for header files                                                       #
################################################################################

# Check if V4L header file is present
if test "x$with_videosrc" = "xyes"; then
	AC_CHECK_HEADER([linux/videodev2.h], [with_videosrc=yes], [with_videosrc=no])
fi

# Check if Video Frame buffer header file is present
if test "x$with_fbdev_videosink" = "xyes"; then
	AC_CHECK_HEADER([linux/fb.h], [with_fbdev_videosink=yes], [with_fbdev_videosink=no])
fi

################################################################################
# Check for pkg-config modules                                                 #
################################################################################

# Check for vorbis component dependencies
if test "x$with_alsa" = "xyes"; then
	PKG_CHECK_MODULES([ALSA], [alsa], [with_alsa=yes], [with_alsa=no])
fi

# Check for vorbis component dependencies
if test "x$with_vorbis" = "xyes"; then
	PKG_CHECK_MODULES([VORBIS], [vorbis], [with_vorbis=yes], [with_vorbis=no])
fi

# Check for mad component dependencies
if test "x$with_mad" = "xyes"; then
	PKG_CHECK_MODULES([MAD], [mad id3tag], [with_mad=yes], [with_mad=no])
fi

# Check for FFmpeg component dependencies
if test "x$with_ffmpeg" = "xyes"; then
	PKG_CHECK_MODULES([FFMPEG], [libavcodec libavformat libavutil libswscale], [with_ffmpeg=yes], [with_ffmpeg=no])
fi

# Check for FFmpeg API version
if test "x$with_ffmpeg" = "xyes"; then
	AC_DEFINE([FFMPEG_DECODER_VERSION], [1], [FFmpeg audio deocder API])
	old_CPPFLAGS="${CPPFLAGS}"
	old_LDFLAGS="${LDFLAGS}"
	CPPFLAGS="${CPPFLAGS} ${FFMPEG_CFLAGS}"
	LDFLAGS="${LDFLAGS} ${FFMPEG_LIBS}"
	AC_CHECK_LIB([avcodec], [avcodec_decode_audio2],
		     [AC_DEFINE([FFMPEG_DECODER_VERSION], [2])])
	CPPFLAGS="${old_CPPFLAGS}"
	LDFLAGS="${old_LDFLAGS}"
fi

# Check for vorbis library existence
if test "x$with_vorbis" = "xyes"; then
	old_CPPFLAGS="${CPPFLAGS}"
	old_LDFLAGS="${LDFLAGS}"
	CPPFLAGS="${CPPFLAGS} ${VORBIS_CFLAGS}"
	LDFLAGS="${LDFLAGS} ${VORBIS_LIBS}"
	AC_CHECK_HEADER([vorbis/codec.h], [with_vorbis=yes], [with_vorbis=no])
	CPPFLAGS="${old_CPPFLAGS}"
	LDFLAGS="${old_LDFLAGS}"
fi

################################################################################
# Check for types                                                              #
################################################################################

# unused

################################################################################
# Check for structures                                                         #
################################################################################

# unused

################################################################################
# Check for compiler characteristics                                           #
################################################################################

# unused

################################################################################
# Check for library functions                                                  #
################################################################################

# unused

################################################################################
# Check for system services                                                    #
################################################################################

# unused

################################################################################
# Conditionals and file output                                                 #
################################################################################
AM_CONDITIONAL([WITH_V4L2_VIDEOSOURCE], [test x$with_videosrc = xyes])
AM_CONDITIONAL([WITH_FBDEV_VIDEOSINK], [test x$with_fbdev_videosink = xyes])
AM_CONDITIONAL([WITH_ALSA], [test x$with_alsa = xyes])
AM_CONDITIONAL([WITH_FFMPEGCOMPONENTS], [test x$with_ffmpeg = xyes])
AM_CONDITIONAL([WITH_MADCOMPONENTS], [test x$with_mad = xyes])
AM_CONDITIONAL([WITH_VORBISCOMPONENTS], [test x$with_vorbis = xyes])
AM_CONDITIONAL([WITH_AUDIOEFFECTS], [test x$with_audioeffects = xyes])
AM_CONDITIONAL([WITH_VIDEOSRC], [test x$with_videosrc = xyes])

AC_OUTPUT
