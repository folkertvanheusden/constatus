// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include <map>
#include <mutex>
#include <stdint.h>
#include <tuple>
#include <vector>

#include "encoding.h"

class controls;
class filter;
class instance;
class meta;
class resize;
class source;

class video_frame
{
private:
	mutable std::mutex m;

	const meta *const m_;
	const int jpeg_quality;

	uint64_t ts { 0 };
	int w { -1 }, h { -1 };

	std::map<encoding_t, std::pair<uint8_t *, size_t> > data;

	std::map<encoding_t, std::pair<uint8_t *, size_t> >::iterator gen_encoding(const encoding_t new_e);
	std::tuple<uint8_t *, size_t> get_data_and_len_internal(const encoding_t e);

	video_frame(const meta *const m, const int jpeg_quality);

public:
	video_frame(const meta *const m, const int jpeg_quality, const uint64_t ts, const int w, const int h, uint8_t *const data, const size_t len, const encoding_t e);
	virtual ~video_frame();

	void set_ts(const uint64_t ts);
	void set_wh(const int w, const int h);
	void put_data(const uint8_t *const data, const size_t len, const encoding_t e); // makes a copy
	void set_encoding(const encoding_t e);

	int get_w() const;
	int get_h() const;
	uint8_t *get_data(const encoding_t e);
	std::tuple<uint8_t *, size_t> get_data_and_len(const encoding_t e);
	std::tuple<int, int> get_wh() const;
	uint64_t get_ts() const;
	void keep_only_format(const encoding_t e);

	video_frame *duplicate(const std::optional<encoding_t> e);
	video_frame *do_resize(resize *const r, const int new_w, const int new_h);
	video_frame *apply_filtering(instance *const inst, source *const s, video_frame *const prev, const std::vector<filter *> *const filters, controls *const c);
};
