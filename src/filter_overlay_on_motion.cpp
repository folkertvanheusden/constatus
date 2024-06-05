// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <string>
#include <cstring>
#include <time.h>

#include "exec.h"
#include "error.h"
#include "gen.h"
#include "filter_overlay_on_motion.h"
#include "instance.h"
#include "motion_trigger.h"
#include "resize.h"
#include "utils.h"
#include "view.h"


filter_overlay_on_motion::filter_overlay_on_motion(instance *const i, source *const cks, resize *const r, const int display_time_ms) :
	i(i),
	cks(cks),
	r(r),
	display_time_ms(display_time_ms)
{
        cks->start();

	for(auto mi : find_motion_triggers(i))
		static_cast<motion_trigger *>(mi)->register_notifier(this);
}

filter_overlay_on_motion::~filter_overlay_on_motion()
{
	for(auto mi : find_motion_triggers(i))
		static_cast<motion_trigger *>(mi)->unregister_notifier(this);
}

void filter_overlay_on_motion::notify_thread_of_event(const std::string & subject)
{
	changed = true;

	// TODO locking
	since = get_us() / 1000;
}

void filter_overlay_on_motion::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	if (!changed)
		return;

	if (get_us() / 1000 - since >= display_time_ms) {
		changed = false;
		return;
	}

	// OVERLAY
	video_frame *vf = cks->get_frame(true, ts);

	if (vf) {
		const int in_w = vf->get_w();
		const int in_h = vf->get_h();

		int perc = std::max(1, std::min(w * 100 / in_w, h * 100 / in_h));

		pos_t p { center_center, 0, 0 };

		picture_in_picture(r, in_out, w, h, vf->get_data(E_RGB), in_w, in_h, perc, p);

		delete vf;
	}
}
