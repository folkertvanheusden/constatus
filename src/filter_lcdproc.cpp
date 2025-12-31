// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <algorithm>
#include <poll.h>
#include <signal.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <cstring>
#include <unistd.h>

#include "filter_lcdproc.h"
#include "draw_text.h"
#include "utils.h"
#include "log.h"

// https://stackoverflow.com/questions/18675364/c-tokenize-a-string-with-spaces-and-quotes
bool split_in_args(std::vector<std::string>& qargs, std::string command)
{
	qargs.clear();

	int len = command.length();
	bool qot = false, sqot = false, cb = false;
	int arglen = 0;

	for(int i = 0; i < len; i++) {
		int start = i;

		if (command[i] == '\"')
			qot = true;
		else if (command[i] == '\'')
			sqot = true;
		else if (command[i] == '{')
			cb = true;

		if (qot) {
			i++;
			start++;
			while(i<len && command[i] != '\"')
				i++;
			if (i<len)
				qot = false;
			arglen = i - start;
			i++;
		}
		else if (sqot) {
			i++;
			start++;
			while(i<len && command[i] != '\'')
				i++;
			if (i<len)
				sqot = false;
			arglen = i - start;
			i++;
		}
		else if (cb) {
			i++;
			start++;
			while(i<len && command[i] != '}')
				i++;
			if (i<len)
				cb = false;
			arglen = i - start;
			i++;
		}
		else {
			while(i<len && command[i]!=' ')
				i++;

			arglen = i - start;
		}

		qargs.push_back(command.substr(start, arglen));
	}

	if (qot || sqot || cb)
		return false;

	return true;
}

void filter_lcdproc::delete_widget(std::string sid, std::string wid)
{
	for(size_t i=0; i<screens.size();) {
		if (screens.at(i).id != sid)
			continue;

		for(size_t k=0; k<screens.at(i).widgets.size(); k++) {
			if (screens.at(i).widgets.at(k)->id != wid)
				continue;

			delete screens.at(i).widgets.at(k);
			screens.at(i).widgets.erase(screens.at(i).widgets.begin() + k);

			return;
		}
	}
}

void filter_lcdproc::delete_screen(std::string id)
{
	for(size_t i=0; i<screens.size();) {
		if (screens.at(i).id == id) {
			while(!screens.at(i).widgets.empty())
				delete_widget(id, screens.at(i).widgets.at(0)->id);

			screens.erase(screens.begin() + i);
			break;
		}
	}
}

void filter_lcdproc::delete_lcdproc_user(int fd)
{
	for(size_t i=0; i<screens.size();) {
		if (screens.at(i).fd == fd)
			delete_screen(screens.at(i).id);
		else
			i++;
	}
}

std::vector<lcdproc_screen>::iterator filter_lcdproc::find_screen(std::string id)
{
	lpl.lock();

	for(auto it = screens.begin(); it != screens.end(); it++) {
		if (it->id == id)
			return it;
	}

	return screens.end();
}

lcdproc_widget * filter_lcdproc::find_widget(std::string screen_id, std::string widget_id)
{
	lpl.lock();

	for(auto it : screens) {
		if (it.id == screen_id) {
			for(auto cur : it.widgets) {
				if (cur->id == widget_id)
					return cur;
			}

			return nullptr;
		}
	}

	return nullptr;
}

// http://lcdproc.omnipotent.net/download/netstuff.txt
std::pair<bool, std::string> filter_lcdproc::lcdproc_command(const int fd, const char *const cmd)
{
	std::vector<std::string> parts;
       
	if (!split_in_args(parts, cmd)) {
		log(LL_WARNING, "Failed splitting \"%s\"", cmd);
		return { false, "huh?\n" };
	}

	if (parts.size() == 0)
		return { true, "" };

	if (parts.at(0) == "hello")
		return { true, myformat("connect LCDproc 0.5.9 protocol 0.3 lcd wid %d hgt %d cellwid %d cellhgt %d\n", n_col, n_row, font_size, font_size) };

	if (parts.at(0) == "client_set")
		return { true, "success\n" };

	if (parts.at(0) == "client_add_key" || parts.at(0) == "client_del_key")
		return { true, "success\n" };

	if (parts.at(0) == "screen_add" && parts.size() == 2) {
		std::lock_guard<std::mutex> lck(lpl);

		screens.push_back({ fd, 255, parts.at(1), "", 32, { } });

		return { true, myformat("success\nlisten %s\n", parts.at(1).c_str()) };
	}

	if (parts.at(0) == "screen_del" && parts.size() == 2) {
		std::string id = parts.at(1);

		std::lock_guard<std::mutex> lck(lpl);
		delete_screen(id);

		return { true, "success\n" };
	}

	if (parts.at(0) == "screen_set" && parts.size() >= 2) {
		std::string id = parts.at(1);
		bool found = false;

		auto it = find_screen(id);

		if (it != screens.end()) {
			found = true;

			for(size_t i=2; i<parts.size(); i++) {
				std::string parameter = parts.at(i);
				if (parameter.at(0) == '-')
					parameter = parameter.substr(1);

				if (parameter == "priority")
					it->prio = atoi(parts.at(++i).c_str());
				else if (parameter == "name")
					it->name = parts.at(++i);
				else if (parameter == "duration")
					it->duration = atoi(parts.at(++i).c_str());
				else {
					log(LL_WARNING, "screen_set parameter \"%s\" not known", parameter.c_str());
				}
			}
		}
		else {
			log(LL_WARNING, "Screen \"%s\" not found", id.c_str());
		}

		lpl.unlock();

		if (found)
			return { true, "success\n" };

		return { false, "huh?\n" };
	}

	if (parts.at(0) == "widget_add" && parts.size() >= 4) {
		std::string screen_id = parts.at(1);
		std::string widget_id = parts.at(2);

		lcdproc_widget_type_t t = LWT_NONE;
		if (parts.at(3) == "string")
			t = LWT_STRING;
		else if (parts.at(3) == "title")
			t = LWT_TITLE;
		else if (parts.at(3) == "hbar")
			t = LWT_HBAR;
		else if (parts.at(3) == "vbar")
			t = LWT_VBAR;
		else if (parts.at(3) == "frame")
			t = LWT_FRAME;
		else if (parts.at(3) == "num")
			t = LWT_NUM;
		else if (parts.at(3) == "scroller")
			t = LWT_STRING; // FIXME

		auto it = find_screen(screen_id);
		if (it != screens.end()) {
			std::string in;

			if (parts.size() == 6 && parts.at(4) == "-in")
				in = parts.at(5);

			if (t == LWT_NONE) {
				log(LL_WARNING, "Not supporting widget type \"%s\"", parts.at(3).c_str());

				lcdproc_widget *dummy = new lcdproc_widget { LWT_STRING, widget_id };
				it->widgets.push_back(dummy);
			}
			else if (t == LWT_FRAME) {
				lcdproc_frame *frame = new lcdproc_frame { t, widget_id };
				it->widgets.push_back(frame);
			}
			else {
				lcdproc_widget *w = new lcdproc_widget { t, widget_id };
				w->in = in;
				it->widgets.push_back(w);
			}
		}

		lpl.unlock();

		if (it == screens.end())
			log(LL_WARNING, "Screen \"%s\" not found", id.c_str());

		if (it != screens.end())
			return { true, "success\n" };

		return { false, "huh?\n" };
	}

	if (parts.at(0) == "widget_del" && parts.size() == 3) {
		std::string screen_id = parts.at(1);
		std::string widget_id = parts.at(2);

		std::lock_guard<std::mutex> lck(lpl);
		delete_widget(screen_id, widget_id);

		return { true, "success\n" };
	}

	if (parts.at(0) == "widget_set" && parts.size() >= 3) {
		std::string screen_id = parts.at(1);
		std::string widget_id = parts.at(2);

		bool found = false;

		lcdproc_widget *widget = find_widget(screen_id, widget_id);
		if (widget) {
			found = true;

			if (widget->type == LWT_TITLE && parts.size() == 4)
				widget->text = parts.at(3);
			else if (widget->type == LWT_STRING && parts.size() >= 6) {
				widget->x = atoi(parts.at(3).c_str()) - 1;
				widget->y = atoi(parts.at(4).c_str()) - 1;
				widget->text = parts.at(5);
			}
			else if ((widget->type == LWT_HBAR || widget->type == LWT_VBAR) && parts.size() == 6) {
				widget->x = atoi(parts.at(3).c_str()) - 1;
				widget->y = atoi(parts.at(4).c_str()) - 1;
				widget->text = std::string(atoi(parts.at(5).c_str()) / font_size, '*');
				// printf("%d: %d,%d, %d/%s\n", widget->type, widget->x, widget->y, atoi(parts.at(5).c_str()) / font_size, widget->text.c_str());
			}
			else if (widget->type == LWT_FRAME && parts.size() == 11) {
				lcdproc_frame *frame = (lcdproc_frame *)widget;
				frame->left = atoi(parts.at(3).c_str()) - 1;
				frame->top = atoi(parts.at(4).c_str()) - 1;
				frame->right = atoi(parts.at(5).c_str()) - 1;
				frame->bottom = atoi(parts.at(6).c_str()) - 1;
				frame->width = atoi(parts.at(7).c_str());
				frame->height = atoi(parts.at(8).c_str());
				frame->direction = parts.at(9).at(0);
				frame->speed = atoi(parts.at(10).c_str());
			}
			else if (widget->type == LWT_NUM && parts.size() == 5) {
				widget->x = atoi(parts.at(3).c_str()) - 1;
				widget->text = parts.at(4) == "10" ? ":" : parts.at(4);
			}
			else {
				found = false;
				log(LL_WARNING, "Unknown widget");
			}
		}

		lpl.unlock();

		if (found)
			return { true, "success\n" };

		return { false, "huh?\n" };
	}

	if (parts.at(0) == "backlight")
		return { true, "success\n" };

	if (parts.at(0).substr(0, 4) == "menu") {
		log(LL_WARNING, "Menu-commands (\"%s\") are not implemented", parts.at(0).c_str());
		return { true, "success\n" };
	}

	log(LL_WARNING, "Unknown command: \"%s\"", cmd);

	return { false, "huh?\n" };
}

void filter_lcdproc::protocol_processor(std::atomic_bool *const stopped_indicator, std::atomic_bool *const local_stop_flag, const int fd)
{
	sigset_t all_sigs;
	sigfillset(&all_sigs);
	pthread_sigmask(SIG_BLOCK, &all_sigs, nullptr);

	char buffer[4096];
	int o = 0;

	struct pollfd fds[] { { fd, POLLIN, 0 } };

	while(!*local_stop_flag) {
		if (poll(fds, 1, 500) == 0)
			continue;

		if (o == sizeof buffer) {
			log(LL_WARNING, "buffer overflow");
			break;
		}

		int n = read(fd, &buffer[o], sizeof buffer - 1 - o);
		if (n <= 0) {
			log(LL_INFO, "read error: %s", strerror(errno));
			break;
		}

		buffer[o + n] = 0x00;

		o += n;

		for(;;) {
			char *lf = strchr(buffer, '\n');
			if (!lf)
				break;

			*lf = 0x00;

			log(LL_DEBUG, "lcdproc command: %s", buffer);

			try {
				auto rc = lcdproc_command(fd, buffer);

				if (rc.second.empty() == false)
					WRITE(fd, rc.second.c_str(), rc.second.size());

				if (rc.first == false) {
					log(LL_INFO, "Command \"%s\" failed", buffer);
					break;
				}
			}
			catch(const std::runtime_error & re) {
				log(LL_ERR, "Runtime error for %s", re.what());
			}
			catch(const std::exception & ex) {
				log(LL_ERR, "std::exception for %s", ex.what());
			}
			catch(...) {
				log(LL_ERR, "Exception in lcdproc_command");
			}

			int offset_lf = lf - buffer;
			int bytes_left = o - (offset_lf + 1);

			if (bytes_left == 0) {
				o = 0;
				break;
			}

			if (bytes_left < 0) {
				log(LL_ERR, "lcdproc: %d bytes left", bytes_left);
				break;
			}

			memmove(buffer, lf + 1, bytes_left);
			o = bytes_left;
			buffer[o] = 0x00;
		}
	}

	std::lock_guard<std::mutex> lck(lpl);
	delete_lcdproc_user(fd);

	close(fd);

	*stopped_indicator = true;
}

void filter_lcdproc::network_listener()
{
	set_thread_name("lcdproc");

	log(LL_INFO, "LCDProc network listener started");

	int fd = start_listen({ adapter, 13666, 128, false });

	struct pollfd fds[] { { fd, POLLIN, 0 } };

	while(!local_stop_flag) {
		for(size_t i=0; i<threads.size();) {
			if (threads.at(i)->is_terminated) {
				threads.at(i)->th->join();
				delete threads.at(i)->th;
				delete threads.at(i);

				threads.erase(threads.begin() + i);
			}
			else {
				i++;
			}
		}

		if (poll(fds, 1, 500) == 0)
			continue;

		int cfd = accept(fd, nullptr, 0);
		if (cfd == -1)
			continue;

		thread_info_t *ti = new thread_info_t;
		ti->is_terminated = false;
		ti->th = new std::thread(&filter_lcdproc::protocol_processor, this, &ti->is_terminated, &local_stop_flag, cfd);

		threads.push_back(ti);
	}

	log(LL_INFO, "LCDProc network listener stopping");

	for(auto t : threads) {
		t->th->join();
		delete t->th;	
		delete t;
	}

	close(fd);

	log(LL_INFO, "LCDProc network listener stopped");
}

filter_lcdproc::filter_lcdproc(const std::string & adapter, const std::vector<std::string> & font_files, const int x, const int y, const int w, const int h, const std::optional<rgb_t> bg, const int n_col, const int n_row, const rgb_t col, const int switch_interval, const bool invert) : adapter(adapter), x(x), y(y), w(w), h(h), bg(bg), n_col(n_col), n_row(n_row), col(col), switch_interval(switch_interval), invert(invert)
{
	font_size = std::min(w / n_col, h / n_row);

	font = new draw_text(font_files, font_size);

	th = new std::thread(&filter_lcdproc::network_listener, this);
}

filter_lcdproc::~filter_lcdproc()
{
	local_stop_flag = true;

	th->join();
	delete th;
	
	delete font;
}

void filter_lcdproc::apply(instance *const inst, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	int disp_len = n_col * n_row;
	char *display = new char[disp_len];
	memset(display, ' ', disp_len);

	if (last_next == 0)
		last_next = ts;

	std::unique_lock<std::mutex> lck(lpl);

	if (cur_scr_nr >= screens.size())
		cur_scr_nr = 0;

	if (screens.empty() == false) {
		auto cs = screens.at(cur_scr_nr);

		if (cs.widgets.empty())
			log(LL_DEBUG, "Screen \"%s\" is empty", cs.id.c_str());

		for(auto cw : cs.widgets) {

			int tgtx = -1, tgty = -1;
			if (cw->in.empty() == false) {
				tgtx = ((lcdproc_frame *)cw)->left;
				tgty = ((lcdproc_frame *)cw)->top;
			}

			if (cw->type == LWT_TITLE) {
				int len = std::min(int(cw->text.size()), n_col);

				memcpy(&display[0], cw->text.c_str(), len);
			}
			else if (cw->type == LWT_STRING || cw->type == LWT_HBAR || cw->type == LWT_NUM) {
				if (tgtx == -1) {
					int len = std::min(int(cw->text.size()), n_col - cw->x);

					memcpy(&display[cw->y * n_col + cw->x], cw->text.c_str(), len);
				}
				else {
					int tgtlen = ((lcdproc_frame *)cw)->right - ((lcdproc_frame *)cw)->left;
					int worklen = std::min(int(cw->text.size()), tgtlen);

					memcpy(&display[(tgty + cw->y) * n_col + tgtx + cw->x], cw->text.c_str(), worklen);
				}
			}
			else if (cw->type == LWT_VBAR) {
				if (tgtx == -1) {
					int len = cw->text.size();

					for(int i=0; i<len; i++) {
						int o = (cw->y - i) * n_col + cw->x;
						if (o < 0)
							break;

						display[o] = cw->text[i];
					}
				}
				else {
					int tgtlen = ((lcdproc_frame *)cw)->bottom - ((lcdproc_frame *)cw)->top;
					int len = std::min(int(cw->text.size()), tgtlen);

					int o = (tgty + cw->y) * n_col + tgtx + cw->x;

					for(int i=0; i<len; i++, o += n_col) {
						if (o >= disp_len)
							break;

						display[o] = cw->text[i];
					}
				}
			}
		}
	}

	if (ts - last_next >= switch_interval * 1000l && screens.empty() == false) {
		cur_scr_nr = (cur_scr_nr + 1) % screens.size();

		log(LL_DEBUG, "new screen: %s", screens.at(cur_scr_nr).id.c_str());

		last_next = ts;
	}

	lck.unlock();

	int work_x = x;
	int work_y = y;

	for(int iy=0; iy<n_row; iy++) {
		std::string row { &display[iy * n_col], size_t(n_col) };

		draw_text_on_bitmap(font, row, w, h, in_out, font_size, col, bg, work_x, work_y);

		work_y += font_size;
	}

	delete [] display;
}
