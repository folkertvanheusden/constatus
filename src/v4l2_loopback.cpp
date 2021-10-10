// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#if HAVE_LIBV4L2 == 1
#include <atomic>
#include <fcntl.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "error.h"
#include "source.h"
#include "utils.h"
#include "filter.h"
#include "log.h"
#include "v4l2_loopback.h"
#include "picio.h"

v4l2_loopback::v4l2_loopback(const std::string & id, const std::string & descr, source *const s, const double fps, const std::string & dev, const std::string & pixel_format, const std::vector<filter *> *const filters, instance *const inst) : interface(id, descr), s(s), fps(fps), dev(dev), pixel_format(pixel_format), filters(filters), inst(inst)
{
	local_stop_flag = false;
	ct = CT_LOOPBACK;

	th = myjpeg::allocate_transformer();

	if (this -> descr == "")
		this -> descr = dev;
}

v4l2_loopback::~v4l2_loopback()
{
	stop();
	free_filters(filters);

	myjpeg::free_transformer(th);
}

void v4l2_loopback::operator()()
{
	set_thread_name("p2vl");

	s -> start();

	int v4l2sink = open(dev.c_str(), O_WRONLY);
	if (v4l2sink == -1) {
        	set_error(myformat("Failed to open v4l2sink device (%s)", dev.c_str()), true);
		return;
	}

	// setup video for proper format
	struct v4l2_format v;
	memset(&v, 0x00, sizeof v);
	v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	if (ioctl(v4l2sink, VIDIOC_G_FMT, &v)) {
		set_error("VIDIOC_G_FMT failed", true);
		close(v4l2sink);
		return;
	}

	video_frame *prev_frame = nullptr;

	uint64_t prev_ts = 0;
	bool first = true;

        size_t enc_n = 0;

	for(;!local_stop_flag;) {
		pauseCheck();

		video_frame *pvf = s->get_frame(false, prev_ts);

		if (pvf) {
			prev_ts = pvf->get_ts();

			if (first) {
				first = false;

				v.fmt.pix.width = pvf->get_w();
				v.fmt.pix.height = pvf->get_h();

				if (pixel_format == "YUV420") {
					v.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
					enc_n = v.fmt.pix.width * v.fmt.pix.height + v.fmt.pix.width * v.fmt.pix.height / 2;
				}
				else if (pixel_format == "BGR24" || pixel_format == "RGB24") {
					v.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
					enc_n = IMS(pvf->get_w(), pvf->get_h(), 3);
				}

				v.fmt.pix.sizeimage = enc_n;
				if (ioctl(v4l2sink, VIDIOC_S_FMT, &v) == -1) {
					set_error("VIDIOC_S_FMT failed", true);
					break;
				}

				log(id, LL_INFO, "v4l2-loopback pixel format %s, dimensions: %dx%d", pixel_format.c_str(), v.fmt.pix.width, v.fmt.pix.height);
			}

			if (filters && !filters->empty()) {
				video_frame *temp = pvf->apply_filtering(nullptr, s, prev_frame, filters, nullptr);

				delete pvf;
				pvf = temp;
			}

			auto img = pvf->get_data_and_len(E_RGB);

			if (pixel_format == "YUV420") {
				uint8_t *temp { nullptr };
				my_jpeg.rgb_to_i420(th, std::get<0>(img), pvf->get_w(), pvf->get_h(), &temp);

				if (write(v4l2sink, temp, enc_n) == -1) {
					set_error("write to video loopback failed", true);
					free(temp);
					delete pvf;
					break;
				}

				free(temp);
			}
			else {
				if (write(v4l2sink, std::get<0>(img), std::get<1>(img)) == -1) {
					set_error("write to video loopback failed", true);
					delete pvf;
					break;
				}
			}

			delete prev_frame;
			prev_frame = pvf;
		}

		st->track_cpu_usage();

		if (fps > 0)
			mysleep(1000000.0 / fps, &local_stop_flag, s);
	}

	delete prev_frame;

	s -> stop();

	close(v4l2sink);

	register_thread_end("v4l2_loopback");
}
#endif
