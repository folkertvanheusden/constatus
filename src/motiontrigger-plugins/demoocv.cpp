// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <stdint.h>
#include <string.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/imgproc/imgproc.hpp>

extern "C" {

bool first = true;
cv::Mat *prevFrame = NULL;

void * init_motion_trigger(const char *const parameter)
{
	// you can use the parameter for anything you want
	// e.g. the filename of a configuration file or
	// maybe a variable or whatever

	// what you return here, will be given as a parameter
	// to detect_motion

	return NULL;
}

bool detect_motion(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, uint8_t *const current_frame, const uint8_t *const pixel_selection_bitmap, char **const meta)
{
	if (first) {
		first = false;

		if (!prevFrame)
			prevFrame = new cv::Mat();

		*prevFrame = cv::Mat(w * h, 1, CV_8UC3, (void *)prev_frame);
		*prevFrame = prevFrame -> reshape(0, h);

		cv::Mat grayFrame;
		cv::cvtColor(*prevFrame, grayFrame, cv::COLOR_BGR2GRAY);

		cv::GaussianBlur(grayFrame, *prevFrame, cv::Size(21, 21), 0);
	}

	cv::Mat curFrame = cv::Mat(w * h, 1, CV_8UC3, (void *)current_frame);
	curFrame = curFrame.reshape(0, h);

	cv::Mat grayFrame;
	cv::cvtColor(curFrame, grayFrame, cv::COLOR_BGR2GRAY);

	cv::Mat blurFrame;
	cv::GaussianBlur(grayFrame, blurFrame, cv::Size(21, 21), 0);

	cv::Mat keep = blurFrame;

	cv::Mat deltaFrame;
	cv::absdiff(blurFrame, *prevFrame, deltaFrame);

	cv::Mat thFrame;
	cv::threshold(deltaFrame, thFrame, 100, 255, cv::THRESH_BINARY);

	cv::Mat dilFrame;
	cv::dilate(thFrame, dilFrame, cv::Mat(), cv::Point(-1, -1), 2, 1, 1);

	std::vector<std::vector<cv::Point> > contours;
	cv::Mat contourOutput = dilFrame.clone();
	cv::findContours(contourOutput, contours, CV_RETR_LIST, CV_CHAIN_APPROX_NONE);

	*prevFrame = keep.clone();

//
	for(std::vector<cv::Point> v : contours) {
		if (cv::contourArea(v) >= 400)
			return true;
	}

	return false;
}

void uninit_motion_trigger(void *arg)
{
	// free memory etc
}

}
