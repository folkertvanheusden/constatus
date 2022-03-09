# Constatus

Constatus monitors, converts, transforms, filters and multiplexes video-feeds. Feeds like IP-cameras, "video4linux"-devices, pixelflut, VNC-servers, Raspberry Pi-cameras, etc.
It is an NVR (network video recorder) with special features.


## features

- input:
   - video4linux
   - MJPEG cameras
   - RTSP cameras
   - JPEG cameras
   - plugin interface (included is a VNC client plugin)
   - pixelflood
   - gstreamer pipeline
   - pipewire
   - libcamera

- output:
   - AVI file(s)
   - individual JPEGs
   - HTTP streaming (MJPEG, OGG (Theora))
   - video4linux loopback device
   - build-in VNC server
   - pixelflood
   - pipewire
   - others via plugins

- motion detection
   - configurable detector
   - masks
   - also via plugin so that you can easily use one from e.g. OpenCV

- audio detection
   - can start recording video when sound reaches a certain level

- filters:
   - add generic text
     two versions: fast (low CPU) or scaled (text can be any font, size or color)
   - boost contrast
   - convert to grayscale
   - add box around movement
   - show only pixels that have changed
   - mirror vertical and/or horizontal
   - neighbour average noise filter
   - picture overlay (with alpha blending)
   - scaling (resizing)
   - filters plugins (.so-files that can be linked at runtime)
   - lcdproc server to allow dynamic text overlays
   - frei0r plugins
   ...and more!

- managing:
   - Web interface (with authentication using PAM)
     - At run-time configurable dashboard (requires MySQL)
   - REST interface

## compiling

### Debian/Ubuntu

- Required:
  - cmake
  - libconfig++-dev
  - libfontconfig1-dev
  - libfreetype6-dev
  - libicu-dev
  - libturbojpeg0-dev
  - libpng-dev
  - libcurl-dev e.g. libcurl4-openssl-dev
  - libjansson-dev
  - libssl-dev
  - libboost-system-dev

You may need libboost-system-dev, see the 'FAQ' section at the bottom.

- Only for building:
  - build-essential
  - cmake
  - pkg-config

- Not required (OPTIONAL!):
  - libtheora-dev               - Ogg/Theora streaming
  - libcamera-dev               - libcamera still once in a while changes API so this may not always compile
  - libv4l-dev			- USB cameras (it will complain that "libv4l2" is missing if not installed)
  - libnetpbm10-dev		- PBM pictures loading/saving
  - libexiv2-dev		- JPEG meta data
  - libmysqlcppconn-dev		- MySQL
  - frei0r-plugins-dev		- frei0r plugins
  - libpam0g-dev	        - authentication in http
  - libgstreamer1.0-dev		- gstreamer pipelines and AVI-files (AVI files require the gstreamer1.0-plugins-good package as well at runtime)
  - libgstreamer-plugins-base1.0-dev
  - libwebsocketpp-dev          - refreshless http
  - libsdl2-dev                 - X11/Xorg GUI
  - rygel / rygel-2.6-dev / libglib2.0-dev / libxml2-dev / libgee-0.8-dev / libgupnp-av-1.0-dev / libupnp-dev  - announce streams via UPnP on the network
  - libasound2-dev              - audio trigger
  - libmagick++-dev             - for animated gif/mng overlay
  - ffmpeg: libavformat-dev / libswscale-dev / libavcodec-dev / libavutil-dev / libswresample-dev / libavresample-dev / libatomic1 - for mp4 and other file formats
  - pipewire: libpipewire-0.3-dev / libspa-0.2-dev
  - libmosquitto-dev            - for obtaining text that can be placed as an overlay/scroll text


libgwavi is since version 4.3 not used anymore; you can remove it from your computer if you ony installed it for constatus.


## How to use

The program is configured via a text-file. It is processed by libconfig and thus uses that format.

Take a look at the example: constatus.cfg (and the others under examples/). constatus.cfg contains explanations for each and every configuration option.

Constatus can monitor multiple video-sources in 1 instance. For that you have:

  instances = (
     {
       ...
     },
     {
       ...
     },
     ...
  )

Each instance adds a new { ... } section. In such a section one can add multipe interfaces.
For example one source, one or more motion-trigger or audio-triggers, one or more http-servers (web-servers) and one or more targets (named "stream-to-disk" altough this can also be network-servers and gstreamer-pipeliens).

Constatus can apply filters in almost every phase. If you add it in the "source", then this applies to the whole chain. Filters are executed in the order in which they are found on the configuration-file. 

As said, constatus can have multiple streamers, filters, etc.; the only drawback is that it uses more cpu. Having available multiple cpus/cores/threads is an advantag for constatus.

Each item has an id. This id can be referred to by other parts of the configuration. You can leave it empty if you like.

If it is unclear what the "selection-bitmap" masks, then try adding it to an "apply-mask" stream-writer or http-server filter. That way you can see which part of the image is masked off.

Apart from the instances where you can define http-servers there is also a global-http-server which allows you to view all instances in one place.

"maintentance" configures the database-connection. Altough it is called maintentance, this database-connection is also used to store the dashboard configuration (each http-user (or 1 for all if no authentication is configured) can put one or more video-sources on one overview page).

"views" are for combinging multiple video-sources into one view. For example the "source-switch" shows multiple sources in a round-robin way, "all" shows them in a mosaic-view and "pip" as picture-in-picture.

"guis" show sources in a window (on your desktop - when running Constatus on e.g. a laptop).

"announcers" announce sources on the local network. It uses SSDP/UPnP/DLNA for that. They are then visible in VLC/windows explorer on systems in the same LAN.

Read README.rest to get to know how the REST interface works.


Check docs/ to see example configuration-files.


Use motion-to-constatus.py to convert a motion configuration file to a constatus configuration file - this requires the libconf package, see https://pypi.python.org/pypi/libconf


This program contains the "Cruft Regular Font", found to be in the public domain (from http://www.publicdomainfiles.com/show_file.php?id=13501609932993).


## FAQ
* If I restart the program then sometimes some cameras show an error
  - that is a problem of certain cameras that have a limit on the number of viewers it can handle concurrently; apparently the previous session was not terminated fully

* Why "constatus"?
  - it's lost-in-translation name.

* Does not compile (link, to be precise) on some systems complaining about missing boost components
  - sudo apt-get install libboost-system-dev
  - add "target_link_libraries(constatus -lboost_system)" (without quotes) to CMakeLists.txt

* Note: Debian buster is at gupnp-1.0 and Ubuntu 20.04 is at gupnp-1.2, see CMakeLists.txt. This is required if you want "UPnP/SSDP/DLNA" to work.

* Web-interface looks messy
  - add a 'stylesheet = "/path/to/stylesheet.css"; under each http-server section. The important part here is to add the path.
  - since 5.0 a default stylesheet is included

* Low fps camera feed often shows "camera lost" screen
  - Increase the timeout value of that camera. E.g. put "timeout = 2.0;" in the source-section of that camera.


## TIPS
* Do not combine more than 6 cameras in an html-grid: web-browsers usually are not capable of showing more streams in parallel. A solution can be using a "view-all" view instead. 

* If you want to be able to view recordings in a browser, make sure you record to a file-format supported by browsers. For a list, see https://en.wikipedia.org/wiki/HTML5_video#Browser_support
  - To configure for for example "mp4", see "browser.cfg" under examples.

* It can help to add compiler-optimalization flags to CMakeLists.txt before building: lower cpu-usage and/or higher FPS.
  - E.g. for the raspberry pi 3/4 add:
    - set(CMAKE_INTERPROCEDURAL_OPTIMIZATION True)
    - set(CMAKE_CXX_FLAGS "-Ofast -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -ffast-math")
  - On other systems:
    - set(CMAKE_INTERPROCEDURAL_OPTIMIZATION True)
    - set(CMAKE_CXX_FLAGS "-Ofast -march=native -mtune=native -ffast-math")

* If you've limited a source {} to e.g. 4 fps, then (try to) set the camera itself also to that frame-rate. Usually that reduces the cpu-usage of constatus.

* You can interface Constatus to OBS ( https://obsproject.com/ ) via the 'video4linux' output.


## CODE QUALITY

[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/folkertvanheusden/constatus.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/folkertvanheusden/constatus/context:cpp)


## SUPPORT
- no


## TO-DO
* configuration-wizard
* fix the motion-configuration-importer
* onvif support
* audio
* allow users to add comments to recorded videos



(C) 2017-2022 by Folkert van Heusden <mail@vanheusden.com>, Constatus is released under Apache License v2.0
