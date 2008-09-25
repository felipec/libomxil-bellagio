SUBDIRS = src include test m4 doc

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = libomxil-bellagio.spec \
             \
             sis/createandsign.bat \
             sis/libomxil.pkg \
             \
             group/bld.inf  \
             group/libomxil.mmp  \
             group/readme.txt \
             \
             src/components/components_symbian/audiomixer/data/F01F0015.rss \
             src/components/components_symbian/audiomixer/group/BellagioOpenMaxAudioMixer.mmp \
             src/components/components_symbian/audiomixer/inc/BellagioOpenMaxAudioMixer.h \
             src/components/components_symbian/audiomixer/inc/BellagioOpenMaxAudioMixerImplUid.hrh \
             src/components/components_symbian/audiomixer/sis/BellagioOpenMaxAudioMixer.pkg \
             src/components/components_symbian/audiomixer/src/BellagioOpenMaxAudioMixer.cpp \
             src/components/components_symbian/audiomixer/src/BellagioOpenMaxAudioMixerImplFactory.cpp \
             \
             src/components/components_symbian/inc/BellagioOpenMaxComponent.h \
             src/components/components_symbian/inc/BellagioOpenMaxComponentUid.hrh \
             \
             src/components/components_symbian/outputstream/data/F01F0013.rss \
             src/components/components_symbian/outputstream/group/BellagioOpenMaxOutputStream.mmp \
             src/components/components_symbian/outputstream/inc/BellagioOpenMaxOutputStream.h \
             src/components/components_symbian/outputstream/inc/BellagioOpenMaxOutputStreamImplUid.hrh \
             src/components/components_symbian/outputstream/sis/BellagioOpenMaxOutputStream.pkg \
             src/components/components_symbian/outputstream/src/BellagioOpenMaxOutputStream.cpp \
             src/components/components_symbian/outputstream/src/BellagioOpenMaxOutputStreamImplFactory.cpp \
             src/components/components_symbian/outputstream/src/omx_symbian_output_stream.cpp \
             src/components/components_symbian/outputstream/src/omx_symbian_output_stream.h \
             src/components/components_symbian/outputstream/src/omx_symbianoutputstreamsink_component.c \
             src/components/components_symbian/outputstream/src/omx_symbianoutputstreamsink_component.h \
             src/components/components_symbian/outputstream/src/omx_symbian_output_stream_wrapper.cpp \
             src/components/components_symbian/outputstream/src/omx_symbian_output_stream_wrapper.h \
             \
             src/components/components_symbian/src/BellagioOpenMaxComponent.cpp \
             \
             src/components/components_symbian/volume/data/F01F0055.rss \
             src/components/components_symbian/volume/group/BellagioOpenMaxVolume.mmp \
             src/components/components_symbian/volume/inc/BellagioOpenMaxVolume.h \
             src/components/components_symbian/volume/inc/BellagioOpenMaxVolumeImplUid.hrh \
             src/components/components_symbian/volume/sis/BellagioOpenMaxVolume.pkg \
             src/components/components_symbian/volume/src/BellagioOpenMaxVolume.cpp \
             src/components/components_symbian/volume/src/BellagioOpenMaxVolumeImplFactory.cpp \
             \
             src/loaders/loaders_symbian/inc/BellagioOpenMaxLoader.h \
             src/loaders/loaders_symbian/inc/BellagioOpenMaxLoaderUid.hrh \
             \
             src/loaders/loaders_symbian/src/BellagioOpenMaxLoader.cpp \
             src/loaders/loaders_symbian/src/omx_create_loaders_symbian.cpp \
             \
             src/loaders/loaders_symbian/symbianecomloader/data/F01F0033.rss \
             \
             src/loaders/loaders_symbian/symbianecomloader/group/BellagioOpenMaxSymbianLoader.mmp \
             src/loaders/loaders_symbian/symbianecomloader/inc/BellagioOpenMaxSymbianLoader.h \
             src/loaders/loaders_symbian/symbianecomloader/inc/BellagioOpenMaxSymbianLoaderImplUid.hrh \
             src/loaders/loaders_symbian/symbianecomloader/inc/nrc_symbian_component_loader.h \
             src/loaders/loaders_symbian/symbianecomloader/sis/BellagioOpenMaxSymbianLoader.pkg \
             src/loaders/loaders_symbian/symbianecomloader/src/BellagioOpenMaxSymbianLoader.cpp \    
             src/loaders/loaders_symbian/symbianecomloader/src/nrc_symbian_component_loader.cpp \
             src/loaders/loaders_symbian/symbianecomloader/src/BellagioOpenMaxSymbianLoaderImplFactory.cpp \
             \
             test/components/components_symbian/symbianaudiomixer/data/omxaudiomixertest_reg.rss \
             test/components/components_symbian/symbianaudiomixer/group/OmxAudioMixerTest.mmp \
             test/components/components_symbian/symbianaudiomixer/inc/omxaudiomixertest.h \
             test/components/components_symbian/symbianaudiomixer/sis/OmxAudioMixerTest.pkg \
             test/components/components_symbian/symbianaudiomixer/src/omxaudiomixertest.c \
             \
             test/components/components_symbian/symbiancompnameenum/data/omxcompnameenumtest_reg.rss \
             test/components/components_symbian/symbiancompnameenum/group/OmxCompNameEnumTest.mmp \
             test/components/components_symbian/symbiancompnameenum/sis/OmxCompNameEnumTest.pkg \
             test/components/components_symbian/symbiancompnameenum/src/omxcompnameenumtest.c \
             \
             test/components/components_symbian/symbianoutputstream/data/omxsymbianoutputstreamtest_reg.rss \
             test/components/components_symbian/symbianoutputstream/group/OmxSymbianOutputStreamTest.mmp \
             test/components/components_symbian/symbianoutputstream/inc/omxsymbianoutputstreamtest.h \
             test/components/components_symbian/symbianoutputstream/sis/OmxSymbianOutputStreamTest.pkg \
             test/components/components_symbian/symbianoutputstream/src/omxsymbianoutputstreamtest.c

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libomxil-bellagio.pc

docdir = $(DESTDIR)$(prefix)/share/doc/@PACKAGE@
doc_DATA = README \
	   ChangeLog \
	   TODO

uninstall-local:
	rm -rf $(plugindir)
