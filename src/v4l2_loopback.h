// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include "config.h"
#if HAVE_LIBV4L2 == 1
#include <string>
#include <thread>
#include <vector>

#include "interface.h"
#include "picio.h"

class filter;
class source;

class v4l2_loopback : public interface
{
private:
	source *const s;
	const double fps;
	const std::string dev, pixel_format;
	const std::vector<filter *> *const filters;
	instance *const inst;
	transformer_t th;

public:
	v4l2_loopback(const std::string & id, const std::string & descr, source *const s, const double fps, const std::string & dev, const std::string & pixel_format, const std::vector<filter *> *const filters, instance *const inst);
	virtual ~v4l2_loopback();

	void operator()() override;
};
#endif
