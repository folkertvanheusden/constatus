// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <cstring>
#include <time.h>
#include <unistd.h>

#include "gen.h"
#include "filter_add_text.h"
#include "filter_scroll.h"
#include "error.h"
#include "utils.h"
#include "exec.h"

void blit(uint8_t *const out, const int w, const int h, const int x, const int y, const uint8_t *const in, const int in_w, const int in_h, const int off_x, const int off_y)
{
	for(int wy=off_y; wy<in_h; wy++) {
		int target_y = y + wy;
		if (target_y >= h)
			break;
		if (target_y < 0)
			continue;

		int out_offset = target_y * w * 3;
		int in_offset  = wy * in_w;

		for(int wx=off_x; wx<in_w; wx++) {
			int target_x = wx + x;
			if (target_x >= w)
				break;
			if (target_x < 0)
				continue;

			int temp_out_offset = out_offset + target_x * 3;
			int temp_in_offset  = in_offset  + wx;

			const uint8_t c     = in[temp_in_offset];

			out[temp_out_offset + 0] = c;
			out[temp_out_offset + 1] = c;
			out[temp_out_offset + 2] = c;
		}
	}
}

filter_scroll::filter_scroll(const std::string & font_file, const int x, const int y, const int text_w, const int n_lines, const int font_size, const std::string & exec_what, const bool horizontal_scroll, const std::optional<rgb_t> bg, const int scroll_speed, const rgb_t col, const bool invert, const std::map<std::string, feed *> & text_feeds) : x(x), y(y), text_w(text_w), n_lines(n_lines), font_size(font_size), exec_what(exec_what), horizontal_scroll(horizontal_scroll), bg(bg), scroll_speed(scroll_speed), col(col), invert(invert), text_feeds(text_feeds)
{
	font = new draw_text(font_file, font_size);

	restart_process();

	start();
}

filter_scroll::~filter_scroll()
{
	local_stop_flag = true;

	if (fd != -1)
		close(fd);

	stop();

	while(!buffer.empty()) {
		free(buffer.at(0).bitmap);

		buffer.erase(buffer.begin());
	}
}

void filter_scroll::operator()()
{
	while(!local_stop_flag) {
		auto [ has_data, data ] = poll_for_data();

		if (has_data && !data.empty()) {
			scroll_entry_t se { "", nullptr, 0, 0 };
			se.text = data;

			const std::lock_guard<std::mutex> lock(buffer_lock);

			while(buffer.size() >= n_lines) {
				delete [] buffer.at(0).bitmap;

				buffer.erase(buffer.begin());
			}

			buffer.push_back(se);
		}

		st->track_cpu_usage();
	}
}

void filter_scroll::restart_process()
{
	if (fd != -1)
		close(fd);

	pid_t pid;
	exec_with_pty(exec_what, &fd, &pid);
}

std::tuple<bool, std::string> filter_scroll::poll_for_data()
{
	struct pollfd fds[] = { { fd, POLLIN, 0 } };

	if (poll(fds, 1, 1) == 1) {
		char in[4096];
		int rc = read(fd, in, sizeof in - 1);

		if (rc <= 0) {
			restart_process();

			return { false, "" };
		}

		in[rc] = 0x00;

		char *p = in;
		for(;;) {
			char *cr = strchr(p, '\r');
			if (!cr)
				break;

			*cr = ' ';
			p = cr;
		}

		in_buffer += in;
	}

	size_t lf = in_buffer.find('\n');
	if (lf != std::string::npos) {
		std::string out = in_buffer.substr(0, lf);
		in_buffer = in_buffer.substr(lf + 1);

		return { true, out };
	}

	return { false, "" };
}

void filter_scroll::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	int work_x = x < 0 ? x + w : x;
	int work_y = y < 0 ? y + h : y;

	if (horizontal_scroll) {
		std::string scroll_what;

		const std::lock_guard<std::mutex> lock(buffer_lock);
		for(auto & what : buffer) {
			if (what.bitmap == nullptr) {
				std::string text = unescape(what.text, ts, i, specific_int, text_feeds);

				font->draw_string(text, font_size, &what.bitmap, &what.w);
				what.h = font_size;
			}
		}

		int x = work_x, use_w = text_w == -1 ? w : text_w;
		size_t bitmap_nr = 0;
		bool first = true;

		while(x < use_w && !buffer.empty()) {
			int offset_x = first ? cur_x_pos : 0;
			first = false;

			blit(in_out, w, h, x - offset_x, work_y, buffer.at(bitmap_nr).bitmap, buffer.at(bitmap_nr).w, buffer.at(bitmap_nr).h, offset_x, 0);

			x += buffer.at(bitmap_nr).w - offset_x;

			bitmap_nr++;
			if (bitmap_nr >= buffer.size())
				bitmap_nr = 0;
		}

		if (!buffer.empty())
			cur_x_pos = (ts / (1000000 / scroll_speed)) % buffer.at(0).w;
	}
	else {
		const std::lock_guard<std::mutex> lock(buffer_lock);

		for(auto what : buffer) {
			std::string text_out = unescape(what.text, ts, i, specific_int, text_feeds);

			std::vector<std::string> *parts { nullptr };
			if (text_out.find("\n") != std::string::npos)
				parts = split(text_out.c_str(), "\n");
			else
				parts = split(text_out.c_str(), "\\n");

			for(std::string cl : *parts) {
				draw_text_on_bitmap(font, cl, w, h, in_out, font_size, col, bg, work_x, work_y);

				work_y += font_size + 1;
			}

			delete parts;
		}
	}
}
