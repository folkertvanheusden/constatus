// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#if HAVE_LIBCAMERA == 1 || HAVE_LIBCAMERA2 == 1
#include <errno.h>
#include <cstring>
#include <linux/videodev2.h>
#include <sys/mman.h>

#include "error.h"
#include "source_libcamera.h"
#include "log.h"
#include "utils.h"
#include "ptz_v4l.h"
#include "parameters.h"
#include "controls.h"

source_libcamera::source_libcamera(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & dev, const int jpeg_quality, const double max_fps, const int w_requested, const int h_requested, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, const bool prefer_jpeg, const std::map<std::string, parameter *> & ctrls, controls *const c) : source(id, descr, exec_failure, max_fps, r, resize_w, resize_h, loglevel, timeout, filters, failure, c, jpeg_quality), dev(dev), w_requested(w_requested), h_requested(h_requested), prefer_jpeg(prefer_jpeg), ctrls(ctrls)
{
}

source_libcamera::~source_libcamera()
{
	stop();

	delete c;
}

void source_libcamera::request_completed(libcamera::Request *request)
{
	if (request->status() == libcamera::Request::RequestCancelled)
                return;

        const auto & buffers = request->buffers();

        for(auto bufferPair : buffers) {
		libcamera::FrameBuffer *const buffer = bufferPair.second;
                const libcamera::FrameMetadata & metadata = buffer->metadata();

		for(const libcamera::FrameBuffer::Plane & plane : buffer->planes()) {
			void *data = mappedBuffers[plane.fd.fd()].first;
			unsigned int length = plane.length;

			if (pixelformat == V4L2_PIX_FMT_MJPEG || pixelformat == V4L2_PIX_FMT_JPEG)
				set_frame(E_JPEG, (const uint8_t *)data, length);
			else if (pixelformat == V4L2_PIX_FMT_RGB24)
				set_frame(E_RGB, (const uint8_t *)data, length);
			else
				log(id, LL_ERR, "Unexpected pixelformat");

			break;
		}
        }

	request->reuse(libcamera::Request::ReuseBuffers);
        camera->queueRequest(request);
}

void source_libcamera::list_devices()
{
	libcamera::CameraManager *lcm = new libcamera::CameraManager();

	if (int rc = lcm->start(); rc != 0)
		error_exit(false, "libcamera: %s", strerror(-rc));

	auto cams = lcm->cameras();
	for(auto camera : cams)
		printf("libcamera device: %s\n", camera.get()->id().c_str());

	lcm->stop();
}

#include <unistd.h>
void source_libcamera::operator()()
{
	log(id, LL_INFO, "source libcamera thread started");

	set_thread_name("src_libcamera");

	cm = new libcamera::CameraManager();

	if (int rc = cm->start(); rc != 0)
		error_exit(false, "libcamera: %s", strerror(-rc));

	camera = cm->get(dev);
	if (!camera)
		error_exit(false, "Camera \"%s\" not found", dev.c_str());

	if (camera->acquire())
		error_exit(false, "Cannot acquire \"%s\"", dev.c_str());

	log(id, LL_INFO, "Camera name: %s", camera->id().c_str());

	std::string controls_list;
        for(const auto & ctrl : camera->controls()) {
                const libcamera::ControlId *id = ctrl.first;

		controls_list += " " + ctrl.first->name();
	}

	log(id, LL_INFO, " Controls:%s", controls_list.c_str());

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;

	libcamera::StreamRoles roles{ libcamera::VideoRecording };
	std::unique_ptr<libcamera::CameraConfiguration> camera_config = camera->generateConfiguration(roles);

	size_t idx = 0;
	for(; idx<camera_config->size(); idx++) {
		uint32_t format = camera_config->at(idx).pixelFormat.fourcc();

		if (prefer_jpeg && (format == V4L2_PIX_FMT_MJPEG || format == V4L2_PIX_FMT_JPEG))
			break;

		if (!prefer_jpeg && format == V4L2_PIX_FMT_RGB24)
			break;
	}

	if (idx == camera_config->size())
		idx = 0;

	log(id, LL_INFO, "Requested: %d x %d, slot %zu of %zu", w_requested, h_requested, idx, camera_config->size());
	libcamera::StreamConfiguration & stream_config = camera_config->at(idx);
	log(id, LL_INFO, "Default: %s", stream_config.toString().c_str());

	stream_config.size.width = w_requested;
	stream_config.size.height = h_requested;
	stream_config.pixelFormat = libcamera::formats::RGB888;

	camera_config->validate();

	pixelformat = stream_config.pixelFormat.fourcc();
	log(id, LL_INFO, "Validated configuration is: %s (%c%c%c%c)", stream_config.toString().c_str(), pixelformat, pixelformat >> 8, pixelformat >> 16, pixelformat >> 24);

	std::unique_lock<std::mutex> lck(lock);
	width = stream_config.size.width;
	height = stream_config.size.height;
	lck.unlock();

	bool fail = false;

	if (camera->configure(camera_config.get())) {
		log(id, LL_ERR, "Cannot configure camera");

		fail = true;
	}
	else {
		allocator = new libcamera::FrameBufferAllocator(camera);

		for (libcamera::StreamConfiguration &cfg : *camera_config) {
			int ret = allocator->allocate(cfg.stream());
			if (ret < 0) {
				log(id, LL_ERR, "Can't allocate buffers");
				fail = true;
			}

			size_t allocated = allocator->buffers(cfg.stream()).size();
			log(id, LL_INFO, "Allocated %zu buffers for stream", allocated);
		}

		stream = stream_config.stream();
		const std::vector<std::unique_ptr<libcamera::FrameBuffer>> & buffers = allocator->buffers(stream);

		for(size_t i = 0; i < buffers.size(); i++) {
			std::unique_ptr<libcamera::Request> request = camera->createRequest();
			if (!request) {
				fail = true;
				log(id, LL_ERR, "Can't create request for camera");
				break;
			}

			const std::unique_ptr<libcamera::FrameBuffer> & buffer = buffers[i];
			if (int ret = request->addBuffer(stream, buffer.get()); ret < 0) {
				fail = true;
				log(id, LL_ERR, "Can't set buffer for request: %s", strerror(-ret));
				break;
			}

			for(const auto & ctrl : camera->controls()) {
				const libcamera::ControlId *id = ctrl.first;

				auto it = ctrls.find(str_tolower(ctrl.first->name()));

				if (it != ctrls.end()) {
					libcamera::ControlList & ctls = request->controls();

					if (id->type() == libcamera::ControlTypeFloat)
						ctls.set(id->id(), float(atof(it->second->get_value_string().c_str())));
					else if (id->type() == libcamera::ControlTypeString)
						ctls.set(id->id(), it->second->get_value_string());
				}
			}

			requests.push_back(std::move(request));
		}

		if (!fail) {
			log(id, LL_INFO, "source libcamera: starting main-loop");

			camera->requestCompleted.connect(this, &source_libcamera::request_completed);

			camera->start();

		sleep(5);
			for(auto & request : requests)
				camera->queueRequest(request.get());

			for(;!local_stop_flag;) {
				mysleep(100000, &local_stop_flag, nullptr);

				st->track_cpu_usage();
			}
		}
	}

	log(id, LL_INFO, "source libcamera thread terminating");

        camera->stop();
        allocator->free(stream);
        delete allocator;

        camera->release();
        cm->stop();

	register_thread_end("source libcamera");
}

void source_libcamera::pan_tilt(const double abs_pan, const double abs_tilt)
{
}

void source_libcamera::get_pan_tilt(double *const pan, double *const tilt) const
{
}
#endif
