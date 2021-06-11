Name:       constatus
Version:    4.6
Release:    0
Summary:    Video monitoring and streaming program
License:    AGPL3.0
Source0:    %{name}-%{version}.tgz
URL:        https://vanheusden.com/constatus/
BuildRequires: libconfig-devel
BuildRequires: fontconfig-devel
BuildRequires: libicu-devel
BuildRequires: turbojpeg-devel
BuildRequires: libpng-devel
BuildRequires: libcurl-devel
BuildRequires: jansson-devel
BuildRequires: boost-devel
BuildRequires: libatomic
BuildRequires: openssl-devel
BuildRequires: libv4l-devel
BuildRequires: netpbm-devel
BuildRequires: exiv2-devel
BuildRequires: frei0r-devel
BuildRequires: pam-devel
BuildRequires: gstreamer1-devel
BuildRequires: gstreamer1-plugins-base-devel
BuildRequires: SDL2-devel
BuildRequires: rygel-devel
BuildRequires: alsa-lib-devel
BuildRequires: ImageMagick-c++-devel
Requires: libconfig
Requires: fontconfig
Requires: libicu
Requires: turbojpeg
Requires: libpng
Requires: libcurl
Requires: jansson
Requires: boost
Requires: libatomic
Requires: openssl
Requires: libv4l
Requires: netpbm
Requires: exiv2
Requires: frei0r
Requires: pam
Requires: gstreamer1
Requires: gstreamer1-plugins-base
Requires: SDL2
Requires: rygel
Requires: alsa-lib
Requires: ImageMagick-c++

%description
Video monitoring and streaming program.
It can detect motion, act on that. It can filter,
merge streams, write to disk, send to v4l2-
loopback, has an internal webserver and plugin
interfaces

%prep
%setup -q -n %{name}-%{version}

%build
%cmake .
%make_build

%install
mkdir -p %{buildroot}/usr/bin/
install -m 755 constatus %{buildroot}/usr/bin/constatus
mkdir -p %{buildroot}/usr/share/constatus/examples
install -m 644 examples/* %{buildroot}/usr/share/constatus/examples
install -m 644 constatus.cfg %{buildroot}/usr/share/constatus/constatus.cfg
install -m 644 LICENSE %{buildroot}/usr/share/constatus/LICENSE
install -m 644 README.md %{buildroot}/usr/share/constatus/README.md
install -m 644 README.rest %{buildroot}/usr/share/constatus/README.rest
install -m 644 stylesheet.css %{buildroot}/usr/share/constatus/stylesheet.css

%files
/usr/bin/constatus
/usr/share/constatus/constatus.cfg
/usr/share/constatus/LICENSE
/usr/share/constatus/README.md
/usr/share/constatus/README.rest
/usr/share/constatus/stylesheet.css
/usr/share/constatus/examples/browser.cfg
/usr/share/constatus/examples/lcdproc-overlay.cfg
/usr/share/constatus/examples/lowres-detection-highres-store.cfg
/usr/share/constatus/examples/mosaic-stream.cfg
/usr/share/constatus/examples/multicast.cfg
/usr/share/constatus/examples/overlay-test.png
/usr/share/constatus/examples/interfacing-to-obs-studio.cfg
/usr/share/constatus/examples/lcdproc-overlay.py
/usr/share/constatus/examples/mjpeg-multiplexer.cfg
/usr/share/constatus/examples/motion-to-avi-file.cfg
/usr/share/constatus/examples/network-trigger.cfg
/usr/share/constatus/examples/README.md

%changelog
* Tue Aug 4 2020 Folkert van Heusden <mail@vanheusden.com>
-
