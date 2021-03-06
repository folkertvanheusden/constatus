* browser.cfg

How to produce video-files that can be viewed in a browser.

* interfacing-to-obs-studio.cfg

Connecting Constatus to https://obsproject.com/

* lcdproc-overlay.cfg / lcdproc-overlay.py

Constatus has an lcdproc-server (via a 'filter') which allows you
to easily overlay texts. This can be any text you like.
The cfg-file is a regular constatus cfg-file, the python script
is an example how to interface to the constatus (or any other!)
lcdproc server.

* lowres-detection-highres-store.cfg

Do motion detection on a low-res video source and record the
high-res source.

* mjpeg-multiplexer.cfg

Let Constatus retrieve a video stream from some MJPEG video-source
and let multiple viewers view it. This allows more viewers in case
the source has a limit.

* mosaic-stream.cfg

Shows how to view multiple video streams in one view.

* motion-to-avi-file.cfg

How to produce a regular AVI file when motion is detected.

* multicast-tx.cfg

Example of the gstreamer (https://gstreamer.freedesktop.org/)
integration. This example shows how to multicast a video stream on
the network.

* network-trigger.cfg

Constatus can also start recording when it gets an external
tigger. Here a trigger is received over the network.

* pipewire-source.cfg

How to use Constatus with a PipeWire video source.

* pipewire-target.cfg

How to use Constatus with a PipeWire video target.

* video4linux.cfg

Just a simple video4linux video source http server configuration.

* pixelflood.cfg

Takes a video stream (from e.g. a webcam), resizes it and sends
the result to a pixelflood-server.

* motion-web.cfg

Shows how to get a red border around a video stream when motion
is detected.

* timelapse.cfg

Shows how to generate a timelapse and give access to it via a
webserver.

* crop.cfg

Shows how to cut a part of a videostream and publish it as a
new source.

* overlay-on-motion.cfg

Overlay stream with an other stream when motion is detected
in the other stream.
