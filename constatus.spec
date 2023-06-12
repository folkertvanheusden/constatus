Name:       constatus
Version:    6.0
Release:    0
Summary:    Video monitoring and streaming program
License:    MIT
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
BuildRequires: freetype-devel
BuildRequires: libtheora-devel
BuildRequires: libcamera-devel
BuildRequires: websocketpp-devel
BuildRequires: glib2-devel
BuildRequires: libxml2-devel
BuildRequires: libgee-devel
BuildRequires: gupnp-devel
BuildRequires: libupnp-devel
BuildRequires: libavformat-free-devel
BuildRequires: libswresample-free-devel
BuildRequires: libswscale-free-devel
BuildRequires: libavcodec-free-devel
BuildRequires: libavutil-free-devel
BuildRequires: libatomic_ops-devel
BuildRequires: pipewire-devel
BuildRequires: mosquitto-devel
BuildRequires: mysql-connector-net-devel
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
Requires: frei0r-plugins
Requires: pam
Requires: gstreamer1
Requires: gstreamer1-plugins-base
Requires: SDL2
Requires: rygel
Requires: alsa-lib
Requires: ImageMagick-c++
Requires: freetype
Requires: libtheora
Requires: libcamera
Requires: glib2
Requires: libxml2
Requires: libgee
Requires: gupnp
Requires: libupnp
Requires: libavformat-free
Requires: libswresample-free
Requires: libswscale-free
Requires: libavcodec-free
Requires: libavutil-free
Requires: libatomic_ops
Requires: pipewire
Requires: mosquitto
Requires: mysql-connector-net

%description
Video monitoring and streaming program.
It can detect motion, act on that. It can filter,
merge streams, write to disk, send to v4l2-
loopback, has an internal webserver and plugin
interfaces

%prep
%setup -q -n %{name}-%{version}

%build
%cmake
%cmake_build

%install
%cmake_install

%files
/usr/bin/constatus
/usr/bin/list-libcamera
/usr/share/doc/constatus/
/usr/share/man/man1/constatus.1.gz

%changelog
* Mon Jun 12 2023 Folkert van Heusden <mail@vanheusden.com>
-
