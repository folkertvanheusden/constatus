// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <stdint.h>
#include <string.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/objdetect/objdetect.hpp>

extern "C" {

void * init_filter(const char *const parameter)
{
	cv::CascadeClassifier *face_cascade = new cv::CascadeClassifier();
	if (!face_cascade -> load("/usr/share/opencv/haarcascades/haarcascade_frontalface_alt.xml"))
		printf("load failed\n");

	return face_cascade;
}

void apply_filter(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, uint8_t *const result)
{
	cv::CascadeClassifier *face_cascade = (cv::CascadeClassifier *)arg;

	cv::Mat frame = cv::Mat(h, w, CV_8UC3, (void *)current_frame);

	cv::Mat frame_gray;
	cv::cvtColor(frame, frame_gray, cv::COLOR_BGR2GRAY);
	cv::equalizeHist(frame_gray, frame_gray);

	std::vector<cv::Rect> faces;
	face_cascade -> detectMultiScale(frame_gray, faces, 1.1, 2, 0 | cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30));

	for(size_t i = 0; i < faces.size(); i++)
		cv::rectangle(frame, faces[i].tl(), faces[i].br(), cv::Scalar(0, 0, 255), 2, 8, 0);

	memcpy(result, frame.data, w * h * 3);

	frame_gray.release();
	frame.release();
}

void uninit_filter(void *arg)
{
	delete (cv::CascadeClassifier *)arg;
}

}
