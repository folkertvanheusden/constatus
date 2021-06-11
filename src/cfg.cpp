// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <dlfcn.h>
#include <libconfig.h++>
#include <set>
#include <string>
#include <cstring>
#include <vector>

using namespace libconfig;

#include "instance.h"
#include "config.h"
#include "error.h"
#include "utils.h"
#include "parameters.h"
#include "source.h"
#if HAVE_LIBV4L2 == 1
#include "source_v4l.h"
#endif
#if HAVE_LIBCAMERA == 1
#include "source_libcamera.h"
#endif
#include "source_http_mjpeg.h"
#include "source_http_jpeg.h"
#include "source_filesystem_jpeg.h"
#include "source_ffmpeg.h"
#include "source_plugin.h"
#if HAVE_GSTREAMER == 1
#include "source_gstreamer.h"
#endif
#include "source_pixelflood.h"
#include "source_static.h"
#include "source_black.h"
#include "webservices.h"
#include "http_server.h"
#include "audio_trigger.h"
#include "motion_trigger.h"
#include "motion_trigger_generic.h"
#include "motion_trigger_other_source.h"
#include "target.h"
#if HAVE_GSTREAMER == 1
#include "target_avi.h"
#endif
#if HAVE_FFMPEG == 1
#include "target_ffmpeg.h"
#endif
#if HAVE_GSTREAMER == 1
#include "target_gstreamer.h"
#endif
#include "target_jpeg.h"
#include "target_plugin.h"
#include "target_extpipe.h"
#include "target_vnc.h"
#include "target_pixelflood.h"
#include "target_new_source.h"
#include "filter.h"
#include "filter_add_bitmap.h"
#include "filter_mirror_v.h"
#include "filter_mirror_h.h"
#include "filter_noise_neighavg.h"
#include "filter_apply_mask.h"
#include "filter_add_text.h"
#include "filter_add_scaled_text.h"
#include "filter_grayscale.h"
#include "filter_boost_contrast.h"
#include "filter_marker_simple.h"
#include "filter_average.h"
#include "filter_median.h"
#include "filter_draw.h"
#include "filter_chromakey.h"
#include "filter_copy.h"
#include "filter_despeckle.h"
#include "filter_lcdproc.h"
#if HAVE_FREI0R == 1
#include "filter_plugin_frei0r.h"
#endif
#include "filter_scroll.h"
#if HAVE_IMAGICK == 1
#include "filter_anigif_overlay.h"
#endif
#include "gui.h"
#if HAVE_LIBSDL2 == 1
#include "gui_sdl.h"
#endif
#if HAVE_LIBV4L2 == 1
#include "v4l2_loopback.h"
#endif
#include "filter_overlay.h"
#include "picio.h"
#include "filter_plugin.h"
#include "log.h"
#include "cfg.h"
#include "resize_crop.h"
#include "resize_fine.h"
#include "filter_motion_only.h"
#include "selection_mask.h"
#include "cleaner.h"
#include "view_html_grid.h"
#include "view_ss.h"
#include "view_html_all.h"
#include "view_all.h"
#include "view_pip.h"
#include "view_3d.h"
#include "source_delay.h"
#include "http_auth.h"
#include "http_auth_pam.h"
#include "controls_software.h"
#include "controls_v4l.h"
#include "announce_upnp.h"
#include "audio_alsa.h"
#include "schedule.h"
#include "pos.h"

int to_loglevel(const std::string & ll)
{
	if (ll == "fatal")
		return LL_FATAL;

	if (ll == "error")
		return LL_ERR;

	if (ll == "warning")
		return LL_WARNING;

	if (ll == "info")
		return LL_INFO;

	if (ll == "debug")
		return LL_DEBUG;

	if (ll == "debug-verbose")
		return LL_DEBUG_VERBOSE;

	fprintf(stderr, "Log level '%s' not recognized\n", ll.c_str());

	return -1;
}

void find_by_id(const configuration_t *const cfg, const std::string & id, instance **inst, interface **i)
{
	*inst = nullptr;
	*i = nullptr;

	for(instance * cur_inst : cfg -> instances) {
		for(interface *cur_i : cur_inst -> interfaces) {
			if (cur_i -> get_id() == id) {
				*inst = cur_inst;
				*i = cur_i;
			}
		}
	}
}

source *find_source(instance *const inst)
{
	if (!inst)
		return nullptr;

	for(interface *i : inst -> interfaces) {
		if (i -> get_class_type() == CT_SOURCE)
			return (source *)i;
	}

	return nullptr;
}

view *find_view(instance *const inst)
{
	if (!inst)
		return nullptr;

	for(interface *i : inst -> interfaces) {
		if (i -> get_class_type() == CT_VIEW)
			return (view *)i;
	}

	return nullptr;
}

view *find_view_by_child(const configuration_t *const cfg, const std::string & child_id)
{
	for(instance * cur_inst : cfg -> instances) {
		for(interface *cur_i : cur_inst -> interfaces) {
			if (cur_i -> get_class_type() == CT_VIEW) {
				auto sources = ((view *)cur_i)->get_sources();

				for(auto s : sources) {
					if (s.id == child_id)
						return (view *)cur_i;
				}
			}
		}
	}

	return nullptr;
}

view *find_view(instance *const views, const std::string & id)
{
	if (!views)
		return NULL;

	for(interface *cur : views -> interfaces) {
		view *v = (view *)cur;
		if (v -> get_id() == id)
			return v;
	}

	return NULL;
}

std::vector<interface *> find_motion_triggers(instance *const inst)
{
	std::vector<interface *> out;

	if (inst) {
		for(interface *i : inst -> interfaces) {
			if (i -> get_class_type() == CT_MOTIONTRIGGER)
				out.push_back(i);
		}
	}

	return out;
}

instance *find_instance_by_interface(const configuration_t *const cfg, const interface *i_f)
{
	for(instance * inst : cfg -> instances) {
		for(interface *cur : inst -> interfaces) {
			if (cur == i_f)
				return inst;
		}
	}

	return nullptr;
}

instance *find_instance_by_name(const configuration_t *const cfg, const std::string & name)
{
	for(instance * inst : cfg -> instances) {
		if (inst -> name == name)
			return inst;
	}

	return nullptr;
}

interface *find_interface_by_id(instance *const inst, const std::string & id)
{
	if (!inst) 
		return nullptr;

	for(interface *i : inst -> interfaces) {
		std::string id_int = i -> get_id();

		if (id == id_int)
			return i;
	}

	return nullptr;
}

interface *find_interface_by_id(configuration_t *const cfg, const std::string & id)
{
	for(auto inst : cfg -> instances) {
		interface *i = find_interface_by_id(inst, id);
		if (i)
			return i;
	}

	for (auto i : cfg -> global_http_servers) {
		if (i->get_id() == id)
			return i;
	}

	return nullptr;
}

std::optional<std::pair<std::string, error_state_t> > find_errors(instance *const inst)
{
	for(interface *cur_i : inst->interfaces) {
		auto err = cur_i->get_last_error();

		if (err.has_value()) {
			std::pair<std::string, error_state_t> rc = { cur_i->get_id(), err.value() };

			return rc;
		}
	}

	return { };
}

std::optional<std::pair<std::string, error_state_t> > find_errors(configuration_t *const cfg)
{
	for(instance * cur_inst : cfg -> instances) {
		auto rc = find_errors(cur_inst);
		
		if (rc.has_value())
			return rc;
	}

	return { };
}

bool check_for_motion(instance *const inst)
{
	for(interface *i : inst -> interfaces) {
		if (i -> get_class_type() == CT_MOTIONTRIGGER && ((motion_trigger *)i) -> check_motion())
			return true;
	}

	return false;
}

std::string cfg_str(const Config & cfg, const std::string & key, const char *descr, const bool optional, const std::string & def)
{
	try {
		return (const char *)cfg.lookup(key.c_str());
	}
	catch(const SettingNotFoundException &nfex) {
		if (!optional)
			error_exit(false, "\"%s\" not found (%s)", key.c_str(), descr);
	}

	log(LL_DEBUG, "\"%s\" not found (%s), assuming default (%s)", key.c_str(), descr, def.c_str());

	return def; // field is optional
}

std::string cfg_str(const Setting & cfg, const std::string & key, const char *descr, const bool optional, const std::string & def, std::map<std::string, parameter *> *const dp = nullptr)
{
	std::string v = def;

	try {
		v = (const char *)cfg.lookup(key.c_str());
	}
	catch(const SettingNotFoundException &nfex) {
		if (!optional)
			error_exit(false, "\"%s\" not found (%s)", key.c_str(), descr);

		log(LL_DEBUG, "\"%s\" not found (%s), assuming default (%s)", key.c_str(), descr, def.c_str());
	}

	catch(const SettingTypeException & ste) {
		error_exit(false, "Expected a string value for \"%s\" (%s) at line %d but got something else (%s)", key.c_str(), descr, cfg.getSourceLine(), ste.what());
	}

	if (dp)
		parameter::add_value(*dp, key, descr, v);

	return v;
}

double cfg_float(const Setting & cfg, const char *const key, const char *descr, const bool optional, const double def=-1.0, std::map<std::string, parameter *> *const dp = nullptr)
{
	double v = def;

	try {
		v = cfg.lookup(key);
	}
	catch(const SettingNotFoundException &nfex) {
		if (!optional)
			error_exit(false, "\"%s\" not found (%s)", key, descr);

		log(LL_DEBUG, "\"%s\" not found (%s), assuming default (%f)", key, descr, def);
	}

	catch(const SettingTypeException & ste) {
		error_exit(false, "Expected a float value for \"%s\" (%s) at line %d but got something else (did you forget to add \".0\"?)", key, descr, cfg.getSourceLine());
	}

	if (dp)
		parameter::add_value(*dp, key, descr, v);

	return v;
}

int cfg_int(const Setting & cfg, const std::string & key, const char *descr, const bool optional, const int def=-1, std::map<std::string, parameter *> *const dp = nullptr)
{
	int v = def;

	try {
		v = cfg.lookup(key.c_str());
	}
	catch(const SettingNotFoundException &nfex) {
		if (!optional)
			error_exit(false, "\"%s\" not found (%s)", key.c_str(), descr);

		log(LL_DEBUG, "\"%s\" not found (%s), assuming default (%d)", key.c_str(), descr, def);
	}

	catch(const SettingTypeException & ste) {
		error_exit(false, "Expected an int value for \"%s\" (%s) at line %d but got something else", key.c_str(), descr, cfg.getSourceLine());
	}

	if (dp)
		parameter::add_value(*dp, key, descr, v);

	return v;
}

int cfg_bool(const Config & cfg, const char *const key, const char *descr, const bool optional, const bool def=false)
{
	bool v = def;

	try {
		v = cfg.lookup(key);
	}
	catch(const SettingNotFoundException &nfex) {
		if (!optional)
			error_exit(false, "\"%s\" not found (%s)", key, descr);

		log(LL_DEBUG, "\"%s\" not found (%s), assuming default (%d)", key, descr, def);
	}

	catch(const SettingTypeException & ste) {
		error_exit(false, "Expected a boolean value for \"%s\" (%s) but got something else", key, descr);
	}

	return v;
}

int cfg_bool(const Setting & cfg, const char *const key, const char *descr, const bool optional, const bool def=false, std::map<std::string, parameter *> *const dp = nullptr)
{
	bool v = def;

	try {
		v = cfg.lookup(key);
	}
	catch(const SettingNotFoundException &nfex) {
		if (!optional)
			error_exit(false, "\"%s\" not found (%s)", key, descr);

		log(LL_DEBUG, "\"%s\" not found (%s), assuming default (%d)", key, descr, def);
	}

	catch(const SettingTypeException & ste) {
		error_exit(false, "Expected a boolean value for \"%s\" (%s) at line %d but got something else", key, descr, cfg.getSourceLine());
	}

	if (dp)
		parameter::add_value(*dp, key, descr, v);

	return v;
}

pixelflood_protocol_t cfg_convert_pp(const std::string & pp_str)
{
	pixelflood_protocol_t pp = pixelflood_protocol_t(-1);
	if (pp_str == "tcp-txt")
		pp = PP_TCP_TXT;
	else if (pp_str == "udp-bin")
		pp = PP_UDP_BIN;
	else
		error_exit(false, "Pixelflut format \"%s\" is unknown", pp_str.c_str());

	return pp;
}

void add_filters(std::vector<filter *> *af, const std::vector<filter *> *const in)
{
	for(filter *f : *in)
		af -> push_back(f);
}

failure_t def_fail;

failure_t default_failure()
{
	return def_fail;
}

void set_default_failure()
{
	def_fail.message = "Camera down!";
	def_fail.position = pos_to_pos("center-center");
	def_fail.fm = F_MESSAGE;
}

failure_t load_failure(const Setting & cfg)
{
	failure_t failure;

	try {
		Setting & fh = cfg["failure-handling"];

		failure.bg_bitmap = cfg_str(fh, "bitmap", "bitmap to set as background in case of failure (optional)", true, "");
		failure.message = cfg_str(fh, "message", "text to put on display in case of failure (optional)", true, "");
		failure.position = pos_to_pos(cfg_str(fh, "position", "where to put the text", true, "center-center"));

		std::string mode = cfg_str(fh, "mode", "how to handle failures: message, simple or nothing (which shows a black message)", true, "message");
		if (mode == "nothing")
			failure.fm = F_NOTHING;
		else if (mode == "message")
			failure.fm = F_MESSAGE;
		else if (mode == "simple")
			failure.fm = F_SIMPLE;
		else
			error_exit(false, "Failure mode \"%s\" is not known", mode.c_str());
	}
	catch(SettingNotFoundException & snfe) {
		return def_fail;
	}

	return failure;
}

std::optional<rgb_t> get_color(const Setting & cfg, const std::string & prefix = "")
{
	rgb_t col { 255, 0, 0 };

	std::string color = cfg_str(cfg, prefix + "color", "color name (red, blue, green, black, white, magenta, cyan, yellow)", true, "");
	std::string rgb = cfg_str(cfg, prefix + "rgb", "r,g,b value (seperated by comma)", true, "");

	if (color == "red")
		col = { 255, 0, 0 };
	else if (color == "green")
		col = { 0, 255, 0 };
	else if (color == "blue")
		col = { 0, 0, 255 };
	else if (color == "black")
		col = { 0, 0, 0 };
	else if (color == "white")
		col = { 255, 255, 255 };
	else if (color == "magenta")
		col = { 255, 0, 255 };
	else if (color == "cyan")
		col = { 0, 255, 255 };
	else if (color == "yellow")
		col = { 255, 255, 0 };
	else if (color.empty()) {
		if (rgb.empty()) {
			int r = cfg_int(cfg, prefix + "r", "red value for color", true, -1);
			if (r == -1)
				return { };

			col.g = cfg_int(cfg, prefix + "g", "green value for color", false, 255);
			col.b = cfg_int(cfg, prefix + "b", "blue value for color", false, 255);
		}
		else {
			std::vector<std::string> *parts = split(rgb, ",");
			if (parts->size() != 3)
				error_exit(false, "rgb triplet consists of 3 values");
			col.r = atoi(parts->at(0).c_str());
			col.g = atoi(parts->at(1).c_str());
			col.b = atoi(parts->at(2).c_str());
			delete parts;
		}
	}
	else {
		error_exit(false, "Color \"%s\" is not known", color.c_str());
	}

	return col;
}

bool find_interval_or_fps(const Setting & cfg, double *const interval, const std::string & fps_name, double *const fps)
{
	bool have_interval = cfg.lookupValue("interval", *interval);
	bool have_fps = cfg.lookupValue(fps_name.c_str(), *fps);

	if (!have_interval && !have_fps)
		return false;

	if (have_interval && have_fps)
		return false;

	if (have_interval) {
		if (*interval == 0)
			return false;

		if (*interval < 0)
			*fps = -1.0;
		else
			*fps = 1.0 / *interval;
	}
	else {
		if (*fps == 0)
			return false;

		if (*fps <= 0)
			*interval = -1.0;
		else
			*interval = 1.0 / *fps;
	}

	return true;
}

pos_t find_xy_or_pos(const Setting & cfg, const std::string & what)
{
	pos_t pos { none };

	try {
		std::string s_pos = (const char *)cfg.lookup("position");

		pos = pos_to_pos(s_pos);
	}
	catch(SettingNotFoundException & snfe) {
		pos.type = axy;
		pos.x = cfg_int(cfg, "x", myformat("x-coordinate of %s", what.c_str()).c_str(), false, 0);
		pos.y = cfg_int(cfg, "y", myformat("y-coordinate of %s", what.c_str()).c_str(), false, 0);
	}

	return pos;
}

std::vector<filter *> *load_filters(configuration_t *const cfg, const Setting & in)
{
	std::vector<filter *> *const filters = new std::vector<filter *>();

	size_t n_f = in.getLength();
	log(LL_DEBUG, " %zu filters", n_f);

	for(size_t i=0; i<n_f; i++) {
		const Setting & ae = in[i];

		std::string s_type = cfg_str(ae, "type", "filter-type (h-mirror, marker, etc)", false, "");
		if (s_type == "h-mirror")
			filters -> push_back(new filter_mirror_h());
		else if (s_type == "v-mirror")
			filters -> push_back(new filter_mirror_v());
		else if (s_type == "neighbour-noise-filter")
			filters -> push_back(new filter_noise_neighavg());
		else if (s_type == "grayscale") {
			std::string mode = cfg_str(ae, "mode", "conversion mode: fast, cie-1931, lightness, pal or ntsc", true, "fast");

			to_grayscale_t m = G_FAST;
			if (mode == "fast")
				m = G_FAST;
			else if (mode == "cie-1931")
				m = G_CIE_1931;
			else if (mode == "pal" || mode == "ntsc")
				m = G_PAL_NTSC;
			else if (mode == "lightness")
				m = G_LIGHTNESS;
			else
				error_exit(false, "Conversion mode \"%s\" not supported (grayscale filter)", mode.c_str());

			filters -> push_back(new filter_grayscale(m));
		}
		else if (s_type == "filter-plugin") {
			std::string file = cfg_str(ae, "file", "filename of filter plugin", false, "");
			std::string par = cfg_str(ae, "par", "parameter for filter plugin", true, "");

			filters -> push_back(new filter_plugin(file, par));
		}
#if HAVE_FREI0R == 1
		else if (s_type == "filter-plugin-frei0r" || s_type == "frei0r") {
			std::string file = cfg_str(ae, "file", "filename of frei0r filter plugin", false, "");
			std::string pars = cfg_str(ae, "pars", "parameters for frei0r filter plugin (seperated by spaces, k=v pairs)", true, "");

			std::vector<source *> other_sources;

			try {
				const Setting & s_ids = ae.lookup("sources");

				for(int i=0; i<s_ids.getLength(); i++) {
					const Setting & src = s_ids[i];

					std::string id = cfg_str(src, "id", "ID of source", false, "");

					interface *int_ = find_interface_by_id(cfg, id);
					if (!int_)
						error_exit(false, "Source with id \"%\" not found", id.c_str());

					other_sources.push_back((source *)int_);
				}
			}
			catch(SettingNotFoundException & snfe) {
			}

			filters -> push_back(new filter_plugin_frei0r(file, pars, other_sources, cfg->r));
		}
#endif
		else if (s_type == "marker") {
			std::string motion_source = cfg_str(ae, "motion-source", "source to monitor, leave empty for monitoring by itself (less efficient)", true, "");
			instance *const i = motion_source.empty() ? nullptr : (instance *)find_instance_by_name(cfg, motion_source);

			std::string s_type = cfg_str(ae, "marker-type", "marker type, e.g. box, cross, circle or border", true, "box");

			sm_type_t st = t_box;
			if (s_type == "box")
				st = t_box;
			else if (s_type == "cross")
				st = t_cross;
			else if (s_type == "circle")
				st = t_circle;
			else if (s_type == "border")
				st = t_border;

			sm_mode_t sm = m_red;
			if (st != t_border) {
				std::string s_mode = cfg_str(ae, "mode", "marker mode, e.g. red, red-invert, invert or color - ", false, "red");

				if (s_mode == "red")
					sm = m_red;
				else if (s_mode == "red-invert")
					sm = m_red_invert;
				else if (s_mode == "invert")
					sm = m_invert;
				else if (s_mode == "color")
					sm = m_color;
			}

			auto color = cfg_str(ae, "pixels-color", "when \"mode\" is set to \"color\", select an rgb tripple here to use (comma seperated, no spaces)", true, "255,255,0");

			sm_pixel_t pixel;
			pixel.mode = sm;

			std::vector<std::string> *parts = split(color, ",");
			if (parts->size() != 3)
				error_exit(false, "\"pixels-color\" requires 3 parameters, comman seperated");

			pixel.r = atoi(parts->at(0).c_str());
			pixel.g = atoi(parts->at(1).c_str());
			pixel.b = atoi(parts->at(2).c_str());

			delete parts;

			int thick = cfg_int(ae, "thick", "thickness of the marker", true, 2);

			std::string selection_bitmap = cfg_str(ae, "selection-bitmap", "bitmaps indicating which pixels to look at. must be same size as webcam image and must be a .pbm-file. leave empty to disable.", true, "");
			selection_mask *psm = selection_bitmap.empty() ? nullptr : new selection_mask(cfg->r, selection_bitmap);
			int noise_level = cfg_int(ae, "noise-factor", "at what difference levell is the pixel considered to be changed", true, 32);

			double pixels_changed_perctange = i ? -1.0 : cfg_float(ae, "pixels-changed-percentage", "what %% of pixels need to be changed before the marker is drawn", false, 1.0);
			bool remember_motion = cfg_bool(ae, "remember-trigger", "store movement bitmap in $motion-box$, to be used elsewhere", true, false);

			filters -> push_back(new filter_marker_simple(i, pixel, st, psm, noise_level, pixels_changed_perctange, thick, remember_motion));
		}
		else if (s_type == "apply-mask") {
			std::string selection_bitmap = cfg_str(ae, "selection-bitmap", "bitmaps indicating which pixels to mask. must be same size as webcam image and must be a .png or .pbm-file.", false, "");
			bool soft_mask = cfg_bool(ae, "soft-mask", "soft mask", true, false);

			selection_mask *sm = selection_bitmap.empty() ? nullptr : new selection_mask(cfg->r, selection_bitmap);

			filters -> push_back(new filter_apply_mask(sm, soft_mask));
		}
		else if (s_type == "motion-only") {
			std::string selection_bitmap = cfg_str(ae, "selection-bitmap", "bitmaps indicating which pixels to look at. must be same size as webcam image and must be a .pbm-file. leave empty to disable.", true, "");
			selection_mask *sm = selection_bitmap.empty() ? nullptr : new selection_mask(cfg->r, selection_bitmap);
			int noise_level = cfg_int(ae, "noise-factor", "at what difference levell is the pixel considered to be changed", true, 32);
			bool diff_only = cfg_bool(ae, "diff-only", "show difference in pixel value, not original picture", true, false);

			filters -> push_back(new filter_motion_only(sm, noise_level, diff_only));
		}
		else if (s_type == "boost-contrast")
			filters -> push_back(new filter_boost_contrast());
		else if (s_type == "overlay") {
			std::string s_pic = cfg_str(ae, "picture", "PNG to overlay (with alpha channel!)", false, "");

			pos_t pos = find_xy_or_pos(ae, "static overlay");

			filters -> push_back(new filter_overlay(s_pic, pos));
		}
#if HAVE_IMAGICK == 1
		else if (s_type == "gif-overlay") {
			std::string s_pic = cfg_str(ae, "picture", "(animated) GIF/MNG to overlay", false, "");

			pos_t pos = find_xy_or_pos(ae, "animated overlay");

			filters -> push_back(new filter_anigif_overlay(s_pic, pos));
		}
#endif
		else if (s_type == "text") {
			std::string s_text = cfg_str(ae, "text", "what text to show", false, "");
			std::string s_position = cfg_str(ae, "position", "where to put it, e.g. upper-left, lower-center, center-right, etc", false, "");

			pos_t pos = pos_to_pos(s_position);

			filters -> push_back(new filter_add_text(s_text, pos));
		}
		else if (s_type == "lcdproc") {
			std::string listen_adapter = cfg_str(ae, "listen-adapter", "network interface to listen on or 0.0.0.0 or ::1 for all", true, "0.0.0.0");
			std::string font = cfg_str(ae, "font", "which font to use (complete filename of a .ttf font-file)", false, "");
			int x = cfg_int(ae, "x", "x-coordinate of window", false, 0);
			int y = cfg_int(ae, "y", "y-coordinate of window", false, 0);
			int w = cfg_int(ae, "w", "width of window", false, 0);
			int h = cfg_int(ae, "h", "height of window", false, 0);
			int n_col = cfg_int(ae, "n-col", "number of characters in a row in the window", false, 0);
			int n_row = cfg_int(ae, "n-row", "number of rows of characters in the window", false, 0);
			int switch_interval = cfg_int(ae, "switch-interval", "when to switch between screens. in milliseconds.", true, 4000);
			bool invert = cfg_bool(ae, "invert", "invert colors", true, false);

			std::optional<rgb_t> col = get_color(ae);
			if (col.has_value() == false) {
				log(LL_WARNING, "No color set for \"lcdproc\", assuming black");
				col = { 0, 0, 0 };
			}

			auto bg_col = get_color(ae, "bg-");

			filters -> push_back(new filter_lcdproc(listen_adapter, font, x, y, w, h, bg_col, n_col, n_row, col.value(), switch_interval, invert));
		}
		else if (s_type == "scaled-text") {
			std::string s_text = cfg_str(ae, "text", "what text to show", false, "");
			std::string font = cfg_str(ae, "font", "which font to use (complete filename of a .ttf font-file)", false, "");
			int x = cfg_int(ae, "x", "x-coordinate of text", false, 0);
			int y = cfg_int(ae, "y", "y-coordinate of text", false, 0);
			int fs = cfg_int(ae, "font-size", "font size (in pixels)", false, 10);
			bool invert = cfg_bool(ae, "invert", "invert colors", true, false);

			std::optional<rgb_t> col = get_color(ae);
			if (col.has_value() == false) {
				log(LL_WARNING, "No color set for \"scaled-text\", assuming black");
				col = { 0, 0, 0 };
			}

			auto bg_col = get_color(ae, "bg-");

			filters -> push_back(new filter_add_scaled_text(s_text, font, x, y, fs, bg_col, col.value(), invert));
		}
		else if (s_type == "average") {
			int average_n = cfg_int(ae, "average-n", "how many frames to average", false, 3);

			filters -> push_back(new filter_average(average_n));
		}
		else if (s_type == "median") {
			int median_n = cfg_int(ae, "median-n", "how many frames to find the median of", false, 3);

			filters -> push_back(new filter_median(median_n));
		}
		else if (s_type == "exec-scroll") {
			std::string exec_what = cfg_str(ae, "exec-what", "what executable/script to invoke", false, "");
			std::string font = cfg_str(ae, "font", "which font-file to use (complete filename of a .ttf font-file)", false, "");
			int x = cfg_int(ae, "x", "x-coordinate of text", false, 0);
			int w = cfg_int(ae, "w", "width of text", false, -1);
			int y = cfg_int(ae, "y", "y-coordinate of text", false, 0);
			int fs = cfg_int(ae, "font-size", "font size (in pixels)", false, 10);
			int n_lines = cfg_int(ae, "n-lines", "show how many lines of text", false, 3);
			bool h_s = cfg_bool(ae, "horizontal", "scroll horizontally", true, false);
			int scroll_speed_h = cfg_int(ae, "scroll-speed-h", "how fast to scroll horizontally, in pixels per iteration", true, 1);
			bool invert = cfg_bool(ae, "invert", "invert colors", true, false);

			std::optional<rgb_t> col = get_color(ae);
			if (col.has_value() == false) {
				log(LL_WARNING, "No color set for \"exec-scroll\", assuming black");
				col = { 0, 0, 0 };
			}

			auto bg_col = get_color(ae, "bg-");

			filters -> push_back(new filter_scroll(font, x, y, w, n_lines, fs, exec_what, h_s, bg_col, scroll_speed_h, col.value(), invert));
		}
		else if (s_type == "draw") {
			int x = cfg_int(ae, "x", "x-coordinate of box", false, 0);
			int y = cfg_int(ae, "y", "y-coordinate of box", false, 0);
			int w = cfg_int(ae, "w", "width of box", false, 0);
			int h = cfg_int(ae, "h", "height of box", false, 0);

			std::optional<rgb_t> col = get_color(ae);
			if (col.has_value() == false) {
				log(LL_WARNING, "No color set for \"draw\", assuming black");
				col = { 0, 0, 0 };
			}

			filters -> push_back(new filter_draw(x, y, w, h, col.value()));
		}
		else if (s_type == "chroma-key") {
			std::string inst_name = cfg_str(ae, "replace-by", "replace green by pixels from 'replace-by'-source", false, "");
			instance *inst = find_instance_by_name(cfg, inst_name);
			if (!inst)
				error_exit(false, "Instance \"%s\" is not known", inst_name.c_str());

			source *cks = find_source(inst);
			if (!inst)
				error_exit(false, "Instance \"%s\" has no (video-)source", inst_name.c_str());
	
			filters->push_back(new filter_chromakey(cks, cfg->r));
		}
		else if (s_type == "paste") {
			std::string which = cfg_str(ae, "which", "bitmap to put, e.g. \"$motion-box$\" from \"marker\"", false, "");
			int x = cfg_int(ae, "x", "x-coordinate to put bitmap", false, 0);
			int y = cfg_int(ae, "y", "y-coordinate to put bitmap", false, 0);

			filters->push_back(new filter_add_bitmap(which, x, y));
		}
		else if (s_type == "copy") {
			std::string remember_as = cfg_str(ae, "remember-as", "under what name to remember this fragment", false, "");
			int x = cfg_int(ae, "x", "x-coordinate to copy from", false, 0);
			int y = cfg_int(ae, "y", "y-coordinate to copy from", false, 0);
			int w = cfg_int(ae, "w", "width", false, 1);
			int h = cfg_int(ae, "h", "height", false, 1);

			filters->push_back(new filter_copy(remember_as, x, y, w, h));
		}
		else if (s_type == "despeckle") {
			std::string pattern = cfg_str(ae, "pattern", "despeckle pattern. d/D for dilate (5/9), e/E for erodee (5/9)", false, "");

			filters->push_back(new filter_despeckle(pattern));
		}
		else {
			error_exit(false, "Filter %s is not known", s_type.c_str());
		}
	}

	return filters;
}

source * load_source(configuration_t *const cfg, const Setting & o_source, const int loglevel_in)
{
	source *s = nullptr;

	try
	{
		const std::string id = cfg_str(o_source, "id", "some identifier: used for selecting this module", true, "");
		log(LL_INFO, "Configuring the video-source %s...", id.c_str());

		const std::string s_type = cfg_str(o_source, "type", "source-type", false, "");
		const std::string descr = cfg_str(o_source, "descr", "description: visible in e.g. the http server", true, "");

		double max_fps = cfg_float(o_source, "max-fps", "limit the number of frames per second acquired to this value or -1.0 to disable", true, -1.0);
		if (max_fps == 0)
			error_exit(false, "Video-source: max-fps must be either > 0 or -1.0. Use -1.0 for no FPS limit.");

		int resize_w = cfg_int(o_source, "resize-width", "resize picture width to this (-1 to disable)", true, -1);
		int resize_h = cfg_int(o_source, "resize-height", "resize picture height to this (-1 to disable)", true, -1);

		double timeout = cfg_float(o_source, "timeout", "how long to wait for the camera before considering it be offline (in seconds)", true, 1.0);

		std::string exec_failure = cfg_str(o_source, "exec-failure", "script to start when the camera/video-source fails", true, "");

		int jpeg_quality = cfg_int(o_source, "quality", "JPEG quality, this influences the size", true, 75);

		int loglevel = to_loglevel(cfg_str(o_source, "log-level", "log-level for this source", true, ll_to_str(loglevel_in)));
		if (loglevel == -1)
			return nullptr;

		bool use_controls = cfg_bool(o_source, "enable-controls", "enable controling of contrast/saturation/etc in the user interface. emulates a software implementation (relatively slow!) if the hardware cannot it not by itself", true, false);

		bool on_demand = cfg_bool(o_source, "on-demand", "start/stop this \"source\" depending on the demand. e.g. connections to ip-cameras will be stopped when no-one is viewing", true, false);

		failure_t failure = load_failure(o_source);

#if ALSA_FOUND == 1
		audio *a { nullptr };

		try {
			const Setting & audio_settings = o_source["audio-settings"];
			const std::string audio_type = cfg_str(audio_settings, "type", "input type, currently only \"alsa\" is available", true, "alsa");
			if (audio_type != "alsa")
				error_exit(false, "audio-source type \"%s\" is not supported", audio_type.c_str());
			const std::string device = cfg_str(audio_settings, "device", "e.g. hw:1,0", false, "");
			const int samplerate = cfg_int(audio_settings, "samplerate", "sample rate, eh 44100", false, 350);

			a = new audio_alsa(device, samplerate);
		}
		catch(SettingNotFoundException & snfe) {
		}
#endif

		std::vector<filter *> *source_filters = nullptr;
		try {
			const Setting & f = o_source.lookup("filters");
			source_filters = load_filters(cfg, f);
		}
		catch(SettingNotFoundException & snfe) {
		}

#if HAVE_LIBV4L2 == 1
		if (s_type == "v4l") {
			int w = cfg_int(o_source, "width", "width of picture", false);
			int h = cfg_int(o_source, "height", "height of picture", false);
			std::string dev = cfg_str(o_source, "device", "linux v4l2 device", false, "/dev/video0");
			bool prefer_jpeg = cfg_bool(o_source, "prefer-jpeg", "try to get directly JPEG from camera", true, false);

			s = new source_v4l(id, descr, exec_failure, dev, jpeg_quality, max_fps, w, h, cfg->r, resize_w, resize_h, loglevel, timeout, source_filters, failure, prefer_jpeg, use_controls);
		}
		else
#endif
#if HAVE_LIBCAMERA == 1
		if (s_type == "libcamera") {
			int w = cfg_int(o_source, "width", "width of picture", false);
			int h = cfg_int(o_source, "height", "height of picture", false);
			std::string dev = cfg_str(o_source, "device", "device name", false, "");
			bool prefer_jpeg = cfg_bool(o_source, "prefer-jpeg", "try to get directly JPEG from camera", true, false);

			std::map<std::string, parameter *> ctrls;

			try {
				const Setting & ctls = o_source.lookup("controls");

				size_t n_ctls = ctls.getLength();
				for(size_t i=0; i<n_ctls; i++) {
					const Setting & ctl = ctls[i];

					std::string name = cfg_str(ctl, "name", "control name", false, "");
					std::string value = cfg_str(ctl, "data", "control value", false, "");

					parameter::add_value(ctrls, str_tolower(name), "control " + name, value);
				}

			}
			catch(SettingNotFoundException & snfe) {
			}

			s = new source_libcamera(id, descr, exec_failure, dev, jpeg_quality, max_fps, w, h, cfg->r, resize_w, resize_h, loglevel, timeout, source_filters, failure, prefer_jpeg, ctrls, use_controls ? new controls_software() : nullptr);
		}
		else
#endif
		if (s_type == "jpeg") {
			bool ign_cert = cfg_bool(o_source, "ignore-cert", "ignore SSL errors", true, false);
			const std::string auth = cfg_str(o_source, "http-auth", "HTTP authentication string", true, "");
			const std::string url = cfg_str(o_source, "url", "address of JPEG stream", false, "");

			s = new source_http_jpeg(id, descr, exec_failure, url, ign_cert, auth, max_fps, cfg->r, resize_w, resize_h, loglevel, timeout, source_filters, failure, use_controls ? new controls_software() : nullptr, jpeg_quality);
		}
		else if (s_type == "jpeg-file") {
			const std::string path = cfg_str(o_source, "path", "path + file name of JPEG file to monitor", false, "");

			s = new source_filesystem_jpeg(id, descr, exec_failure, path, max_fps, cfg->r, resize_w, resize_h, loglevel, source_filters, failure, use_controls ? new controls_software() : nullptr, jpeg_quality);
		}
		else if (s_type == "broken-mjpeg") {
			const std::string url = cfg_str(o_source, "url", "address of MJPEG stream", false, "");
			bool ign_cert = cfg_bool(o_source, "ignore-cert", "ignore SSL errors", true, false);

			s = new source_http_mjpeg(id, descr, exec_failure, url, ign_cert, max_fps, cfg->r, resize_w, resize_h, loglevel, timeout, source_filters, failure, use_controls ? new controls_software() : nullptr, jpeg_quality);
		}

#if HAVE_FFMPEG == 1
		else if (s_type == "rtsp" || s_type == "mjpeg" || s_type == "stream" || s_type == "ffmpeg") {
			const std::string url = cfg_str(o_source, "url", "address of video stream", false, "");
			bool tcp = cfg_bool(o_source, "tcp", "use TCP for RTSP transport (instead of default UDP)", true, false);

			s = new source_ffmpeg(id, descr, exec_failure, url, tcp, max_fps, cfg->r, resize_w, resize_h, loglevel, timeout, source_filters, failure, use_controls ? new controls_software() : nullptr, jpeg_quality);
		}
#endif
		else if (s_type == "plugin") {
			std::string plugin_bin = cfg_str(o_source, "source-plugin-file", "filename of video data source plugin", true, "");
			std::string plugin_arg = cfg_str(o_source, "source-plugin-parameter", "parameter for video data source plugin", true, "");

			s = new source_plugin(id, descr, exec_failure, plugin_bin, plugin_arg, max_fps, cfg->r, resize_w, resize_h, loglevel, timeout, source_filters, failure, use_controls ? new controls_software() : nullptr, jpeg_quality);
		}
		else if (s_type == "delay") {
			int n_frames = cfg_int(o_source, "n-frames", "how many frames in the past", false, 1);
			std::string delayed_source = cfg_str(o_source, "delayed-source", "source to delay", false, "");

			source *s_s = (source *)find_interface_by_id(cfg, delayed_source);
			if (!s_s)
				error_exit(false, "Source \"%s\" is not known", delayed_source.c_str());

			s = new source_delay(id, descr, exec_failure, s_s, jpeg_quality, n_frames, max_fps, cfg->r, resize_w, resize_h, loglevel, timeout, source_filters, failure, use_controls ? new controls_software() : nullptr);
		}
#if HAVE_GSTREAMER == 1
		else if (s_type == "gstreamer") {
			std::string pipeline = cfg_str(o_source, "pipeline", "gstreamer pipeline. Note: it should end with \" ! appsink name=constatus\"!", false, "");

			s = new source_gstreamer(id, descr, exec_failure, pipeline, cfg->r, resize_w, resize_h, loglevel, timeout, source_filters, failure, use_controls ? new controls_software() : nullptr, jpeg_quality);
		}
#endif
		else if (s_type == "pixelflood") {
			std::string listen_adapter = cfg_str(o_source, "listen-adapter", "network interface to listen on or 0.0.0.0 or ::1 for all", true, "0.0.0.0");
			int listen_port = cfg_int(o_source, "listen-port", "Port to listen on", false, 5004);
			int pixel_size = cfg_int(o_source, "pixel-size", "Pixel-size", true, 1);
			std::string pp_str = cfg_str(o_source, "pp", "Pixelflut format: tcp-txt or udp-bin", false, "tcp-txt");
			int w = cfg_int(o_source, "width", "width of picture", false);
			int h = cfg_int(o_source, "height", "height of picture", false);

			pixelflood_protocol_t pp = cfg_convert_pp(pp_str);
			bool dgram = pp == PP_UDP_BIN;

			s = new source_pixelflood(id, descr, exec_failure, { listen_adapter, listen_port, SOMAXCONN, dgram }, pixel_size, pp, max_fps, w, h, loglevel, source_filters, failure, use_controls ? new controls_software() : nullptr, jpeg_quality);
		}
		else if (s_type == "static") {
			int w = cfg_int(o_source, "width", "width of picture", false);
			int h = cfg_int(o_source, "height", "height of picture", false);

			s = new source_static(id, descr, w, h, use_controls ? new controls_software() : nullptr, jpeg_quality);

			delete source_filters;
		}
		else if (s_type == "black") {
			int w = cfg_int(o_source, "width", "width of picture", false);
			int h = cfg_int(o_source, "height", "height of picture", false);

			s = new source_black(id, descr, w, h, use_controls ? new controls_software() : nullptr);

			delete source_filters;
		}
		else {
			error_exit(false, "Source-type \"%s\" is not known", s_type.c_str());
		}

		double watchdog_timeout = cfg_float(o_source, "watchdog-timeout", "when the \"source\" stops streaming for the number of seconds given here, it is restarted", true, -1.0);
		if (watchdog_timeout > 0.)
			s->start_watchdog(watchdog_timeout);

		if (on_demand)
			s->set_on_demand(on_demand);

#if ALSA_FOUND == 1
		s->set_audio(a);
#endif
	}
	catch(SettingException & se) {
		error_exit(false, "Error in %s: %s", se.getPath(), se.what());
	}

	return s;
}

stream_plugin_t * load_stream_plugin(const Setting & in)
{
	std::string file = cfg_str(in, "stream-writer-plugin-file", "filename of stream writer plugin", false, "");
	if (file.empty())
		return nullptr;

	stream_plugin_t *sp = new stream_plugin_t;

	sp -> par = cfg_str(in, "stream-writer-plugin-parameter", "parameter for stream writer plugin", true, "");

	void *library = dlopen(file.c_str(), RTLD_NOW);
	if (!library)
		error_exit(true, "Failed opening stream writer library %s (%s)", file.c_str(), dlerror());

	sp -> init_plugin = (tp_init_plugin_t)find_symbol(library, "init_plugin", "stream writer plugin", file.c_str());
	sp -> open_file = (open_file_t)find_symbol(library, "open_file", "stream writer plugin", file.c_str());
	sp -> write_frame = (write_frame_t)find_symbol(library, "write_frame", "stream writer plugin", file.c_str());
	sp -> close_file = (close_file_t)find_symbol(library, "close_file", "stream writer plugin", file.c_str());
	sp -> uninit_plugin = (tp_uninit_plugin_t)find_symbol(library, "uninit_plugin", "stream writer plugin", file.c_str());

	return sp;
}

void interval_fps_error(const char *const name, const char *what, const char *id)
{
	error_exit(false, "Interval/%s %s not set or invalid (e.g. 0) for target (%s). Make sure that you use a float-value for the fps/interval, e.g. 13.0 instead of 13", name, what, id);
}

schedule *load_scheduler(const Setting & sched_obj)
{
	size_t n_sched = sched_obj.getLength();

	if (n_sched == 0) {
		log(LL_WARNING, "schedule is empty: is that correct?");
		return nullptr;
	}

	std::vector<std::string> sched_vector;

	for(size_t i=0; i<n_sched; i++)
		sched_vector.push_back(sched_obj[i]);

	return new schedule(sched_vector);
}

target * load_target(const Setting & in, source *const s, meta *const m, configuration_t *const cfg)
{
	target *t = nullptr;

	const std::string id = cfg_str(in, "id", "some identifier: used for selecting this module", true, "");
	const std::string descr = cfg_str(in, "descr", "description: visible in e.g. the http server", true, "");

	log(LL_INFO, "Loading target %s", id.c_str());

#if HAVE_GSTREAMER == 1
	std::string format = cfg_str(in, "format", "avi, extpipe, ffmpeg (for mp4, ogg, etc), jpeg, vnc, plugin, as-a-new-source or pixelflood", false, "");
#else
	std::string format = cfg_str(in, "format", "extpipe, ffmpeg (for mp4, ogg, etc), jpeg, vnc, plugin, as-a-new-source or pixelflood", false, "");
#endif

	std::string path = cfg_str(in, "path", "directory to write to", format == "as-a-new-source" || format == "gstreamer" || format == "pixelflood", "");
	std::string prefix = cfg_str(in, "prefix", "string to begin filename with", true, "");
	std::string fmt = cfg_str(in, "fmt", "specifies filename format", true, default_fmt);

	std::string exec_start = cfg_str(in, "exec-start", "script to start when recording starts", true, "");
	std::string exec_cycle = cfg_str(in, "exec-cycle", "script to start when the record file is restarted", true, "");
	std::string exec_end = cfg_str(in, "exec-end", "script to start when the recording stops", true, "");

	int restart_interval = cfg_int(in, "restart-interval", "after how many seconds should the stream-file be restarted", true, -1);

	int jpeg_quality = cfg_int(in, "quality", "JPEG quality, this influences the size", true, 75);

	bool handle_failure = cfg_bool(in, "handle-failure", "handle failure or not (see source definition)", true, true);

	double fps = 0, interval = 0;
	if (!find_interval_or_fps(in, &interval, "fps", &fps))
		interval_fps_error("fps", "for writing frames to disk (FPS as the video frames are retrieved)", id.c_str());

	double override_fps = cfg_float(in, "override-fps", "What FPS to use in the output file. That way you can let the resulting file be played faster (or slower) than how the stream is obtained from the camera. -1.0 to disable", true, -1.0);

	std::vector<filter *> *filters = nullptr;
	try {
		const Setting & f = in.lookup("filters");
		filters = load_filters(cfg, f);
	}
	catch(SettingNotFoundException & snfe) {
	}

	schedule *sched = nullptr;

	try {
		sched = load_scheduler(in.lookup("schedule"));
	}
	catch(SettingNotFoundException & snfe) {
	}

#if HAVE_GSTREAMER == 1
	if (format == "avi") {
		t = new target_avi(id, descr, s, path, prefix, fmt, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end, override_fps, cfg, false, handle_failure, sched);
	}
	else
#endif
	if (format == "extpipe") {
		std::string cmd = cfg_str(in, "cmd", "Command to send the frames to", false, "");

		t = new target_extpipe(id, descr, s, path, prefix, fmt, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end, cmd, cfg, false, handle_failure, sched);
	}
#if HAVE_FFMPEG == 1
	else if (format == "ffmpeg") {
		int bitrate = cfg_int(in, "bitrate", "How many bits per second to emit. For 352x288 200000 is a sane value. This value affects the quality.", true, 200000);
		std::string type = cfg_str(in, "ffmpeg-type", "E.g. flv, mp4", true, "mp4");
		std::string const parameters = cfg_str(in, "ffmpeg-parameters", "Parameters specific for ffmpeg.", true, "");

		t = new target_ffmpeg(id, descr, parameters, s, path, prefix, fmt, restart_interval, interval, type, bitrate, filters, exec_start, exec_cycle, exec_end, override_fps, cfg, false, handle_failure, sched);
	}
#endif
	else if (format == "jpeg")
		t = new target_jpeg(id, descr, s, path, prefix, fmt, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end, cfg, false, handle_failure, sched);
	else if (format == "plugin") {
		stream_plugin_t *sp = load_stream_plugin(in);

		t = new target_plugin(id, descr, s, path, prefix, fmt, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end, sp, override_fps, cfg, false, handle_failure, sched);
	}
	else if (format == "vnc") {
		std::string listen_adapter = cfg_str(in, "listen-adapter", "network interface to listen on or 0.0.0.0 or ::1 for all", false, "");
		int listen_port = cfg_int(in, "listen-port", "port to listen on", false, 5901);

		t = new target_vnc(id, descr, s, { listen_adapter, listen_port, SOMAXCONN, false }, restart_interval, interval, filters, exec_start, exec_end, cfg, false, handle_failure, sched);
	}
	else if (format == "pixelflood") {
		std::string host = cfg_str(in, "host", "IPv4 address to send to", false, "");
		int port = cfg_int(in, "port", "port to send to", false, 5004);
		int pfw = cfg_int(in, "pf-w", "width of pixelflood target", true, 128);
		int pfh = cfg_int(in, "pf-h", "height of pixelflood target", true, 16);
		int quality = 100;
		std::string pp_str = cfg_str(in, "pp", "Pixelflut format: tcp-txt or udp-bin", false, "tcp-txt");
		int xoff = cfg_int(in, "x-off", "x-offset in pixelflood target", true, 0);
		int yoff = cfg_int(in, "y-off", "y-offset in pixelflood target", true, 0);

		pixelflood_protocol_t pp = cfg_convert_pp(pp_str);

		t = new target_pixelflood(id, descr, s, interval, filters, -1, cfg, host, port, pfw, pfh, quality, pp, xoff, yoff, handle_failure, sched);
	}
#if HAVE_GSTREAMER == 1
	else if (format == "gstreamer") {
		std::string pipeline = cfg_str(in, "pipeline", "gstreamer pipeline. Note: it should start with \"appsrc name=constatus ! \"", false, "");

		t = new target_gstreamer(id, descr, s, pipeline, interval, filters, cfg, sched);
	}
#endif
	else if (format == "as-a-new-source") {
		const std::string new_id = cfg_str(in, "new-id", "some identifier: used for selecting this module", false, "");
		const std::string new_descr = cfg_str(in, "new-descr", "description: visible in e.g. the http server", false, "");

		t = new target_new_source(id, descr, s, interval, filters, cfg, new_id, new_descr, sched);

		instance *new_source_instance = new instance;
		new_source_instance->name = myformat("%s to %s", id.c_str(), new_id.c_str());
		new_source_instance->interfaces.push_back(((target_new_source *)t) -> get_new_source());

		cfg->instances.push_back(new_source_instance);
	}
	else {
		error_exit(false, "Format %s is unknown (stream to disk backends)", format.c_str());
	}

	return t;
}

std::vector<interface *> load_motion_triggers(configuration_t *const cfg, const Setting &o_mt, source *const s, instance *const ci)
{
	std::vector<interface *> out;

	size_t n_vlb = o_mt.getLength();

	log(LL_DEBUG, " %zu motion trigger(s)", n_vlb);

	if (n_vlb == 0)
		log(LL_INFO, " 0 triggers, is that correct?");

	for(size_t i=0; i<n_vlb; i++) {
		const Setting &trigger = o_mt[i];

		const std::string id = cfg_str(trigger, "id", "some identifier: used for selecting this module", true, "");
		const std::string descr = cfg_str(trigger, "descr", "description: visible in e.g. the http server", true, "");

		schedule *sched = nullptr;

		try {
			sched = load_scheduler(trigger.lookup("schedule"));
		}
		catch(SettingNotFoundException & snfe) {
		}

		std::map<std::string, parameter *> dp; // detection parameters

		/////////////
		std::vector<target *> *motion_targets = new std::vector<target *>();

		try {
			const Setting & t = trigger.lookup("targets");

			size_t n_t = t.getLength();
			log(LL_DEBUG, " %zu motion trigger target(s)", n_t);

			if (n_t == 0)
				log(LL_WARNING, " 0 trigger targets, is that correct?");

			for(size_t i=0; i<n_t; i++) {
				const Setting & ct = t[i];

				motion_targets -> push_back(load_target(ct, s, s -> get_meta(), cfg));
			}
		}
		catch(SettingNotFoundException & snfe) {
		}
		//////////

		std::string motion_source = cfg_str(trigger, "motion-source", "an other source to monitor, leave empty for monitoring by itself", true, "");
		instance *const lr_motion_trigger = motion_source.empty() ? nullptr : (instance *)find_instance_by_name(cfg, motion_source);

		interface *m = nullptr;

		if (lr_motion_trigger) {
			log(LL_INFO, "this trigger will listen to events caught by \"%s\"", lr_motion_trigger->name.c_str());

			cfg_int(trigger, "pre-motion-record-duration", "how many frames to record that happened before the motion started", true, 0, &dp);

			m = new motion_trigger_other_source(id, descr, s, lr_motion_trigger, motion_targets, dp, sched);
		}
		else {
			cfg_int(trigger, "noise-factor", "at what difference levell is the pixel considered to be changed", true, 32, &dp);
			cfg_float(trigger, "min-pixels-changed-percentage", "minimum percentage (%%) of pixels need to be changed before the motion trigger is triggered", true, 1.0, &dp);
			cfg_float(trigger, "max-pixels-changed-percentage", "maximum percentage (%%) of pixels need to be changed before the motion trigger is triggered", true, 100.0, &dp);

			cfg_bool(trigger, "pan-tilt", "pan/tilt the camera, depending on where the movement takes place", true, false, &dp);

			cfg_str(trigger, "remember-trigger", "copy first frame with motion in this copy/paste buffer", true, "", &dp);

			cfg_str(trigger, "despeckle-filter", "filter to apply before detecting motion, see example in constatus.cfg from the source distribution", true, "", &dp);

			cfg_int(trigger, "min-duration", "minimum number of frames to record", true, 5, &dp);
			cfg_int(trigger, "mute-duration", "how long not to record (in frames) after motion has stopped", true, 5, &dp);
			cfg_int(trigger, "min-n-frames", "how many frames should have motion before recording starts", true, 1, &dp);
			cfg_int(trigger, "pre-motion-record-duration", "how many frames to record that happened before the motion started", true, 10, &dp);
			int warmup_duration = cfg_int(trigger, "warmup-duration", "how many frames to ignore so that the camera can warm-up", true, 10);

			if (cfg_float(trigger, "max-fps", "maximum number of frames per second to analyze (or -1.0 for no limit)", true, -1.0, &dp) == 0)
				error_exit(false, "Motion triggers: max-fps must be either > 0 or -1.0. Use -1.0 for no FPS limit.");

			std::string selection_bitmap = cfg_str(trigger, "selection-bitmap", "bitmaps indicating which pixels to look at. must be same size as webcam image and must be a .pbm-file. leave empty to disable.", true, "");
			selection_mask *sm = selection_bitmap.empty() ? nullptr : new selection_mask(cfg->r, selection_bitmap);

			std::vector<filter *> *filters_detection = nullptr;
			try {
				const Setting & f = trigger.lookup("filters-detection");
				filters_detection = load_filters(cfg, f);
			}
			catch(SettingNotFoundException & snfe) {
			}

			std::string file = cfg_str(trigger, "trigger-plugin-file", "filename of motion detection plugin", true, "");
			ext_trigger_t *et = nullptr;
			if (!file.empty()) {
				et = new ext_trigger_t;
				et -> par = cfg_str(trigger, "trigger-plugin-parameter", "parameter for motion detection plugin", true, "");

				et -> library = dlopen(file.c_str(), RTLD_NOW);
				if (!et -> library)
					error_exit(true, "Failed opening motion detection plugin library %s (%s)", file.c_str(), dlerror());

				et -> init_motion_trigger = (init_motion_trigger_t)find_symbol(et -> library, "init_motion_trigger", "motion detection plugin", file.c_str());
				et -> detect_motion = (detect_motion_t)find_symbol(et -> library, "detect_motion", "motion detection plugin", file.c_str());
				et -> uninit_motion_trigger = (uninit_motion_trigger_t)find_symbol(et -> library, "uninit_motion_trigger", "motion detection plugin", file.c_str());
			}

			std::string exec_start = cfg_str(trigger, "exec-start", "script to start when recording starts", true, "");
			std::string exec_end = cfg_str(trigger, "exec-end", "script to start when the recording stops", true, "");

			m = new motion_trigger_generic(id, descr, s, warmup_duration, filters_detection, motion_targets, sm, et, exec_start, exec_end, ci, dp, sched);
		}

		out.push_back(m);
	}

	return out;
}

std::vector<interface *> load_audio_triggers(const Setting &o_audio, instance *const i, source *const s)
{
	std::vector<interface *> out;

	std::string id = cfg_str(o_audio, "id", "some identifier: used for selecting this module", true, "");
	std::string descr = cfg_str(o_audio, "descr", "description: visible in e.g. the http server", true, "");

	int threshold = cfg_int(o_audio, "threshold", "threshold for when a trigger is counted", false, 350);
	int min_n_triggers = cfg_int(o_audio, "min-n-triggers", "number of triggers to trigger the recording", false, 25);

	std::string targets = cfg_str(o_audio, "trigger-ids", "comma seperated list of video motion triggers to trigger", false, "");
	std::vector<std::string> *tgts = split(targets, ",");

	audio_trigger *at = new audio_trigger(id, descr, i, s, threshold, min_n_triggers, *tgts);

	delete tgts;

	out.push_back(at);

	return out;
}

std::pair<source *, std::vector<target *> *> load_view(configuration_t *const cfg, const Setting & in, instance *const inst)
{
	const std::string id = cfg_str(in, "id", "some identifier: used for selecting this module", true, "");
	const std::string descr = cfg_str(in, "descr", "description: visible in e.g. the http server", true, "");

	const std::string type = cfg_str(in, "type", "type: html-grid", false, "html-grid");

	std::vector<view_src_t> ids;
	const Setting & s_ids = in.lookup("sources");

	for(int i=0; i<s_ids.getLength(); i++) {
		const Setting & src = s_ids[i];

		std::string id = cfg_str(src, "id", "ID of source", false, "");

		std::string p = cfg_str(src, "position", "where to put the source", true, "none");
		pos_t pos = pos_to_pos(p);

		ids.push_back({ id, pos });
	}

	int width = cfg_int(in, "width", "width of the total view", true, -1);
	int height = cfg_int(in, "height", "height of the total view", true, -1);

	int gwidth = cfg_int(in, "grid-width", "number of columns", true, -1);
	int gheight = cfg_int(in, "grid-height", "number of rows", true, -1);

	double interval = cfg_float(in, "switch-interval", "when to switch source", true, 5.0);

	int jpeg_quality = cfg_int(in, "quality", "JPEG quality, this influences the size", true, 75);

	view *v = nullptr;
	std::vector<target *> *targets = nullptr;

	if (type == "html-grid")
		v = new view_html_grid(cfg, id, descr, width, height, ids, gwidth, gheight, interval, jpeg_quality);
	else if (type == "source-switch" || type == "all" || type == "pip" || type == "3d" || type == "3D") {
		targets = new std::vector<target *>();

		std::vector<filter *> *filters = nullptr;
		try {
			const Setting & f = in.lookup("filters");
			filters = load_filters(cfg, f);
		}
		catch(SettingNotFoundException & snfe) {
		}

		if (type == "source-switch") {
			bool auto_shrink = cfg_bool(in, "auto-shrink", "automatically resize when source-size does not match selected w/h of view", true, true);

			v = new view_ss(cfg, id, descr, width, height, auto_shrink, ids, interval, filters, jpeg_quality);
		}
		else if (type == "all") {
			if (width == -1 || height == -1)
				error_exit(false, "view of type \"all\" must have a valid width/height, not -1");

			if (gwidth == -1 || gheight == -1)
				error_exit(false, "view of type \"all\" must have a valid grid-width/grid-height, not -1");

			v = new view_all(cfg, id, descr, width, height, gwidth, gheight, ids, interval, filters, jpeg_quality);
		}
		else if (type == "pip") {
			int perc = cfg_int(in, "percentage", "resize (shrink) percentage", false, 25);

			v = new view_pip(cfg, id, descr, width, height, ids, filters, perc, jpeg_quality);
		}
		else if (type == "3d" || type == "3D") {
			const std::string packing = cfg_str(in, "packing", "how to combine the left & right picture", false, "side-by-side");

			view_3d_mode_t v3m = view_3d_sidebyside;
			if (packing == "side-by-side")
				v3m = view_3d_sidebyside;
			else if (packing == "top-bottom")
				v3m = view_3d_topbottom;
			else if (packing == "alternating-lines")
				v3m = view_3d_lines;
			else if (packing == "alternating-columns")
				v3m = view_3d_columns;
			else if (packing == "checkerboard")
				v3m = view_3d_checkerboard;
			else if (packing == "frame-sequence")
				v3m = view_3d_framesequence;
			else if (packing == "red-green")
				v3m = view_3d_redgreen;
			else if (packing == "red-blue")
				v3m = view_3d_redblue;
			else
				error_exit(false, "Packing method \"%s\" not known", packing.c_str());

			v = new view_3d(cfg, id, descr, -1, -1, ids, filters, v3m, jpeg_quality);
		}

		try {
			const Setting & t = in.lookup("targets");

			size_t n_t = t.getLength();
			log(LL_DEBUG, " %zu view target(s) for %s", n_t, id.c_str());

			for(size_t i=0; i<n_t; i++) {
				const Setting & ct = t[i];

				targets -> push_back(load_target(ct, v, nullptr, cfg));
			}
		}
		catch(SettingNotFoundException & snfe) {
		}
	}
	else if (type == "html-all") {
		ids.clear();

		for(instance * cur_inst : cfg -> instances) {
			source *s = find_source(cur_inst);
			ids.push_back({ s -> get_id(), none });
		}

		v = new view_html_all(cfg, id, descr, ids, jpeg_quality);
	}
	else
		error_exit(false, "Currently only \"html-grid\", \"source-switch\", \"view-all\" and \"html-all\" (for views, id %s) are supported", id.c_str());

	return { v, targets };
}

std::vector<interface *> load_guis(configuration_t *const cfg, const Setting & gs)
{
	std::vector<interface *> out;

#if HAVE_LIBSDL2 == 1
	size_t n_gl = gs.getLength();

	log(LL_DEBUG, " %zu GUI(s)", n_gl);

	if (n_gl == 0)
		log(LL_WARNING, " 0 GUIs, is that correct?");
	else if (SDL_Init(SDL_INIT_VIDEO) < 0)
		error_exit(false, "Failed to initialize SDL");

	for(size_t i=0; i<n_gl; i++) {
		const Setting &gui = gs[i];

		const std::string id = cfg_str(gui, "id", "some identifier: used for selecting this module", true, "");
		const std::string descr = cfg_str(gui, "descr", "description: visible in e.g. the http server", true, "");

		std::string src = cfg_str(gui, "source", "source to view", false, "");
		source *const s = (source *)find_interface_by_id(cfg, src);
		if (!s)
			error_exit(false, "Source(! not instance!) \"%s\" not found", src.c_str());

		bool handle_failure = cfg_bool(gui, "handle-failure", "handle failure or not (see source definition)", true, true);

		int target_w = cfg_int(gui, "target-width", "target picture width (size of window)", true, -1);
		int target_h = cfg_int(gui, "target-height", "target picture height (size of window)", true, -1);

		double fps = 0, interval = 0;
		if (!find_interval_or_fps(gui, &interval, "fps", &fps))
			interval_fps_error("fps", "for showing video frames", id.c_str());

		std::vector<filter *> *filters = nullptr;
		try {
			const Setting & f = gui.lookup("filters");
			filters = load_filters(cfg, f);
		}
		catch(SettingNotFoundException & snfe) {
		}

		interface *h = new gui_sdl(cfg, id, descr, s, fps, target_w, target_h, filters, handle_failure);

		out.push_back(h);
	}
#endif

	return out;
}

std::vector<interface *> load_net_announcers(configuration_t *const cfg, const Setting & ann)
{
	std::vector<interface *> out;

#if HAVE_RYGEL == 1
	size_t n_announce = ann.getLength();

	log(LL_DEBUG, " %zu announcers", n_announce);

	for(size_t i=0; i<n_announce; i++) {
		const Setting & announcers = ann[i];

		const std::string id = cfg_str(announcers, "id", "some identifier: used for selecting this module", true, "");
		const std::string descr = cfg_str(announcers, "descr", "description: visible in e.g. the http server", true, "");

		std::string http_id_list = cfg_str(announcers, "announce-ids", "comma seperated list of ID(s)s of HTTP server(s) to announce URLs from", false, "");

		std::string net_int_list = cfg_str(announcers, "network-interfaces", "comma seperated list of network interfaces to announce on", false, "");
		std::vector<std::string> *interfaces = split(net_int_list, ",");

		std::vector<std::string> *ids = split(http_id_list, ",");

		interface *h = new announce_upnp(cfg, id, descr, *ids, *interfaces);

		delete ids;

		delete interfaces;

		out.push_back(h);
	}
#endif

	return out;
}

std::vector<interface *> load_http_servers(configuration_t *const cfg, instance *const ci, const Setting & hs, const bool local_instance, source *const s, instance *const views, meta *const m)
{
	size_t n_hl = hs.getLength();

	log(LL_DEBUG, " %zu http server(s)", n_hl);

	std::vector<interface *> out;

	if (n_hl == 0 && ci)
		log(LL_WARNING, " 0 servers, is that correct?");

	for(size_t i=0; i<n_hl; i++) {
		const Setting &server = hs[i];

		const std::string id = cfg_str(server, "id", "some identifier: used for selecting this module", true, "");
		const std::string descr = cfg_str(server, "descr", "description: visible in e.g. the http server", true, "");

		std::string listen_adapter = cfg_str(server, "listen-adapter", "network interface to listen on or 0.0.0.0 or ::1 for all", false, "");
		int listen_port = cfg_int(server, "listen-port", "port to listen on", false, 8080);
		log(LL_INFO, " HTTP server listening on %s:%d", listen_adapter.c_str(), listen_port);

		std::string auth_method = cfg_str(server, "http-auth-method", "none, file or pam", true, "none");

		http_auth *auth = nullptr;
		if (auth_method == "file") {
			std::string auth_file = cfg_str(server, "http-auth-file", "file containing authentication data", true, "");
			auth = new http_auth(auth_file);
		}
#if HAVE_LIBPAM == 1
		else if (auth_method == "pam") {
			auth = new http_auth_pam();
		}
#endif
		else if (auth_method != "none") {
			error_exit(false, "Authentication method \"%s\" not understood", auth_method.c_str());
		}

		int jpeg_quality = cfg_int(server, "quality", "JPEG quality, this influences the size", true, 75);
		int time_limit = cfg_int(server, "time-limit", "how long (in seconds) to stream before the connection is closed", true, -1);
		bool handle_failure = cfg_bool(server, "handle-failure", "handle failure or not (see source definition)", true, true);

		int resize_w = cfg_int(server, "resize-width", "resize picture width to this (-1 to disable)", true, -1);
		int resize_h = cfg_int(server, "resize-height", "resize picture height to this (-1 to disable)", true, -1);

		bool motion_compatible = cfg_bool(server, "motion-compatible", "only stream MJPEG and do not wait for HTTP request", true, false);
		bool allow_admin = cfg_bool(server, "allow-admin", "when enabled, you can partially configure services", true, false);
		bool is_rest = cfg_bool(server, "is-rest", "when enabled then this is a REST (only!) interface", true, false);
		bool archive_access = cfg_bool(server, "archive-access", "when enabled, you can retrieve recorded video/images", true, false);

		std::string snapshot_dir = cfg_str(server, "snapshot-dir", "where to store snapshots (triggered by HTTP server). see \"allow-admin\".", true, "");
		bool with_subdirs = cfg_bool(server, "dir-with-subdirs", "when enabled, also the sub-directories of snapshot-dir are given access to", true, false);

		std::string stylesheet = cfg_str(server, "stylesheet", "what stylesheet to use (optional)", true, "");

		std::string motd = cfg_str(server, "motd", "file to read for a MOTD (message of the day) in the main-page", true, "");

		std::string key_file = cfg_str(server, "key-file", "key-file for https; setting this sets this server-instance to be an https instance", true, "");
		std::string cert_file = cfg_str(server, "certificate-file", "certificate-file for https; setting this sets this server-instance to be an https instance", true, "");

		std::vector<filter *> *http_filters = nullptr;
		try {
			const Setting & f = server.lookup("filters");
			http_filters = load_filters(cfg, f);
		}
		catch(SettingNotFoundException & snfe) {
		}

		double fps = 0, interval = 0;
		if (!find_interval_or_fps(server, &interval, "fps", &fps) && is_rest == false)
			interval_fps_error("fps", "for showing video frames", id.c_str());

		ssl_pars_t sp;
		bool ssl_enabled = !key_file.empty() || !cert_file.empty();

		if (ssl_enabled) {
			sp.key_file = key_file;
			sp.certificate_file = cert_file;
		}

#if HAVE_WEBSOCKETPP == 1
		int websocket_port = cfg_int(server, "websocket-port", "port for websocket operations (optional)", true, -1);
		std::string websocket_url = cfg_str(server, "websocket-url", "URL for websocket operations (optional) - e.g. for a behind-proxy use case", true, "");
		bool ws_privacy = cfg_bool(server, "websocket-privacy", "if set to true, IP addresses of other viewers are NOT sent to other viewers", true, true);
#else
		int websocket_port = -1;
		std::string websocket_url;
		bool ws_privacy = true;
#endif

		int max_cookie_age = cfg_int(server, "max-cookie-age", "maximum age of an HTTP cookie (in seconds)", true, 1800);

		interface *h = new http_server(cfg, auth, ci, id, descr, { listen_adapter, listen_port, SOMAXCONN, false }, fps, jpeg_quality, time_limit, http_filters, resize_w, resize_h, motion_compatible ? s : nullptr, allow_admin, archive_access, snapshot_dir, with_subdirs, is_rest, views, handle_failure, ssl_enabled ? &sp : nullptr, stylesheet, websocket_port, websocket_url, max_cookie_age, motd, ws_privacy);

		if (ci)
			ci -> interfaces.push_back(h);

		out.push_back(h);
	}

	return out;
}

#if HAVE_LIBV4L2 == 1
interface * load_v4l2_loopback(configuration_t *const cfg, const Setting & o_vlb, source *const s, instance *const ci)
{
	const std::string id = cfg_str(o_vlb, "id", "some identifier: used for selecting this module", true, "");
	const std::string descr = cfg_str(o_vlb, "descr", "description: visible in e.g. the http server", true, "");
	const std::string pixel_format = cfg_str(o_vlb, "pixel-format", "format of the pixel-values. YUV420, RGB24", false, "YUV420");

	std::string dev = cfg_str(o_vlb, "device", "Linux v4l2 device to connect to", true, "");

	double fps = 0, interval = 0;
	if (!find_interval_or_fps(o_vlb, &interval, "fps", &fps))
		interval_fps_error("fps", "for sending frames to loopback", id.c_str());

	std::vector<filter *> *filters = nullptr;
	try {
		const Setting & f = o_vlb.lookup("filters");
		filters = load_filters(cfg, f);
	}
	catch(SettingNotFoundException & snfe) {
	}

	return new v4l2_loopback(id, descr, s, fps, dev, pixel_format, filters, ci);
}
#endif

bool load_configuration(configuration_t *const cfg, const bool verbose, const int ll)
{
	cfg->lock.lock();

	log(LL_INFO, "Loading %s...", cfg->cfg_file.c_str());

	Config lc_cfg;
	try
	{
		lc_cfg.readFile(cfg->cfg_file.c_str());
	}
	catch(const FileIOException &fioex)
	{
		fprintf(stderr, "I/O error while reading configuration file %s\n", cfg->cfg_file.c_str());
		return false;
	}
	catch(const ParseException &pex)
	{
		fprintf(stderr, "Configuration file %s parse error at line %d: %s\n", pex.getFile(), pex.getLine(), pex.getError());
		return false;
	}

	const Setting& root = lc_cfg.getRoot();

	//***

	std::string logfile = cfg_str(lc_cfg, "logfile", "file where to store logging", false, "");

	int loglevel = -1;

	if (verbose)
		loglevel = ll;
	else {
		std::string level_str = cfg_str(lc_cfg, "log-level", "log level (fatal, error, warning, info, debug, debug-verbose)", false, "");

		loglevel = to_loglevel(level_str);
	}

	setlogfile(logfile.empty() ? nullptr : logfile.c_str(), loglevel);

	log(LL_INFO, " *** " NAME " v" VERSION " starting ***");

	std::string resize_type = cfg_str(lc_cfg, "resize-type", "can be regular or crop. This selects what method will be used for resizing the video stream (if requested).", true, "");
	bool resize_before_crop = cfg_bool(lc_cfg, "resize-before-crop", "resize before crop", true, false);
	bool resize_crop_center = cfg_bool(lc_cfg, "resize-crop-center", "center after crop", true, false);

	cfg->r = nullptr;
	if (resize_type == "regular" || resize_type == "") {
		cfg->r = new resize();

		if (resize_type == "")
			log(LL_INFO, "No resizer/scaler selected (\"resize-type\"); assuming default");
	}
	else if (resize_type == "fine")
		cfg->r = new resize_fine();
	else if (resize_type == "crop")
		cfg->r = new resize_crop(resize_crop_center, resize_before_crop);
	else {
		fprintf(stderr, "Scaler/resizer of type \"%s\" is not known\n", resize_type.c_str());
		return false;
	}

	log(LL_INFO, "Configuring maintenance settings...");
	cfg->clnr = nullptr;
	cfg->nrpe.port = -1;
	try {
		const Setting & m = root.lookup("maintenance");

		try {
			cfg->nrpe.adapter = cfg_str(m, "nrpe-service-adapter", "network interface to listen on or 0.0.0.0 or ::1 for all", true, "0.0.0.0");
			cfg->nrpe.port = cfg_int(m, "nrpe-service", "on what tcp port to listen for NRPE requests (usually 5666), NO TLS", true, -1);
		}
		catch(SettingNotFoundException & snfe) {
		}

		cfg->db_url = cfg_str(m, "db-url", "database URL", true, "");

		if (!cfg->db_url.empty()) {
			cfg->db_user = cfg_str(m, "db-user", "database username", true, "");
			cfg->db_pass = cfg_str(m, "db-pass", "database password", true, "");
			interface::register_database(cfg->db_url, cfg->db_user, cfg->db_pass);

			int clean_interval = cfg_int(m, "clean-interval", "how often (in seconds) to check for old files (-1 to disable)", true, -1);
			int keep_max = cfg_int(m, "keep-max", "after how many days should things be deleted (-1 to disable)", true, -1);
			int cookie_max_age = cfg_int(m, "max-cookie-age", "maximum age of a cookie before purging it", true, 30 * 60);
			cfg->clnr = new cleaner(cfg->db_url, cfg->db_user, cfg->db_pass, clean_interval, keep_max, cookie_max_age);
		}
	}
	catch(SettingNotFoundException & snfe) {
		log(LL_INFO, " no maintenance-items set");
	}

	log(LL_INFO, "Configuring default failure handling...");
	set_default_failure();
	def_fail = load_failure(root);

	const Setting &o_instances = root["instances"];
	size_t n_instances = o_instances.getLength();

	if (n_instances == 0)
		log(LL_WARNING, " 0 instances, is that correct? Note that since version 2.0 the configuration file layout changed a tiny bit to accomodate multiple instances!");

	instance *main_instance = nullptr;

	for(size_t i_loop=0; i_loop<n_instances; i_loop++) {
		const Setting &instance_root = o_instances[i_loop];

		//***
		source *s = load_source(cfg, instance_root["source"], loglevel);
		if (!s)
			return false;

		instance *ci = new instance; // ci = current instance

		if (main_instance == nullptr)
			main_instance = ci;

		ci -> name = cfg_str(instance_root, "instance-name", "instance-name", true, "default instance name");
		ci -> descr = cfg_str(instance_root, "instance-descr", "instance-descr", true, "");

		ci -> interfaces.push_back(s);

		//***
		log(LL_INFO, "Configuring the meta sources...");

		try {
			const Setting &ms = instance_root["meta-plugin"];
			size_t n_ms = ms.getLength();

			log(LL_DEBUG, " %zu meta plugin(s)", n_ms);

			if (n_ms == 0)
				log(LL_WARNING, " 0 plugins, is that correct?");

			for(size_t i=0; i<n_ms; i++) {
				const Setting &server = ms[i];

				std::string file = cfg_str(server, "plugin-file", "filename of the meta plugin", false, "");
				std::string par = cfg_str(server, "plugin-parameter", "parameter of the meta plugin", true, "");

				s->get_meta()->add_plugin(file, par.c_str());
			}
		}
		catch(const SettingNotFoundException &nfex) {
			log(LL_INFO, " no meta source");
		}

		// listen adapter, listen port, source, fps, jpeg quality, time limit (in seconds)
		log(LL_INFO, "Configuring the HTTP server(s)...");
		try {
			load_http_servers(cfg, ci, instance_root["http-server"], true, s, nullptr, s -> get_meta());
		}
		catch(const SettingNotFoundException &nfex) {
			log(LL_INFO, " no HTTP server");
		}
		//***
#if HAVE_LIBV4L2 == 1
		log(LL_INFO, "Configuring a video-loopback...");
		try
		{
			const Setting &o_vlb = instance_root["video-loopback"];

			interface *f = load_v4l2_loopback(cfg, o_vlb, s, ci);
			ci -> interfaces.push_back(f);
		}
		catch(const SettingNotFoundException &nfex) {
			log(LL_INFO, " no video loopback");
		}
#endif
		//***

		log(LL_INFO, "Configuring the motion trigger(s)...");
		try
		{
			std::vector<interface *> triggers = load_motion_triggers(cfg, instance_root["motion-trigger"], s, ci);

			ci->interfaces.insert(ci->interfaces.end(), triggers.begin(), triggers.end());
		}
		catch(const SettingNotFoundException &nfex) {
			log(LL_INFO, " no motion trigger defined");
		}

		//***

		log(LL_INFO, "Configuring the audio trigger(s)...");
		try
		{
			std::vector<interface *> triggers = load_audio_triggers(instance_root["audio-trigger"], ci, s);

			ci->interfaces.insert(ci->interfaces.end(), triggers.begin(), triggers.end());
		}
		catch(const SettingNotFoundException &nfex) {
			log(LL_INFO, " no audio trigger defined");
		}

		//***
		log(LL_INFO, "Configuring the targets / stream-to-disk backend(s)...");
		try {
			const Setting & t = instance_root.lookup("stream-to-disk");

			for(size_t i=0; i<t.getLength(); i++)
				ci -> interfaces.push_back(load_target(t[i], s, s -> get_meta(), cfg));
		}
		catch(SettingNotFoundException & snfe) {
		}

		try {
			const Setting & t = instance_root.lookup("targets");

			for(size_t i=0; i<t.getLength(); i++)
				ci -> interfaces.push_back(load_target(t[i], s, s -> get_meta(), cfg));
		}
		catch(SettingNotFoundException & snfe) {
		}

		if (!s) {
			fprintf(stderr, "No video-source selected\n");
			return false;
		}

		cfg->instances.push_back(ci);
	}

	//***
	log(LL_INFO, "Configuring views...");
	instance *views = nullptr;
	try {
		const Setting & v = root.lookup("views");

		views = new instance; // ci = current instance
		views -> name = "views";

		size_t n_v = v.getLength();
		log(LL_DEBUG, " %zu views", n_v);

		for(size_t i=0; i<n_v; i++) {
			const Setting & cv = v[i];

			auto vp = load_view(cfg, cv, views);

			views -> interfaces.push_back(vp.first);

			for(auto t : *vp.second)
				views -> interfaces.push_back(t);

			delete vp.second;

#if HAVE_LIBV4L2 == 1
			try {
				const Setting &o_vlb = cv["video-loopback"];
				log(LL_INFO, "Configuring a video-loopback...");

				interface *f = load_v4l2_loopback(cfg, o_vlb, vp.first, views);
				views -> interfaces.push_back(f);
			}
			catch(const SettingNotFoundException &nfex) {
			}
#endif
		}

		cfg->instances.push_back(views);
	}
	catch(SettingNotFoundException & snfe) {
		log(LL_INFO, " no views defined");
	}

	try {
		log(LL_INFO, "Loading global HTTP(/REST) server(s)");
		cfg->global_http_servers = load_http_servers(cfg, nullptr, root["global-http-server"], false, nullptr, views, nullptr);
	}
	catch(const SettingNotFoundException &nfex) {
		log(LL_INFO, " no global HTTP(/REST) server(s)");
	}

#if HAVE_LIBSDL2 == 1
	try {
		log(LL_INFO, "Loading GUIs");
		cfg->guis = load_guis(cfg, root["guis"]);
	}
	catch(const SettingNotFoundException &nfex) {
		log(LL_INFO, " no GUIs");
	}
#endif

#if HAVE_RYGEL == 1
	try {
		log(LL_INFO, "Loading network announcers");
		cfg->announcers = load_net_announcers(cfg, root["announcers"]);
	}
	catch(const SettingNotFoundException &nfex) {
	}
#endif

	std::set<std::string> check_id;
	for(instance * inst : cfg->instances) {
		for(const interface *const i: inst -> interfaces) {
			std::string cur = i -> get_id();
			//printf("%s %s %s\n", inst -> name.c_str(), cur.c_str(), i -> get_descr().c_str());

			if (check_id.find(cur) != check_id.end())
				log(LL_WARNING, "There are multiple modules with the same ID (%s): this will give problems when trying to use the REST interface!", cur.c_str());
			else
				check_id.insert(cur);
		}
	}

	check_id.clear();
	for(instance * inst : cfg->instances) {
		if (check_id.find(inst -> name) != check_id.end())
			log(LL_WARNING, "There are multiple instances with the same ID (%s): this will give problems when trying to use the REST and WEB-interfaces!", inst -> name.c_str());
		else
			check_id.insert(inst -> name);
	}

	cfg->lock.unlock();

	return true;
}
