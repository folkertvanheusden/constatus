// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#if HAVE_LIBCAMERA == 1 || HAVE_LIBCAMERA2 == 1
#include <errno.h>
#include <cstring>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>

#include "encoding.h"
#include "error.h"
#include "source_libcamera.h"
#include "log.h"
#include "utils.h"
#include "ptz_v4l.h"
#include "parameters.h"
#include "controls.h"

source_libcamera::source_libcamera(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & dev, const int jpeg_quality, const double max_fps, const int w_requested, const int h_requested, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, const bool prefer_jpeg, const std::map<std::string, parameter *> & ctrls, controls *const c, const int rotate_angle, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio) : source(id, descr, exec_failure, max_fps, r, resize_w, resize_h, loglevel, timeout, filters, failure, c, jpeg_quality, text_feeds, keep_aspectratio), dev(dev), w_requested(w_requested), h_requested(h_requested), prefer_jpeg(prefer_jpeg), ctrls(ctrls), rotate_angle(rotate_angle)
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
			const uint8_t *data = (const uint8_t *)mappedBuffers[plane.fd.get()].first;
			const unsigned int length = plane.length;

			if (pixelformat == libcamera::formats::MJPEG)
				set_frame(E_JPEG, data, length);
#ifdef __arm__  // hopefully a raspberry pi
			else if (pixelformat == libcamera::formats::RGB888)
				set_frame(E_BGR, data, length);
			else if (pixelformat == libcamera::formats::BGR888)
				set_frame(E_RGB, data, length);
#else
			else if (pixelformat == libcamera::formats::RGB888)
				set_frame(E_RGB, data, length);
			else if (pixelformat == libcamera::formats::BGR888)
				set_frame(E_BGR, data, length);
#endif
			else if (pixelformat == libcamera::formats::YUYV)
				set_frame(E_YUYV, data, length);
			else {
				log(id, LL_ERR, "Unexpected pixelformat (%x / %c%c%c%c)", pixelformat, pixelformat >> 24 ?:' ', pixelformat >> 16 ?:' ', pixelformat >> 8 ?:' ', pixelformat ?:' ');
			}
		}
	}

	request->reuse(libcamera::Request::ReuseBuffers);

	int rc = camera->queueRequest(request);
	if (rc)
		log(id, LL_ERR, "source libcamera: error %s", strerror(rc));
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
		controls_list += " " + ctrl.first->name();

		switch(ctrl.first->type()) {
			case libcamera::ControlTypeNone:
				break;
			case libcamera::ControlTypeBool:
				controls_list += "(bool)"; break;
			case libcamera::ControlTypeByte:
				controls_list += "(byte)"; break;
			case libcamera::ControlTypeInteger32:
				controls_list += "(32bit value)"; break;
			case libcamera::ControlTypeInteger64:
				controls_list += "(64bit value)"; break;
			case libcamera::ControlTypeFloat:
				controls_list += "(floating point)"; break;
			case libcamera::ControlTypeString:
				controls_list += "(string)"; break;
			default:
				controls_list += "(?)";
				break;
		}
	}

	log(id, LL_INFO, " Controls:%s", controls_list.c_str());

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;

	libcamera::StreamRoles roles{ libcamera::StreamRole::VideoRecording };
	std::unique_ptr<libcamera::CameraConfiguration> camera_config = camera->generateConfiguration(roles);

	if (!camera_config.get() || camera_config->size() == 0)
		error_exit(false, "The device \"%s\" cannot produce a video stream that Constatus can use", dev.c_str());

	libcamera::StreamConfiguration & stream_config = camera_config->at(0);
	libcamera::PixelFormat best_format = stream_config.pixelFormat;

	do {
		// reset as validate can alter it
		if (rotate_angle == 0) {
		}
		else if (rotate_angle == 90)
			camera_config->transform = libcamera::Transform::Rot90;
		else if (rotate_angle == 180)
			camera_config->transform = libcamera::Transform::Rot180;
		else if (rotate_angle == 270)
			camera_config->transform = libcamera::Transform::Rot270;
		else
			error_exit(false, "Can only rotate in steps of 90 degrees (not %d, libcamera - %s)", rotate_angle, dev.c_str());

		// try MJPEG if preferred
		if (prefer_jpeg) {
			stream_config.size.width = w_requested;
			stream_config.size.height = h_requested;
			stream_config.pixelFormat = libcamera::formats::MJPEG;

			libcamera::CameraConfiguration::Status status = camera_config->validate();

			if (status == libcamera::CameraConfiguration::Status::Valid)
				break;

			if (status == libcamera::CameraConfiguration::Status::Adjusted) {
				best_format = stream_config.pixelFormat;

				break;
			}
		}

		// try RGB
		stream_config.size.width = w_requested;
		stream_config.size.height = h_requested;
#ifdef __arm__  // hopefully a raspberry pi
		stream_config.pixelFormat = libcamera::formats::BGR888;
#else
		stream_config.pixelFormat = libcamera::formats::RGB888;
#endif

		log(id, LL_INFO, "Trying: %s", stream_config.toString().c_str());

		libcamera::CameraConfiguration::Status status = camera_config->validate();

		if (status == libcamera::CameraConfiguration::Status::Valid)
			break;

		if (status == libcamera::CameraConfiguration::Status::Adjusted)
			best_format = stream_config.pixelFormat;

		// try BGR
		stream_config.size.width = w_requested;
		stream_config.size.height = h_requested;
#ifdef __arm__  // hopefully a raspberry pi
		stream_config.pixelFormat = libcamera::formats::RGB888;
#else
		stream_config.pixelFormat = libcamera::formats::BGR888;
#endif

		log(id, LL_INFO, "Trying: %s", stream_config.toString().c_str());

		status = camera_config->validate();

		if (status == libcamera::CameraConfiguration::Status::Valid)
			break;

		if (status == libcamera::CameraConfiguration::Status::Adjusted)
			best_format = stream_config.pixelFormat;

		// try YUYV
		stream_config.size.width = w_requested;
		stream_config.size.height = h_requested;
		stream_config.pixelFormat = libcamera::formats::YUYV;

		log(id, LL_INFO, "Trying: %s", stream_config.toString().c_str());

		status = camera_config->validate();

		if (status == libcamera::CameraConfiguration::Status::Valid)
			break;

		if (status == libcamera::CameraConfiguration::Status::Adjusted)
			best_format = stream_config.pixelFormat;

		// try MJPEG also as last resort
		if (!prefer_jpeg) {
			stream_config.size.width = w_requested;
			stream_config.size.height = h_requested;
			stream_config.pixelFormat = libcamera::formats::MJPEG;

			status = camera_config->validate();

			if (status == libcamera::CameraConfiguration::Status::Valid || status == libcamera::CameraConfiguration::Status::Adjusted)
				break;
		}

		stream_config.size.width = w_requested;
		stream_config.size.height = h_requested;
		stream_config.pixelFormat = best_format;

		if (camera_config->validate())
			error_exit(false, "source libcamera: configuration is unexpectedly invalid");
	}
	while(0);

	log(LL_INFO, "source libcamera: chosen format: %s", stream_config.toString().c_str());

	pixelformat = stream_config.pixelFormat;

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
				error_exit(false, "Can't create request for camera");
				break;
			}

			const std::unique_ptr<libcamera::FrameBuffer> & buffer = buffers[i];
			if (int ret = request->addBuffer(stream, buffer.get()); ret < 0) {
				fail = true;
				error_exit(false, "Can't set buffer for request: %s", strerror(-ret));
				break;
			}

			for(const libcamera::FrameBuffer::Plane & plane : buffer->planes()) {
				void *memory = mmap(NULL, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);
				if (memory == MAP_FAILED)
					error_exit(true, "mmap failed for fd %d, size %ld", plane.fd.get(), plane.length);

				mappedBuffers[plane.fd.get()] = std::make_pair(memory, plane.length);
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
					else if (id->type() == libcamera::ControlTypeBool)
						ctls.set(id->id(), it->second->get_value_bool());
					else if (id->type() == libcamera::ControlTypeInteger32 || id->type() == libcamera::ControlTypeInteger64)
						ctls.set(id->id(), it->second->get_value_int());
					else if (id->type() == libcamera::ControlTypeNone)
						log(LL_WARNING, "Don't know how to hande libcamera::ControlTypeNone");
					else
						error_exit(false, "Unimplemented control-type for source libcamera");
				}
			}

			requests.push_back(std::move(request));
		}

		if (!fail) {
			log(id, LL_INFO, "source libcamera: starting main-loop");

			camera->requestCompleted.connect(this, &source_libcamera::request_completed);

			int rc = camera->start();
			if (rc == 0) {
				for(auto & request : requests) {
					rc = camera->queueRequest(request.get());

					if (rc)
						log(id, LL_ERR, "source libcamera: queue request error %s", strerror(rc));
				}

				for(;!local_stop_flag;) {
					mysleep(100000, &local_stop_flag, nullptr);

					st->track_cpu_usage();
				}
			}
			else {
				log(id, LL_ERR, "source libcamera: cannot start camera (%s)", strerror(rc));
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
