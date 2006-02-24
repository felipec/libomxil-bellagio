Name: libomxil
Version: 0.1
Release: 0
Copyright: GNU LGPL
Group: System Environment/Libraries
Source: 
Buildroot: /var/tmp/libomxil-root
Summary: OpenMAX Integration Layer library and components
Vendor: STMicroelectronics

%description
This library includes the OpenMAX Integration Layer core and components

%prep
%setup
%build
./configure --prefix=$RPM_BUILD_ROOT/usr/local/
make

%install
rm -rf $RPM_BUILD_ROOT
make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/local/lib/libomxil.a
/usr/local/lib/libomxil.la
/usr/local/lib/libomxil.so
/usr/local/lib/libomxil.so.0
/usr/local/lib/libomxil.so.0.0.0
/usr/local/include/omx-core.h
/usr/local/doc/libomxil/AUTHORS
/usr/local/doc/libomxil/COPYING
/usr/local/doc/libomxil/ChangeLog
/usr/local/doc/libomxil/INSTALL
/usr/local/doc/libomxil/NEWS
/usr/local/doc/libomxil/README
/usr/local/doc/libomxil/TODO

%changelog
* Mon Feb 6 2006 Giulio Urlini
- First build attempt

