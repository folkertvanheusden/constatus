// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <stdint.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/objdetect/objdetect.hpp>
// #include <opencv2/highgui/highgui.hpp>

extern "C" {

void * init_motion_trigger(const char *const parameter)
{
	// you can use the parameter for anything you want
	// e.g. the filename of a configuration file or
	// maybe a variable or whatever

	// what you return here, will be given as a parameter
	// to detect_motion

	cv::CascadeClassifier *face_cascade = new cv::CascadeClassifier();
	if (!face_cascade -> load("/usr/share/opencv/haarcascades/haarcascade_frontalface_alt.xml"))
		printf("load failed\n");

	return face_cascade;
}

bool detect_motion(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, uint8_t *const current_frame, const uint8_t *const pixel_selection_bitmap, char **const meta)
{
	cv::CascadeClassifier *face_cascade = (cv::CascadeClassifier *)arg;

	cv::Mat frame = cv::Mat(h, w, CV_8UC3, (void *)current_frame);

	cv::Mat frame_gray;
	cv::cvtColor(frame, frame_gray, cv::COLOR_BGR2GRAY);
	cv::equalizeHist(frame_gray, frame_gray);

//	cv::imwrite("gray.png", frame_gray);
//	cv::imwrite("rgb.png", frame);
//	exit(0);

	std::vector<cv::Rect> faces;
	face_cascade -> detectMultiScale(frame_gray, faces, 1.1, 2, 0 | cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30));

	frame_gray.release();
	frame.release();

	return !faces.empty();
}

void uninit_motion_trigger(void *arg)
{
	// free memory etc
	delete (cv::CascadeClassifier *)arg;
}

}
