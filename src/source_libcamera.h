// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "config.h"
#if HAVE_LIBCAMERA == 1 || HAVE_LIBCAMERA2 == 1
#include <atomic>
#include <map>
#include <string>
#include <thread>
#include <libcamera/libcamera.h>

#include "source.h"

class controls;
class parameter;

class source_libcamera : public source
{
protected:
	const std::string dev;
	const int w_requested, h_requested;
	const bool prefer_jpeg;
	const std::map<std::string, parameter *> ctrls;
	const int rotate_angle;

	libcamera::CameraManager *cm{ nullptr };
	std::shared_ptr<libcamera::Camera> camera;
	libcamera::FrameBufferAllocator *allocator{ nullptr };
	libcamera::Stream *stream{ nullptr };
	std::vector<std::unique_ptr<libcamera::Request> > requests;
	std::map<int, std::pair<void *, unsigned int>> mappedBuffers;

	uint32_t pixelformat{ 0 };

	void request_completed(libcamera::Request *request);

public:
	source_libcamera(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & dev, const int jpeg_quality, const double max_fps, const int w_requested, const int h_requested, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, const bool prefer_jpeg, const std::map<std::string, parameter *> & ctrls, controls *const c, const int rotate_angle);
	virtual ~source_libcamera();

	static void list_devices();

	virtual void pan_tilt(const double abs_pan, const double abs_tilt) override;
	virtual void get_pan_tilt(double *const pan, double *const tilt) const override;

	virtual void operator()() override;
};
#endif
