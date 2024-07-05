#include "config.h"
#include <stdio.h>
#include <string.h>
#include <libcamera/libcamera.h>

int main(int argc, char *argv[])
{
	libcamera::CameraManager *lcm = new libcamera::CameraManager();

	if (int rc = lcm->start(); rc != 0) {
		fprintf(stderr, "libcamera: %s", strerror(-rc));
		return 1;
	}

	auto cams = lcm->cameras();
	for(auto camera : cams) {
		printf("libcamera device: %s\n", camera.get()->id().c_str());

		printf("   resolutions:\n");
		for(auto & stream : camera.get()->streams())
			printf("      %s\n", stream->configuration().toString().c_str());
	}

	return 0;
}
