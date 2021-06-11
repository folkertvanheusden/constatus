// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <stdint.h>
#include <string.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/imgproc/imgproc.hpp>

extern "C" {

void * init_filter(const char *const parameter)
{
	// you can use the parameter for anything you want
	// e.g. the filename of a configuration file or
	// maybe a variable or whatever

	printf("OpenCV build info: %s, number of threads: %u, using optimized code: %d\n", cv::getBuildInformation().c_str(), cv::getNumThreads(), cv::useOptimized());

	return NULL;
}

cv::RNG rng(12345);

void apply_filter(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, uint8_t *const result)
{
	cv::Mat imageWithData = cv::Mat(w * h, 1, CV_8UC3, (void *)current_frame);
	cv::Mat reshapedImage = imageWithData.reshape(0, h);

	cv::Mat imageGray;
	cv::cvtColor(reshapedImage, imageGray, CV_BGR2GRAY);

	//cv::threshold(imageGray, imageGray, 128, 255, CV_THRESH_BINARY);
	cv::Canny(imageGray, imageGray, 100, 100*2, 3);

	std::vector<std::vector<cv::Point> > contours;
	std::vector<cv::Vec4i> hierarchy;

	cv::findContours(imageGray, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cv::Point(0, 0));

	for(size_t i = 0; i< contours.size(); i++) {
		cv::Scalar color = cv::Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255));

		drawContours(reshapedImage, contours, i, color, 2, 8, hierarchy, 0, cv::Point());
	}

	memcpy(result, reshapedImage.data, w * h * 3);
}

void uninit_filter(void *arg)
{
	// free memory etc
}

}
