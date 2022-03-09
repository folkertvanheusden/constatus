// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
// some code from https://01.org/linuxgraphics/gfx-docs/drm/media/uapi/v4l/v4l2grab.c.html
#include "config.h"
#if HAVE_LIBV4L2 == 1
#include <fcntl.h>
#include <libv4l2.h>
#include <math.h>
#include <poll.h>
#include <cstring>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "error.h"
#include "source.h"
#include "source_v4l.h"
#include "picio.h"
#include "log.h"
#include "utils.h"
#include "ptz_v4l.h"
#include "controls_v4l.h"

static bool xioctl(int fh, long unsigned int request, void *arg, const char *const name)
{
        int r;

        do {
                r = v4l2_ioctl(fh, request, arg);
        } while (r == -1 && (errno == EINTR || errno == EAGAIN));

        if (r == -1) {
                log(LL_WARNING, "ioctl %s: error %d, %s\n", name, errno, strerror(errno));
		return false;
	}

	return true;
}

source_v4l::source_v4l(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & dev, const int jpeg_quality, const double max_fps, const int w_override, const int h_override, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, const bool prefer_jpeg, const bool use_controls, const std::map<std::string, feed *> & text_feeds) : source(id, descr, exec_failure, max_fps, r, resize_w, resize_h, loglevel, timeout, filters, failure, nullptr, jpeg_quality, text_feeds), dev(dev), prefer_jpeg(prefer_jpeg), w_override(w_override), h_override(h_override), use_controls(use_controls)
{
	fd = -1;
	vw = vh = -1;
	pixelformat = 0;
	n_buffers = 0;
}

source_v4l::~source_v4l()
{
	stop();
}

bool try_format(int fd, struct v4l2_format *const fmt, int codec)
{
	fmt->fmt.pix.pixelformat = codec;

        if (xioctl(fd, VIDIOC_S_FMT, fmt, "VIDIOC_S_FMT (try_format)") == false)
		return false;

	return fmt->fmt.pix.pixelformat == codec;
}

void source_v4l::operator()()
{
	log(id, LL_INFO, "source v4l2 thread started");

	set_thread_name("src_v4l2");

	fd = v4l2_open(dev.c_str(), O_RDWR, 0);
	if (fd == -1) {
		set_error(myformat("Cannot access video4linux device \"%s\"", dev.c_str()), true);
		do_exec_failure();
		return;
	}

	struct v4l2_format fmt { 0 };
        struct v4l2_buffer buf { 0 };
        struct v4l2_requestbuffers req { 0 };
        enum v4l2_buf_type type;

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width       = w_override;
        fmt.fmt.pix.height      = h_override;

	if (prefer_jpeg) {
		if (try_format(fd, &fmt, V4L2_PIX_FMT_JPEG))
			log(id, LL_INFO, "JPEG codec chosen");
		else if (try_format(fd, &fmt, V4L2_PIX_FMT_MJPEG))
			log(id, LL_INFO, "MJPEG codec chosen");
		else {
			log(id, LL_INFO, "Cannot use (M)JPEG mode, using RGB24 instead");
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
			xioctl(fd, VIDIOC_S_FMT, &fmt, "VIDIOC_S_FMT");
		}
	}
	else {
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
		xioctl(fd, VIDIOC_S_FMT, &fmt, "VIDIOC_S_FMT");
	}

        fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

        if (fmt.fmt.pix.width != w_override || fmt.fmt.pix.height != h_override)
                log(id, LL_WARNING, "Note: driver is sending image at %dx%d (requested: %dx%d)", fmt.fmt.pix.width, fmt.fmt.pix.height, w_override, h_override);

	std::unique_lock<std::mutex> lck(lock);

	width = fmt.fmt.pix.width;
	height = fmt.fmt.pix.height;

	pixelformat = fmt.fmt.pix.pixelformat;

	v4l2_jpegcompression ctrl{ jpeg_quality };
        ioctl(fd, VIDIOC_G_JPEGCOMP, &ctrl);

	memset(&req, 0x00, sizeof req);
        req.count = 2;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        xioctl(fd, VIDIOC_REQBUFS, &req, "VIDIOC_REQBUFS");

	bool fail = false;

	buffers = (buffer *)calloc(req.count, sizeof(*buffers));
	n_buffers = 0;
        for(; !fail && n_buffers < req.count; ++n_buffers) {
                memset(&buf, 0x00, sizeof(buf));
                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = n_buffers;

                xioctl(fd, VIDIOC_QUERYBUF, &buf, "VIDIOC_QUERYBUF");

                buffers[n_buffers].length = buf.length;
                buffers[n_buffers].start = v4l2_mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

                if (MAP_FAILED == buffers[n_buffers].start) {
			set_error(myformat("Cannot map video4linux device \"%s\" buffers", dev.c_str()), true);
			do_exec_failure();
			fail = true;
                }
        }

	log(id, LL_INFO, "v4l: n_buffers: %d", n_buffers);
        for (int i = 0; !fail && i < n_buffers; ++i) {
                memset(&buf, 0x00, sizeof(buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                xioctl(fd, VIDIOC_QBUF, &buf, "VIDIOC_QBUF");
        }

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        xioctl(fd, VIDIOC_STREAMON, &type, "VIDIOC_STREAMON");

	vw = width;
	vh = height;

	if (need_scale()) {
		width = resize_w;
		height = resize_h;
	}

	lck.unlock();

	track = ptz::check_is_supported(fd);

	controls_lock.lock();
	if (!fail && use_controls)
		this->c = new controls_v4l(fd);
	controls_lock.unlock();

	int bytes = vw * vh * 3;
	unsigned char *conv_buffer = static_cast<unsigned char *>(malloc(bytes));

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;

	struct pollfd fds[] = { { fd, POLLIN, 0 } };

	for(;!fail && !local_stop_flag;) {
		uint64_t start_ts = get_us();

		fds[0].revents = 0;
		if (poll(fds, 1, 100) == 0)
			continue;

		struct v4l2_buffer buf { 0 };
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

		if (v4l2_ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
			set_error(myformat("VIDIOC_DQBUF failed: %s", strerror(errno)), true);
			do_exec_failure();
			break;
		}

		if (work_required() && !is_paused()) {
			unsigned char *io_buffer = (unsigned char *)buffers[buf.index].start;

			if (pixelformat == V4L2_PIX_FMT_JPEG || pixelformat == V4L2_PIX_FMT_MJPEG) {
				int cur_n_bytes = buf.bytesused;

				if (resize_h != -1 || resize_w != -1) {
					int dw = -1, dh = -1;
					unsigned char *temp = NULL;
					if (my_jpeg.read_JPEG_memory(io_buffer, cur_n_bytes, &dw, &dh, &temp))
						set_scaled_frame(temp, dw, dh);
					free(temp);
				}
				else {
					set_frame(E_JPEG, io_buffer, cur_n_bytes);
				}
			}
			else {
				const size_t n = IMS(vw, vh, 3);

				memcpy(conv_buffer, io_buffer, n);

				if (need_scale())
					set_scaled_frame(conv_buffer, vw, vh);
				else
					set_frame(E_RGB, conv_buffer, n);
			}

			clear_error();
		}

		if (v4l2_ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
			set_error("ioctl(VIDIOC_QBUF) failed", true);
			do_exec_failure();
			break;
		}

		uint64_t end_ts = get_us();
		int64_t left = interval - (end_ts - start_ts);

		//printf("total took %d, left %d, interval %ld\n", int(end_ts - start_ts), int(left), int(interval));

		st->track_cpu_usage();

		if (interval > 0 && left > 0)
			myusleep(left);
	}

	free(conv_buffer);

	controls_lock.lock();
	delete c;
	c = nullptr;
	controls_lock.unlock();

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(fd, VIDIOC_STREAMOFF, &type, "VIDIOC_STREAMOFF");

        for(int i = 0; i < n_buffers; ++i)
                v4l2_munmap(buffers[i].start, buffers[i].length);

	v4l2_close(fd);

	free(buffers);

	delete track;

	register_thread_end("source v4l2");
}

void source_v4l::pan_tilt(const double abs_pan, const double abs_tilt)
{
	if (track) {
		track->pan(abs_pan);
		track->tilt(abs_tilt);
	}
}

void source_v4l::get_pan_tilt(double *const pan, double *const tilt) const
{
	if (track) {
		*pan = track->get_pan();
		*tilt = track->get_tilt();
	}
}
#endif
