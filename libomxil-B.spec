Name: libomxil-B
Version: 0.3.3
Release: 0
License: GNU LGPL
Group: System Environment/Libraries
Source: libomxil-B-0.3.3.tar.gz
Summary: OpenMAX Integration Layer 1.1 library and components.
Vendor: STMicroelectronics

%description
The OpenMAX IL API defines a standardized media component interface to
enable developers and platform providers to integrate and communicate
with multimedia codecs implemented in hardware or software.
 
The libomxil shared library implements the OpenMAX IL Core functionalities.
Three dynamically loadable components are also included: OMX alsa sink 
component, OMX mp3,aac,ogg decoder component and OMX volume control component.
(requires ffmpeg library, not part of this package).

%prep
%setup
%build
./configure
make

%install
make install

%clean

%files
%defattr(-,root,root)
/usr/local/bin/omxregister
/usr/local/doc/omxil/ChangeLog
/usr/local/doc/omxil/README
/usr/local/doc/omxil/TODO
/usr/local/include/OMX_Audio.h
/usr/local/include/OMX_Component.h
/usr/local/include/OMX_ContentPipe.h
/usr/local/include/OMX_Core.h
/usr/local/include/OMX_IVCommon.h
/usr/local/include/OMX_Image.h
/usr/local/include/OMX_Index.h
/usr/local/include/OMX_Other.h
/usr/local/include/OMX_Types.h
/usr/local/include/OMX_Video.h
/usr/local/lib/libomxil.a
/usr/local/lib/libomxil.la
/usr/local/lib/libomxil.so.0
/usr/local/lib/libomxil.so.0.0.0
/usr/local/lib/omxilcomponents/libomxalsa.a
/usr/local/lib/omxilcomponents/libomxalsa.la
/usr/local/lib/omxilcomponents/libomxalsa.so
/usr/local/lib/omxilcomponents/libomxalsa.so.0
/usr/local/lib/omxilcomponents/libomxalsa.so.0.0.0
/usr/local/lib/omxilcomponents/libomxffmpeg.a
/usr/local/lib/omxilcomponents/libomxffmpeg.la
/usr/local/lib/omxilcomponents/libomxffmpeg.so
/usr/local/lib/omxilcomponents/libomxffmpeg.so.0
/usr/local/lib/omxilcomponents/libomxffmpeg.so.0.0.0
/usr/local/lib/omxilcomponents/libomxfilereader.a
/usr/local/lib/omxilcomponents/libomxfilereader.la
/usr/local/lib/omxilcomponents/libomxfilereader.so
/usr/local/lib/omxilcomponents/libomxfilereader.so.0
/usr/local/lib/omxilcomponents/libomxfilereader.so.0.0.0
/usr/local/lib/omxilcomponents/libomxmad.a
/usr/local/lib/omxilcomponents/libomxmad.la
/usr/local/lib/omxilcomponents/libomxmad.so
/usr/local/lib/omxilcomponents/libomxmad.so.0
/usr/local/lib/omxilcomponents/libomxmad.so.0.0.0
/usr/local/lib/omxilcomponents/libomxvolcontrol.a
/usr/local/lib/omxilcomponents/libomxvolcontrol.la
/usr/local/lib/omxilcomponents/libomxvolcontrol.so
/usr/local/lib/omxilcomponents/libomxvolcontrol.so.0
/usr/local/lib/omxilcomponents/libomxvolcontrol.so.0.0.0
/usr/local/lib/omxilcomponents/libomxvorbis.a
/usr/local/lib/omxilcomponents/libomxvorbis.la
/usr/local/lib/omxilcomponents/libomxvorbis.so
/usr/local/lib/omxilcomponents/libomxvorbis.so.0
/usr/local/lib/omxilcomponents/libomxvorbis.so.0.0.0

%changelog
* Fri Oct 19 2007 Giulio Urlini
- removed fbdev from file list. It is experimental, 
  and not installed on any platform
* Tue Oct 01 2007 Giulio Urlini
- Minor update and name change of this file
* Tue Jun 04 2007 Giulio Urlini
- Bellagio 0.3.2 release
* Tue May 22 2007 Giulio Urlini
- Bellagio 0.3.1 release
* Fri Apr 06 2007 Giulio Urlini
- Bellagio 0.3 release
* Fri Feb 24 2006 David Siorpaes
- Fixed some minor issues in build process
* Mon Feb 6 2006 Giulio Urlini
- First build attempt

