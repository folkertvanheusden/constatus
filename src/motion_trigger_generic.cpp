// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <assert.h>
#include <atomic>
#if defined(__AVX2__) || defined(__AVX512BW__)
#include <immintrin.h>
#endif
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#endif
#include <errno.h>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <sys/time.h>

#include "error.h"
#include "source.h"
#include "utils.h"
#include "picio.h"
#include "target.h"
#include "filter.h"
#include "log.h"
#include "exec.h"
#include "db.h"
#include "motion_trigger_generic.h"
#include "parameters.h"
#include "selection_mask.h"
#include "schedule.h"

void dilation(const uint8_t *const in, const int w, const int h, const int kernel_size, uint8_t *const out)
{
	int half_kernel = kernel_size / 2;

	for(int y=half_kernel; y<h-half_kernel; y++) {
		for(int x=half_kernel; x<w-half_kernel; x++) {
			int best = -1;

			for(int ky=y-half_kernel; ky<y+half_kernel; ky++) {
				int offset = ky * w + x - half_kernel;

				for(int k=0; k<kernel_size; k++)
					best = std::max(int(in[offset++]), best);
			}

			out[y * w + x] = best;
		}
	}
}

void erode(const uint8_t *const in, const int w, const int h, const int kernel_size, uint8_t *const out)
{
	int half_kernel = kernel_size / 2;

	for(int y=half_kernel; y<h-half_kernel; y++) {
		for(int x=half_kernel; x<w-half_kernel; x++) {
			int worst = 256;

			for(int ky=y-half_kernel; ky<y+half_kernel; ky++) {
				int offset = ky * w + x - half_kernel;

				for(int k=0; k<kernel_size; k++)
					worst = std::min(int(in[offset++]), worst);
			}

			out[y * w + x] = worst;
		}
	}
}

void despeckle(uint8_t *const in, const int w, const int h, const std::string & pattern)
{
	size_t n = IMS(w, h, 1);
	uint8_t *temp1 = (uint8_t *)duplicate(in, n);
	uint8_t *temp2 = (uint8_t *)malloc(n);

	for(auto c : pattern) {
		if (c == 'd')
			dilation(temp1, w, h, 5, temp2);
		else if (c == 'e')
			erode(temp1, w, h, 5, temp2);
		else if (c == 'D')
			dilation(temp1, w, h, 9, temp2);
		else if (c == 'E')
			erode(temp1, w, h, 9, temp2);

		uint8_t *temp = temp1;
		temp1 = temp2;
		temp2 = temp;
	}

	memcpy(in, temp1, n);

	int half_kernel = 9 / 2;

	for(int x=0; x<w; x++) {
		for(int y=0; y<half_kernel; y++) 
			in[y * w + x] = 0;

		for(int y=h-half_kernel; y<h; y++)
			in[y * w + x] = 0;
	}

	for(int y=0; y<h; y++) {
		for(int x=0; x<half_kernel; x++)
			in[y * w + x] = 0;

		for(int x=w-half_kernel; x<w; x++)
			in[y * w + x] = 0;
	}

	free(temp2);
	free(temp1);
}

static void to_gray(const uint8_t *const rgb_in, const int n_pixels, uint8_t *const gray_out)
{
	for(int i=0, o=0; o<n_pixels;) {
		int r = rgb_in[i++];
		int g = rgb_in[i++];
		int b = rgb_in[i++];

		gray_out[o++] = (r + g + b) / 3;
	}
}

void calc_diff(uint8_t *const dest, const int n_pixels, const uint8_t *const a, const uint8_t *const b)
{
	for(int i=0; i<n_pixels; i++)
		dest[i] = abs(a[i] - b[i]);
}

int count_over_threshold(const uint8_t *const cur, const uint8_t *const prev, const int n_pixels, const int threshold)
{
	if (threshold > 255)
		return 0;
	if (threshold <= 0)
		return n_pixels;

	int cnt = 0;
	int i   = 0;

	// SIMD processing cascades from widest vectors down to scalar. Each block
	// advances shared index 'i', so there is no overlap.
#if defined(__AVX512BW__)
	{
		const __m512i thresh_vec = _mm512_set1_epi8(static_cast<uint8_t>(threshold));
		for (; i <= n_pixels - 64; i += 64) {
			__m512i a        = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(cur  + i));
			__m512i b        = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(prev + i));
			// abs diff for uint8: max(a,b) - min(a,b)
			__m512i abs_diff = _mm512_sub_epi8(_mm512_max_epu8(a, b), _mm512_min_epu8(a, b));
			__mmask64 mask   = _mm512_cmpge_epu8_mask(abs_diff, thresh_vec);
			cnt += __builtin_popcountll(mask);
		}
	}
#endif
#if defined(__AVX2__)
	{
		const __m256i thresh_vec = _mm256_set1_epi8(static_cast<uint8_t>(threshold));
		for (; i <= n_pixels - 32; i += 32) {
			__m256i a        = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(cur  + i));
			__m256i b        = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(prev + i));
			// abs diff for uint8: max(a,b) - min(a,b)
			__m256i abs_diff = _mm256_sub_epi8(_mm256_max_epu8(a, b), _mm256_min_epu8(a, b));
			// abs_diff >= threshold: max(abs_diff, thresh) == abs_diff
			__m256i cmp      = _mm256_cmpeq_epi8(_mm256_max_epu8(abs_diff, thresh_vec), abs_diff);
			cnt += __builtin_popcount(static_cast<uint32_t>(_mm256_movemask_epi8(cmp)));
		}
	}
#endif
#if defined(__ARM_NEON) || defined(__aarch64__)
	{
		const uint8x16_t thresh_vec = vdupq_n_u8(static_cast<uint8_t>(threshold));
		for (; i <= n_pixels - 16; i += 16) {
			uint8x16_t a        = vld1q_u8(cur + i);
			uint8x16_t b        = vld1q_u8(prev + i);
			uint8x16_t abs_diff = vabdq_u8(a, b);
			uint8x16_t ge       = vcgeq_u8(abs_diff, thresh_vec);
			uint8x16_t ones     = vshrq_n_u8(ge, 7);
			uint16x8_t sum16    = vpaddlq_u8(ones);
			uint32x4_t sum32    = vpaddlq_u16(sum16);
#if defined(__aarch64__)
			cnt += static_cast<int>(vaddvq_u32(sum32));
#else
			cnt += static_cast<int>(vgetq_lane_u32(sum32, 0) + vgetq_lane_u32(sum32, 1) + vgetq_lane_u32(sum32, 2) + vgetq_lane_u32(sum32, 3));
#endif
		}
	}
#endif

	for (; i < n_pixels; i++)
		cnt += abs(cur[i] - prev[i]) >= threshold;

	return cnt;
}

motion_trigger_generic::motion_trigger_generic(const std::string & id, const std::string & descr, source *const s, const int camera_warm_up, const std::vector<filter *> *const filters, std::vector<target *> *const targets, selection_mask *const pixel_select_bitmap, ext_trigger_t *const et, const std::string & e_start, const std::string & e_end, instance *const inst, const std::map<std::string, parameter *> & detection_parameters, schedule *const sched) : motion_trigger(id, descr, detection_parameters, targets, sched), s(s), camera_warm_up(camera_warm_up), filters(filters), pixel_select_bitmap(pixel_select_bitmap), et(et), exec_start(e_start), exec_end(e_end), inst(inst)
{
	ct = CT_MOTIONTRIGGER;

	if (et)
		et -> arg = et -> init_motion_trigger(et -> par.c_str());

	motion_triggered = false;

	local_stop_flag = false;
	th = nullptr;
}

motion_trigger_generic::~motion_trigger_generic()
{
	stop();

	if (et)
		et -> uninit_motion_trigger(et -> arg);

	for(target *t : *targets)
		delete t;
	delete targets;

	delete pixel_select_bitmap;

	free_filters(filters);
}

void motion_trigger_generic::operator()()
{
	set_thread_name("motion_trigger_generic");

	video_frame *prev_frame { nullptr };

	int stopping = 0, mute = 0, w = -1, h = -1;

	std::vector<video_frame *> prerecord;

	motion_triggered = false;

	s -> start();

	if (camera_warm_up)
		log(id, LL_INFO, "Warming up...");

	bool first = true; // at least once to have sane values for w/h
	for(int i=0; (i<camera_warm_up && !local_stop_flag) || first; i++) {
		log(id, LL_DEBUG, "Warm-up... %d", i);

		video_frame *pvf = s->get_frame(false, get_us());
		if (pvf) {
			if (first) {
				first = false;

				w = pvf->get_w();
				h = pvf->get_h();
			}

			delete pvf;
		}
	}

	const int n_pixels = w * h;

	std::thread *exec_start_th { nullptr };
	std::thread *exec_end_th { nullptr };

	log(id, LL_INFO, "Go!");

	std::string despeckle_filter = parameter::get_value_string(parameters, "despeckle-filter");
	if (!despeckle_filter.empty())
		log(id, LL_INFO, "Despeckle filter enabled (%s)", despeckle_filter.c_str());

	uint8_t *gray_cur  = (uint8_t *)malloc(n_pixels);
	uint8_t *gray_prev = (uint8_t *)calloc(1, n_pixels);
	uint8_t *scratch   = (uint8_t *)malloc(n_pixels);

	unsigned long event_nr = -1;
	int count_frames_changed = 0;
	for(;!local_stop_flag;) {
		pauseCheck();

		bool allow_triggers = sched == nullptr || (sched && sched->is_on());

		video_frame *pvf = s -> get_frame(false, get_us());
		if (!pvf)
			continue;

		if (pvf->get_w() != w || pvf->get_h() != h) {
			log(id, LL_ERR, "Camera resized! Aborting");
			delete pvf;
			break;
		}

		st->track_fps();

		if (filters && !filters->empty()) {
			video_frame *temp = pvf->apply_filtering(nullptr, s, prev_frame, filters, nullptr);
			delete pvf;
			pvf = temp;
		}

		pvf->keep_only_format(E_RGB);

		int cnt = 0, cx = 0, cy = 0;
		double pan_factor = 1, tilt_factor = 1;
		bool triggered = false;

		bool pan_tilt = parameter::get_value_bool(parameters, "pan-tilt");

		uint8_t *psb = pixel_select_bitmap ? pixel_select_bitmap -> get_mask(w, h) : nullptr;

		const int nl = parameter::get_value_int(parameters, "noise-factor");
		const int nl3 = nl * 3;

		if (et) {
			if (prev_frame) {
				char *meta = nullptr;

				triggered = et -> detect_motion(et -> arg, pvf->get_ts(), w, h, prev_frame->get_data(E_RGB), pvf->get_data(E_RGB), psb, &meta);

				if (meta) {
					get_meta() -> set_string("$motion-meta$", std::pair<uint64_t, std::string>(get_us(), meta));
					free(meta);
				}

				delete prev_frame;
			}

			prev_frame = pvf->duplicate(E_RGB);
		}
		else if (psb || pan_tilt || !despeckle_filter.empty()) {
			to_gray(pvf->get_data(E_RGB), n_pixels, gray_cur);
			calc_diff(scratch, n_pixels, gray_cur, gray_prev);

			if (!despeckle_filter.empty())
				despeckle(scratch, w, h, despeckle_filter);

			for(int i=0; i<n_pixels; i++) {
				if (psb && !psb[i])
					continue;

				if (scratch[i] >= nl) {
					cnt++;
					cx += i % w;
					cy += i / w;
				}
			}

			double temp = cnt * 100.0 / n_pixels;
			triggered = temp > parameter::get_value_double(parameters, "min-pixels-changed-percentage") && temp < parameter::get_value_double(parameters, "max-pixels-changed-percentage");

			if (triggered && pan_tilt) {
				cx /= cnt;
				cy /= cnt;

				int dx = 0, dy = 0;

				for(int y=0, o=0; y<h; y++) {
					for(int x=0; x<w; x++) {
						if (scratch[o++] >= nl) {
							dx += abs(cx - x);
							dy += abs(cy - y);
						}
					}
				}

				dx /= cnt;
				dy /= cnt;

				pan_factor = (cx - dx) * 2. / w;
				tilt_factor = (cy - dy) * 2. / h;
			}
		}
		else {
			if (prev_frame) {
				cnt = count_over_threshold(pvf->get_data(E_RGB), prev_frame->get_data(E_RGB), n_pixels * 3, nl);

				double temp = cnt * 100.0 / (n_pixels * 3);
				triggered = temp > parameter::get_value_double(parameters, "min-pixels-changed-percentage") && temp < parameter::get_value_double(parameters, "max-pixels-changed-percentage");
			}
		}

		get_meta() -> set_double("$pixels-changed$", std::pair<uint64_t, double>(0, cnt * 100.0 / n_pixels));

		bool triggered_by_audio = cv_event_notified.exchange(false);

		if (mute) {
			log(id, LL_DEBUG, "mute");
			mute--;

			if (mute == 0 && pan_tilt)
				s->pan_tilt(0., 0.);
		}
		else if ((triggered || triggered_by_audio) && allow_triggers) {
			const int min_n_frames = parameter::get_value_int(parameters, "min-n-frames");

			count_frames_changed++;

			if (count_frames_changed >= min_n_frames || et) {
				log(id, LL_INFO, "motion detected (%f%% of the pixels changed)", cnt * 100.0 / n_pixels);

				for(auto ec : event_clients)
					ec->notify_thread_of_event(s->get_id());

				if (!exec_start.empty()) {
					if (check_thread(&exec_start_th))
						exec_start_th = exec(exec_start, "");
				}

				if (!motion_triggered) {
					log(id, LL_DEBUG, " starting store");

					motion_triggered = true;

					event_nr = get_db() -> register_event(id, EVT_MOTION, "motion start");

					time_t t_now = time(nullptr);
					tm tm { };
					localtime_r(&t_now, &tm);
					char buffer[64] { };
					snprintf(buffer, sizeof buffer, "%04d-%02d-%02d %02d:%02d:%02d",
							tm.tm_year + 1900, tm.tm_mon+1, tm.tm_mday,
							tm.tm_hour, tm.tm_min, tm.tm_sec);
					get_meta()->set_string("$motion-timestamp$", std::pair<uint64_t, std::string>(get_us(), buffer));

					if (targets -> empty()) {
						for(auto r : prerecord)
							delete r;
					}
					else {
						std::vector<video_frame *> empty;

						for(size_t i=0; i<targets -> size(); i++) {
							targets -> at(i) -> set_on_demand(true);

							targets -> at(i) -> start(i == 0 ? prerecord : empty, event_nr);
						}
					}

					prerecord.clear();

					// no pan/tilt if ONLY triggered by audio
					if (pan_tilt && triggered) {
						const int hw = w / 2, hh = h / 2;

						int sign_pan = cx < 0 ? -1 : 1, sign_tilt = cy < 0 ? -1 : 1;
						double new_pan = sign_pan * (abs(cx) - abs(hw)) * 180. / hw;
						double new_tilt = sign_tilt * (abs(cy) - abs(hh)) * 180. / hh;

						double cur_pan = 0., cur_tilt = 0.;
						s->get_pan_tilt(&cur_pan, &cur_tilt);

						new_pan = (new_pan - cur_pan) * pan_factor + cur_pan;
						new_tilt = (new_tilt - cur_tilt) * tilt_factor + cur_tilt;

						log(id, LL_ERR, "Pan to %f (%f), tilt to %f (%f)", new_pan, pan_factor, new_tilt, tilt_factor);

						s->pan_tilt(new_pan, new_tilt);
					}

					std::string remember_trigger = parameter::get_value_string(parameters, "remember-trigger");

					if (!remember_trigger.empty()) {
						video_frame *copy = pvf->duplicate({ });

						get_meta()->set_bitmap(remember_trigger, std::pair<uint64_t, video_frame *>(0, copy));
					}
				}
			}
			else {
				log(id, LL_INFO, "%d/%d motion detected (%f%% of the pixels changed)", count_frames_changed, min_n_frames, cnt * 100.0 / n_pixels);
			}

			stopping = 0;
		}
		else if (motion_triggered) {
			log(id, LL_DEBUG, "stop motion");

			count_frames_changed = 0;

			stopping++;

			if (stopping > parameter::get_value_int(parameters, "min-duration")) {
				log(id, LL_INFO, " stopping (%ld)", event_nr);

				get_db()->register_event_end(event_nr);

				for(target *t : *targets)
					t -> stop();

				if (!exec_end.empty()) {
					if (check_thread(&exec_end_th))
						exec_end_th = exec(exec_end, "");
				}

				motion_triggered = false;

				mute = parameter::get_value_int(parameters, "mute-duration");
			}
		}

		size_t pre_duration = parameter::get_value_int(parameters, "pre-motion-record-duration");

		delete prev_frame;
		if (pre_duration > 0) {
			if (prerecord.size() >= pre_duration) {
				delete prerecord.at(0);

				prerecord.erase(prerecord.begin() + 0);
			}

			prerecord.push_back(pvf);

			prev_frame = pvf->duplicate(E_RGB);
		}
		else {
			prev_frame = pvf;
		}

		std::swap(gray_cur, gray_prev);

		st->track_cpu_usage();

		double fps_temp = parameter::get_value_double(parameters, "max-fps");
		if (fps_temp > 0.0)
			mysleep(1000000 / fps_temp, &local_stop_flag, s);
	}

	free(gray_prev);
	free(gray_cur);
	free(scratch);
	delete prev_frame;

	while(!prerecord.empty()) {
		delete prerecord.at(0);

		prerecord.erase(prerecord.begin() + 0);
	}

	join_thread(&exec_start_th, id, "exec start");

	join_thread(&exec_end_th, id, "exec end");

	s -> stop();

	register_thread_end("motion_trigger_generic");
}
