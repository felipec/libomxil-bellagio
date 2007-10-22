AC_INIT([OpenMAX Bellagio], [0.3.3], [], [libomxil-B])
AM_INIT_AUTOMAKE

# Prerequisite autoconf version
AC_PREREQ([2.59])

# Check if the OMX_Core.h file is present
AC_CONFIG_SRCDIR([include/OMX_Core.h])

# Set to 'm4' the directory where the extra autoconf macros are stored
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_FILES([
    Doxyfile
    Makefile
    libomxil-B.pc
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
    m4/Makefile
    test/Makefile
    test/components/Makefile
    test/components/common/Makefile
    test/components/audio/Makefile
    test/components/video/Makefile
    test/components/audio_effects/Makefile
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
    [with_ffmpegcomponents=false],
    [with_ffmpegcomponents=true]) dnl default

# Check whether mad components have been requested
AC_ARG_ENABLE(
    [madcomponents],
    [AC_HELP_STRING(
        [--disable-madcomponents],
        [whether to disable mad components compilation])],
    [with_madcomponents=false],
    [with_madcomponents=true]) dnl default

# Check whether vorbis components have been requested
AC_ARG_ENABLE(
    [vorbiscomponents],
    [AC_HELP_STRING(
        [--disable-vorbiscomponents],
        [whether to disable vorbis components compilation])],
    [with_vorbiscomponents=false],
    [with_vorbiscomponents=true]) dnl default

# Check whether volume component has been requested
AC_ARG_ENABLE(
    [audioeffects],
    [AC_HELP_STRING(
        [--disable-audioeffects],
        [whether to disable the audio effects components compilation])],
    [with_audioeffects=false],
    [with_audioeffects=true]) dnl default

#Check whether fbdev_videosink component has been requested
AC_ARG_ENABLE(
    [fbvideosink],
    [AC_HELP_STRING(
        [--enable-fbvideosink],
        [whether to enable the fbdev video sink])],
    [with_fbdev_videosink=true],
    [with_fbdev_videosink=false])

#Check whether alsa sink component has been requested
AC_ARG_ENABLE(
    [alsasink],
    [AC_HELP_STRING(
        [--disable-alsasink],
        [whether to disable the alsa sink])],
    [with_alsasink=false],
    [with_alsasink=true]) dnl default

#Enable the installation of all the header files
AC_ARG_ENABLE(
    [install_devel],
    [AC_HELP_STRING(
        [--enable-install-devel],
	[whether to install component development header files])],
    [install_devel=true],
    [install_devel=false]) dnl default

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

# Check if Alsa sound library is present
AC_CHECK_LIB(asound, snd_pcm_plugin_init, [with_alsalibrary=true], [with_alsalibrary=false])

#Check for ffmpeg (old version) library existence
AC_CHECK_LIB(avcodec, avcodec_decode_audio, [with_ffmpeglibrary=true], [with_ffmpeglibrary=false])

#Check for ffmpeg (new version) library existence
AC_CHECK_LIB(avcodec, avcodec_decode_audio2, [with_ffmpeglibrary_2=true], [with_ffmpeglibrary_2=false])

################################################################################
# Check for header files                                                       #
################################################################################

# Check if Video Frame buffer header file is present
AC_CHECK_HEADER([linux/fb.h], [with_framebuffer=true], [with_framebuffer=false])

# Check if Alsa sound library is present
AC_CHECK_HEADER([alsa/asoundlib.h], [], [with_alsalibrary=false])

#Check for ffmpeg library existence
AC_CHECK_HEADER([ffmpeg/avcodec.h], [], [with_ffmpeglibrary=false])
AC_CHECK_HEADER([ffmpeg/avformat.h], [], [with_ffmpeglibrary=false])
AC_CHECK_HEADER([ffmpeg/avutil.h], [], [with_ffmpeglibrary=false])
AC_CHECK_HEADER([ffmpeg/swscale.h], [], [with_ffmpeglibrary=false])

################################################################################
# Check for pkg-config modules                                                 #
################################################################################

dnl Check for vorbis component dependencies
PKG_CHECK_MODULES([VORBIS], [vorbis], have_vorbis=yes, have_vorbis=no)

# Check for mad component dependencies
PKG_CHECK_MODULES([MAD], [mad id3tag], have_mad=yes, have_mad=no)

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

AM_CONDITIONAL([WITH_FBDEV_VIDEOSINK], [test x$with_fbdev_videosink = xtrue && test x$with_framebuffer = xtrue])
AM_CONDITIONAL([WITH_ALSASINK], [test x$with_alsasink = xtrue && test x$with_alsalibrary = xtrue])
AM_CONDITIONAL([WITH_FFMPEGLIBRARY], [test x$with_ffmpeglibrary = xtrue])

AM_CONDITIONAL([WITH_FFMPEGLIBRARY_2], test x$with_ffmpeglibrary_2 = xtrue)
AM_CONDITIONAL([WITH_FFMPEGCOMPONENTS],
               [test x$with_ffmpegcomponents = xtrue && test x$with_ffmpeglibrary = xtrue])
AM_CONDITIONAL([WITH_FFMPEGCOMPONENTS_2],
               [test x$with_ffmpegcomponents = xtrue && test x$with_ffmpeglibrary_2 = xtrue])
AM_CONDITIONAL([WITH_MADCOMPONENTS],
               [test x$have_mad = xyes && test x$with_madcomponents = xtrue])
AM_CONDITIONAL([WITH_VORBISCOMPONENTS],
               [test x$have_vorbis = xyes && test x$with_vorbiscomponents = xtrue])
AM_CONDITIONAL([WITH_AUDIOEFFECTS], [test x$with_audioeffects = xtrue])

AM_CONDITIONAL([INSTALL_DEVEL_HEADERS], [test x$install_devel = xtrue])


AC_OUTPUT
