#include <stdio.h>
#include <string.h>
#include <libcamera/libcamera.h>

int main(int argc, char *argv[])
{
	bool verbose = argc >=2 && strcmp(argv[1], "-v") == 0;
	bool very_verbose = argc >=2 && strcmp(argv[1], "-vv") == 0;

	if (!very_verbose)
		libcamera::logSetTarget(libcamera::LoggingTargetNone);

	libcamera::CameraManager *lcm = new libcamera::CameraManager();

	if (int rc = lcm->start(); rc != 0) {
		fprintf(stderr, "libcamera: %s", strerror(-rc));
		return 1;
	}

	auto cams = lcm->cameras();
	for(auto camera : cams) {
		printf("libcamera device: %s\n", camera.get()->id().c_str());

		if (verbose) {
			std::unique_ptr<libcamera::CameraConfiguration> configurations = camera->generateConfiguration({ libcamera::StreamRole::VideoRecording });

			for(auto & configuration: *configurations) {
				printf("   %s\n", configuration.toString().c_str());

				for(auto & format: configuration.formats().pixelformats())
					printf("    %s: %s\n", format.toString().c_str(), configuration.formats().range(format).toString().c_str());
			}
		}
	}

	return 0;
}
