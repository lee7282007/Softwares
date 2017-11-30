﻿
#include "faceLandmarkDetect.h"

// The contents of this file are in the public domain. See LICENSE_FOR_EXAMPLE_PROGRAMS.txt
/*

This example program shows how to find frontal human faces in an image and
estimate their pose.  The pose takes the form of 68 landmarks.  These are
points on the face such as the corners of the mouth, along the eyebrows, on
the eyes, and so forth.

The face detector we use is made using the classic Histogram of Oriented
Gradients (HOG) feature combined with a linear classifier, an image pyramid,
and sliding window detection scheme.  The pose estimator was created by
using dlib's implementation of the paper:
One Millisecond Face Alignment with an Ensemble of Regression Trees by
Vahid Kazemi and Josephine Sullivan, CVPR 2014
and was trained on the iBUG 300-W face landmark dataset (see
https://ibug.doc.ic.ac.uk/resources/facial-point-annotations/):
C. Sagonas, E. Antonakos, G, Tzimiropoulos, S. Zafeiriou, M. Pantic.
300 faces In-the-wild challenge: Database and results.
Image and Vision Computing (IMAVIS), Special Issue on Facial Landmark Localisation "In-The-Wild". 2016.
You can get the trained model file from:
http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2.
Note that the license for the iBUG 300-W dataset excludes commercial use.
So you should contact Imperial College London to find out if it's OK for
you to use this model file in a commercial product.

Also, note that you can train your own models using dlib's machine learning
tools.  See train_shape_predictor_ex.cpp to see an example.

Finally, note that the face detector is fastest when compiled with at least
SSE2 instructions enabled.  So if you are using a PC with an Intel or AMD
chip then you should enable at least SSE2 instructions.  If you are using
cmake to compile this program you can enable them by using one of the
following commands when you create the build project:
cmake path_to_dlib_root/examples -DUSE_SSE2_INSTRUCTIONS=ON
cmake path_to_dlib_root/examples -DUSE_SSE4_INSTRUCTIONS=ON
cmake path_to_dlib_root/examples -DUSE_AVX_INSTRUCTIONS=ON
This will set the appropriate compiler options for GCC, clang, Visual
Studio, or the Intel compiler.  If you are using another compiler then you
need to consult your compiler's manual to determine how to enable these
instructions.  Note that AVX is the fastest but requires a CPU from at least
2011.  SSE4 is the next fastest and is supported by most current machines.
*/

#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
#ifdef With_Debug
#include <dlib/gui_widgets.h>
#endif // With_Debug
#include <dlib/image_io.h>
#include <iostream>

#include <dlib/opencv.h>    
#include <opencv2/objdetect.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/objdetect.hpp>

#include "faceLandmarkDetect.h"

#ifdef __linux
#include <pthread.h>
#else
#include <windows.h>
#include <io.h>
#endif

using namespace dlib;
using namespace std;

static bool g_bShapePredictorInited = false;
static shape_predictor g_sp;
#ifdef __linux
//const static string g_strCascadeName = "/usr/local/FaceParser/data/cascades/haarcascades/haarcascade_frontalface_alt.xml";
/* 下面的检测速度要更快一些，比上面的速度提升一倍 */
const static string g_strCascadeName = "/usr/local/FaceParser/data/cascades/lbpcascades/lbpcascade_frontalface_improved.xml";
const static string g_strFaceLandmarks = "/usr/local/FaceParser/data/shape_predictor_68_face_landmarks.dat"; 
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
#else
const static string g_strCascadeName = "D:\\CureFaceParser\\data\\cascades\\lbpcascades\\lbpcascade_frontalface_improved.xml";
const static string g_strFaceLandmarks = "D:\\CureFaceParser\\data\\shape_predictor_68_face_landmarks.dat";
HANDLE  g_hMutex = NULL;
#endif
static frontal_face_detector g_detector;
static cv::CascadeClassifier g_cascade;

// ----------------------------------------------------------------------------------------
bool faceLandmarkDetect(const string &strImageName, const cv::Mat &matSrc, vectorContours &faceContours, cv::Rect &rectOutput)
{
#ifdef __linux
	pthread_mutex_lock(&g_mutex);
#else
	if (!g_hMutex)
		g_hMutex = CreateMutex(NULL, false, NULL);
	WaitForSingleObject(g_hMutex, INFINITE);
#endif

	int64 tt = 0;
	if (!g_bShapePredictorInited) {
		if (_access(g_strFaceLandmarks.c_str(), 04)) {
			LOG(ERROR) << "File is not exist: " << g_strFaceLandmarks;
			/* 这里改用goto    语句的话，gcc会报怨，并且关闭警告不太好，其实提示作用挺好 */
			ReleaseMutex(g_hMutex);
			return false;
		}
		if (_access(g_strCascadeName.c_str(), 04)) {
			LOG(ERROR) << "File is not exist: " << g_strCascadeName;
			ReleaseMutex(g_hMutex);
			return false;
		}

		/* 加载面部预测器，文件比较大，所以在共享库中全局共享 */
		tt = cv::getTickCount();
		deserialize(g_strFaceLandmarks) >> g_sp;
		if (!g_cascade.load(g_strCascadeName)) {
			LOG(ERROR) << "Load cascadde file failed!";
			ReleaseMutex(g_hMutex);
			return false;
		}
		g_bShapePredictorInited = true;
		tt = cv::getTickCount() - tt;
		LOG(INFO) << "Init shape predictor finish, use time: " << to_string(tt * 1000 / (int64)cv::getTickFrequency()) << "ms";
	}

	// Make the image larger so we can detect small faces.
	//pyramid_up(img); 手动去掉


	/* 检测所有可能存在的人脸 */
	tt = cv::getTickCount();
#ifdef Use_Dlib_Detector
	/* 使用dlib的detector()很慢，720P的图片进行人脸扫描时需要长达8秒的时间 */
	g_detector = get_frontal_face_detector();
	std::vector<dlib::rectangle> dets = g_detector(img);
	LOG(INFO) << strImageName << ": Number of faces detected: " << dets.size();
#else
	std::vector<dlib::rectangle> dets;
	dets.resize(1);
	std::vector<cv::Rect> rectFaces;
	cv::Mat imgGray;
	cv::cvtColor(matSrc, imgGray, cv::COLOR_BGR2GRAY);
	cv::equalizeHist(imgGray, imgGray);
	g_cascade.detectMultiScale(imgGray, rectFaces, 1.1, 2, 0 | cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30));
	if (rectFaces.size() <= 0) {
		LOG(ERROR) << strImageName << ": Cannot detect face from image.";
		ReleaseMutex(g_hMutex);
		return false;
	}
	
    dets[0].set_left(rectFaces[0].x);
    dets[0].set_top(rectFaces[0].y);
    dets[0].set_right(rectFaces[0].x + rectFaces[0].width);
    dets[0].set_bottom(rectFaces[0].y + rectFaces[0].height);
#endif
	tt = cv::getTickCount() - tt;
	LOG(INFO) << strImageName << ": Detect face use time: " << to_string(tt * 1000 / (int64)cv::getTickFrequency()) << "ms";
	
	if (dets.size() <= 0) {
		ReleaseMutex(g_hMutex);
		return false;
	}

	/* 获取面部形状，这里用的是68点进行描述。并且认为只有一个人脸被检测到，也只取第1个检测到的人脸数据 */
	std::vector<cv::Point> vectorShape;

	tt = cv::getTickCount();

	full_object_detection shape = g_sp(cv_image<rgb_pixel>(matSrc), dets[0]);

	tt = cv::getTickCount() - tt;
	LOG(INFO) << strImageName << ": Shape predictor use time: " << to_string(tt * 1000 / (int64)cv::getTickFrequency()) << "ms";

	if (shape.num_parts() < 68) {
		LOG(ERROR) << strImageName << ": Get face shape points failed, shape number parts: " << shape.num_parts();
		ReleaseMutex(g_hMutex);
		return false;
	}

#ifdef With_Debug
	cv::Mat matDebug;
	matSrc.copyTo(matDebug);

	for (uint i = 0; i < shape.num_parts(); ++i) {
		cv::circle(matDebug, cv::Point(shape.part(i).x(), shape.part(i).y()), 5, cv::Scalar(0, 255, 0), 2, cv::FILLED);
	}
	cv::namedWindow("68点区域", cv::WINDOW_NORMAL);
	cv::imshow("68点区域", matDebug);
	cv::waitKey();
#endif

	faceContours.clear();
	tt = cv::getTickCount();

	/* 左脸 */
	vectorShape.resize(13);
	vectorShape[0] = cv::Point(shape.part(36).x() - 20, shape.part(36).y() + 40);
	vectorShape[1] = cv::Point(shape.part(41).x(), shape.part(41).y() + 20);
	vectorShape[2] = cv::Point(shape.part(40).x(), shape.part(40).y() + 20);
	vectorShape[3] = cv::Point(shape.part(39).x(), shape.part(39).y() + 20);
	vectorShape[4] = cv::Point((shape.part(39).x() + shape.part(40).x()) / 2, shape.part(1).y());
	vectorShape[5] = cv::Point(shape.part(40).x(), shape.part(2).y());
	vectorShape[6] = cv::Point(shape.part(41).x(), shape.part(4).y());
	vectorShape[7] = cv::Point(shape.part(5).x() + 20, shape.part(5).y());
	vectorShape[8] = cv::Point(shape.part(4).x() + 20, shape.part(4).y());
	vectorShape[9] = cv::Point(shape.part(3).x() + 20, shape.part(3).y());
	vectorShape[10] = cv::Point(shape.part(2).x() + 20, shape.part(2).y());
	vectorShape[11] = cv::Point(shape.part(1).x() + 20, shape.part(1).y());
	vectorShape[12] = cv::Point(shape.part(0).x() + 20, shape.part(0).y());
	faceContours.push_back(vectorShape);

	/* 右脸 */
	vectorShape.resize(13);
	vectorShape[0] = cv::Point(shape.part(45).x() - 20, shape.part(45).y() + 40);
	vectorShape[1] = cv::Point(shape.part(46).x(), shape.part(46).y() + 20);
	vectorShape[2] = cv::Point(shape.part(47).x(), shape.part(47).y() + 20);
	vectorShape[3] = cv::Point(shape.part(42).x(), shape.part(42).y() + 20);
	vectorShape[4] = cv::Point((shape.part(42).x() + shape.part(47).x()) / 2, shape.part(15).y());
	vectorShape[5] = cv::Point(shape.part(47).x() - 20, shape.part(14).y());
	vectorShape[6] = cv::Point(shape.part(11).x() - 40, shape.part(12).y());
	vectorShape[7] = cv::Point(shape.part(11).x() - 20, shape.part(11).y());
	vectorShape[8] = cv::Point(shape.part(12).x() - 20, shape.part(12).y());
	vectorShape[9] = cv::Point(shape.part(13).x() - 20, shape.part(13).y());
	vectorShape[10] = cv::Point(shape.part(14).x() - 20, shape.part(14).y());
	vectorShape[11] = cv::Point(shape.part(15).x() - 20, shape.part(15).y());
	vectorShape[12] = cv::Point(shape.part(16).x() - 20, shape.part(16).y());
	faceContours.push_back(vectorShape);

	/* 上额 */
	vectorShape.resize(4);
	vectorShape[0] = cv::Point(shape.part(17).x(), shape.part(19).y() - 200);
	vectorShape[1] = cv::Point(shape.part(17).x(), shape.part(19).y() - 20);
	vectorShape[2] = cv::Point(shape.part(25).x(), shape.part(19).y() - 20);
	vectorShape[3] = cv::Point(shape.part(25).x(), shape.part(19).y() - 200);
	faceContours.push_back(vectorShape);

	/* 下额 */
	vectorShape.resize(7);
	vectorShape[0] = cv::Point(shape.part(5).x(), shape.part(5).y());
	vectorShape[1] = cv::Point(shape.part(6).x(), shape.part(6).y());
	vectorShape[2] = cv::Point(shape.part(7).x(), shape.part(7).y());
	vectorShape[3] = cv::Point(shape.part(8).x(), shape.part(8).y());
	vectorShape[4] = cv::Point(shape.part(9).x(), shape.part(9).y());
	vectorShape[5] = cv::Point(shape.part(10).x(), shape.part(10).y());
	vectorShape[6] = cv::Point(shape.part(11).x(), shape.part(11).y());
	faceContours.push_back(vectorShape);

	/* 鼻子 */
	vectorShape.resize(3);
	vectorShape[0] = cv::Point(shape.part(27).x(), shape.part(27).y());
	vectorShape[1] = cv::Point(shape.part(31).x(), shape.part(30).y());
	vectorShape[2] = cv::Point(shape.part(35).x(), shape.part(30).y());
	faceContours.push_back(vectorShape);

	/* 整个人脸正中部分，用于取面部肤色(同下面类似，只是这个取的比较全面；虽然有鼻子等数据，但是最终用的是统计图最大值部分) */
	/* 
		vectorShape.resize(4);
		vectorShape[0] = cv::Point(shape.part(4).x(), shape.part(4).y());
		vectorShape[1] = cv::Point(shape.part(12).x(), shape.part(4).y());
		vectorShape[2] = cv::Point(shape.part(12).x(), shape.part(46).y());
		vectorShape[3] = cv::Point(shape.part(4).x(), shape.part(46).y());
		faceContours.push_back(vectorShape);
	*/
	// 这里还是使用rect保存数据，不然调试显示的时候不太直观
	rectOutput = cv::Rect(cv::Point(shape.part(4).x(), shape.part(4).y()), cv::Point(shape.part(12).x(), shape.part(46).y()));

	tt = cv::getTickCount() - tt;
	LOG(INFO) << strImageName << ": Get face every parts use time: " << to_string(tt * 1000 / (int64)cv::getTickFrequency()) << "ms";

#ifdef __linux
	pthread_mutex_unlock(&g_mutex);
#else
	ReleaseMutex(g_hMutex);
#endif

#ifdef With_Debug
	cv::Mat matTest;
	matSrc.copyTo(matTest);
					
	//cv::drawContours(matTest, contours, 0, cv::Scalar(0, 0, 0), 3);
	cv::drawContours(matTest, faceContours, -1, cv::Scalar(0, 0, 0), cv::LINE_8); // CV_FILLED
	cv::namedWindow("选取的脸部区域", cv::WINDOW_NORMAL);
	cv::imshow("选取的脸部区域", matTest);
	cv::waitKey();
#endif // With_Debug

	return true;
}
// ----------------------------------------------------------------------------------------

