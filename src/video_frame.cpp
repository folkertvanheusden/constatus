// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include <assert.h>
#include <cstring>
#include "gen.h"
#include "utils.h"
#include "resize.h"
#include "picio.h"
#include "controls.h"
#include "instance.h"
#include "meta.h"
#include "source.h"
#include "filter.h"
#include "log.h"

video_frame::video_frame(const meta *const m, const int jpeg_quality) : m_(m), jpeg_quality(jpeg_quality)
{
}

video_frame::video_frame(const meta *const m, const int jpeg_quality, const uint64_t ts, const int w, const int h, uint8_t *const data, const size_t len, const encoding_t e) : m_(m), jpeg_quality(jpeg_quality), ts(ts), w(w), h(h)
{
	this->data.insert({ e, { data, len } });
}

video_frame::~video_frame()
{
	for(auto it : data)
		free(it.second.first);
}

void video_frame::set_ts(const uint64_t ts)
{
	const std::lock_guard<std::mutex> lock(m);

	this->ts = ts;
}

void video_frame::set_wh(const int w, const int h)
{
	const std::lock_guard<std::mutex> lock(m);

	this->w = w;
	this->h = h;
}

void video_frame::put_data(const uint8_t *const data, const size_t len, const encoding_t e)
{
	const std::lock_guard<std::mutex> lock(m);

	uint8_t *new_data = (uint8_t *)::duplicate(data, len);

	std::pair<uint8_t *, size_t> d { new_data, len };
	auto rc = this->data.emplace(e, d);

	assert(rc.second);
}

std::map<encoding_t, std::pair<uint8_t *, size_t> >::iterator video_frame::gen_encoding(const encoding_t new_e)
{
	auto it_rgb = data.find(E_RGB);

	// TODO improve this. decoding jpeg may be faster for example (when also available).
	if (it_rgb == data.end()) {
		auto it_bgr = data.find(E_BGR);

		if (it_bgr != data.end()) {
			size_t n_bytes = w * h * 3;
			uint8_t *frame_rgb = (uint8_t *)malloc(n_bytes);

			for(int i=0; i<n_bytes; i += 3) {
				frame_rgb[i + 0] = it_bgr->second.first[i + 2];
				frame_rgb[i + 1] = it_bgr->second.first[i + 1];
				frame_rgb[i + 2] = it_bgr->second.first[i + 0];
			}

			std::pair<uint8_t *, size_t> d { frame_rgb, n_bytes };
			auto rc = data.emplace(E_RGB, d);
			assert(rc.second);

			if (new_e == E_RGB)
				return rc.first;
		}

		auto it_yuyv = data.find(E_YUYV);

		if (it_yuyv != data.end()) {
			uint8_t *frame_rgb { nullptr };
			yuy2_to_rgb(it_yuyv->second.first, w, h, &frame_rgb);

			std::pair<uint8_t *, size_t> d { frame_rgb, w * h * 3 };
			auto rc = data.emplace(E_RGB, d);
			assert(rc.second);

			if (new_e == E_RGB)
				return rc.first;
		}
	}

	if (new_e == E_JPEG) {
		auto it = data.find(E_RGB);

		uint8_t *frame_jpeg { nullptr };
		size_t frame_jpeg_len = 0;
		if (!my_jpeg.write_JPEG_memory(m_, w, h, jpeg_quality, it->second.first, &frame_jpeg, &frame_jpeg_len)) {
			log(LL_ERR, "write_JPEG_memory failed");
			return data.end();
		}

		std::pair<uint8_t *, size_t> d { frame_jpeg, frame_jpeg_len };
		auto rc = data.emplace(E_JPEG, d);
		assert(rc.second);

		return rc.first;
	}

	if (new_e == E_RGB) {
		auto it = data.find(E_JPEG);

		uint8_t *frame_rgb { nullptr };
		int dw = -1, dh = -1;
		// FIXME treat "dw != w || dh != h" really as an error?
		if (!my_jpeg.read_JPEG_memory(it->second.first, it->second.second, &dw, &dh, &frame_rgb) || dw != w || dh != h) {
			log(LL_ERR, "read_JPEG_memory failed");
			free(frame_rgb);
			return data.end();
		}

		std::pair<uint8_t *, size_t> d { frame_rgb, w * h * 3 };
		auto rc = data.emplace(E_RGB, d);
		assert(rc.second);

		return rc.first;
	}

	return data.end();
}

uint8_t *video_frame::get_data(const encoding_t e)
{
	return std::get<0>(get_data_and_len(e));
}

std::tuple<uint8_t *, size_t> video_frame::get_data_and_len_internal(const encoding_t e)
{
	auto it = data.find(e);

	if (it == data.end()) {
		it = gen_encoding(e);

		if (it == data.end()) {
			// last resort
			// this path is taken when e.g. a JPEG could not be decoded
			log(LL_WARNING, "returning gray failure frame");

			size_t n_pixels = w * h * 3;
			uint8_t *gray = (uint8_t *)malloc(n_pixels);

			memset(gray, 0x80, n_pixels);

			if (e == E_RGB) {
				// takes ownership of the allocated memory
				return std::make_tuple(gray, n_pixels);
			}

			uint8_t *frame_jpeg { nullptr };
			size_t frame_jpeg_len = 0;
			if (!my_jpeg.write_JPEG_memory(m_, w, h, jpeg_quality, gray, &frame_jpeg, &frame_jpeg_len)) {
				log(LL_ERR, "write_JPEG_memory failed");
				return std::make_tuple(nullptr, 0); // this will probably cause a crash
			}

			free(gray);

			return std::make_tuple(frame_jpeg, frame_jpeg_len);
		}
	}

	return std::make_tuple(it->second.first, it->second.second);
}

std::tuple<uint8_t *, size_t> video_frame::get_data_and_len(const encoding_t e)
{
	const std::lock_guard<std::mutex> lock(m);

	return get_data_and_len_internal(e);
}

std::tuple<int, int> video_frame::get_wh() const
{
	const std::lock_guard<std::mutex> lock(m);

	return std::make_tuple(w, h);
}

int video_frame::get_w() const
{
	const std::lock_guard<std::mutex> lock(m);

	return w;
}

int video_frame::get_h() const
{
	const std::lock_guard<std::mutex> lock(m);

	return h;
}

uint64_t video_frame::get_ts() const
{
	const std::lock_guard<std::mutex> lock(m);

	return ts;
}

void video_frame::update_ts()
{
	const std::lock_guard<std::mutex> lock(m);
	ts = get_us();
}

video_frame *video_frame::do_resize(resize *const r, const int new_w, const int new_h)
{
	assert(new_w > 0);
	assert(new_h > 0);

	std::unique_lock<std::mutex> lock(m);

	auto rc = get_data_and_len_internal(E_RGB);

	uint8_t *resized { nullptr };
	r->do_resize(w, h, std::get<0>(rc), new_w, new_h, &resized);

	lock.unlock();

	return new video_frame(m_, jpeg_quality, ts, new_w, new_h, resized, IMS(new_w, new_h, 3), E_RGB);
}

video_frame *video_frame::duplicate(const std::optional<encoding_t> e)
{
	const std::lock_guard<std::mutex> lock(m);

	video_frame *out = new video_frame(m_, jpeg_quality);

	out->set_ts(ts);
	out->set_wh(w, h);

	if (e.has_value()) {
		auto type = e.value();
		auto rc = get_data_and_len_internal(type);

		out->put_data(std::get<0>(rc), std::get<1>(rc), type);
	}
	else {
		for(auto it : data)
			out->put_data(it.second.first, it.second.second, it.first);
	}

	return out;
}

video_frame *video_frame::apply_filtering(instance *const inst, source *const s, video_frame *const prev, const std::vector<filter *> *const filters, controls *const c)
{
	video_frame *out = duplicate(E_RGB);

	if (filters)
		apply_filters(inst, s, filters, prev ? prev->get_data(E_RGB) : nullptr, out->get_data(E_RGB), out->get_ts(), out->get_w(), out->get_h());

	if (c)
		c->apply(out->get_data(E_RGB), out->get_w(), out->get_h());

	return out;
}

void video_frame::keep_only_format(const encoding_t ek)
{
	const std::lock_guard<std::mutex> lock(m);

	// TODO handle E_YUYV
	auto it = data.find(ek == E_RGB ? E_JPEG : E_RGB);

	if (it != data.end()) {
		free(it->second.first);
		data.erase(it);
	}
}

video_frame *video_frame::do_rotate(const int angle)
{
	auto img = get_data_and_len(E_RGB);
	const uint8_t *data = std::get<0>(img);
	const size_t len = std::get<1>(img);

	if (angle == 90) {
		uint8_t *new_ = (uint8_t *)malloc(len);

		for(int y=0; y<h; y++) {
			for(int x=0; x<w; x++) {
				new_[x * h * 3 + y * 3 + 0] = data[y * w * 3 + x * 3 + 0];
				new_[x * h * 3 + y * 3 + 1] = data[y * w * 3 + x * 3 + 1];
				new_[x * h * 3 + y * 3 + 2] = data[y * w * 3 + x * 3 + 2];
			}
		}

		return new video_frame(m_, jpeg_quality, ts, h, w, new_, IMS(h, w, 3), E_RGB);
	}
	else if (angle == 180) {
		uint8_t *new_ = (uint8_t *)malloc(len);

		for(int y=0; y<h; y++)
			memcpy(&new_[y * w * 3], &data[(h - 1 - y) * w * 3], w * 3);

		return new video_frame(m_, jpeg_quality, ts, w, h, new_, IMS(w, h, 3), E_RGB);
	}
	else if (angle == 270) {
		uint8_t *new_ = (uint8_t *)malloc(len);

		for(int y=0; y<h; y++) {
			for(int x=0; x<w; x++) {
				int mx = w - 1 - x;

				new_[x * h * 3 + y * 3 + 0] = data[y * w * 3 + mx * 3 + 0];
				new_[x * h * 3 + y * 3 + 1] = data[y * w * 3 + mx * 3 + 1];
				new_[x * h * 3 + y * 3 + 2] = data[y * w * 3 + mx * 3 + 2];
			}
		}

		return new video_frame(m_, jpeg_quality, ts, h, w, new_, IMS(h, w, 3), E_RGB);
	}

	return duplicate(E_RGB);
}
