﻿
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#ifdef With_Debug
#include "opencv2/highgui.hpp"
#endif // With_Debug

#include "faceSpotsDetect.h"

using namespace cv;

#define MIN_SIZE_PIMPLES 20 // 痘痘的最小尺寸
#define MAX_SIZE_PIMPLES 250
#define MAX_SIZE_BLACKHEADS 20 // 黑头的最大尺寸

#define MAX_RATIO 2.5 // 矩形长宽比最大值
#define MIN_RATIO 0.3 // 矩形长宽比最小值

#define NUMBER_PORE_ROUGH 150
#define NUMBER_PORE_NORMAL 60

#ifndef WITH_SPOTS_AS_PIMPLES
//#define WITH_SPOTS_AS_PIMPLES // 将斑点都当作痘痘来处理，否则排除斑点，只计算痘痘
#endif // WITH_SPOTS_AS_PIMPLES

#define MIN_COLOR_PIMPLES 20
#define MIN_COLOR_DIFF_G_R 20
#define MAX_COLOR_BLACKHEADS 255
#define MIN_COLOR_OIL 190

#define OIL_LEVEL_LOW 10.0
#define OIL_LEVEL_OVER 40.0

int findPimples(const string &strImageName, const Mat &srcImg, Mat &imgMask)
{
	Mat bw;
	vectorContours vectorSpots;
#ifdef Use_SingleChannel
	Mat bgr[3];
	/* 将只有轮廓部分的图进行split通道分离 */
	split(imgMask, bgr);
	/* 这里取绿色通道 */
	bw = bgr[1];
#else
	/* 转换为灰度图的效果和上面取单一通道效果差不太多 */
	cvtColor(imgMask, bw, COLOR_BGR2GRAY);
#endif // Use_SingleChannel
	int pimplesCount = 0; // 找不到边界即设置为0

#ifdef With_Debug
	Mat matDebug;
	srcImg.copyTo(matDebug);
	namedWindow("自适应阈值化之前", WINDOW_NORMAL);
	imshow("自适应阈值化之前", bw);
#endif // With_Debug

	/* 自适应阈值化：图像分割，去除一定范围内的像素 */
	/* bw必须是单通道的8bit图像 */
	/* 第1个参数是输入图像，第2个参数是输出图像，第3个参数是满足条件的最大像素值，第4个参数是所用算法，
		第6个参数是用来计算阈值的块大小(必须是奇数)，第7个参数是需要从加权平均值减去的一个常量 */
	adaptiveThreshold(bw, bw, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 15, 5); // 目前调试这里使用15是最优的，可以再调试
	//adaptiveThreshold(bw, bw, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 13, 5);

#ifdef With_Debug
	namedWindow("自适应阈值化之后", WINDOW_NORMAL);
	imshow("自适应阈值化之后", bw);
#endif // With_Debug

	/* 膨胀操作：前两个参数是输入与输出；参数3：膨胀操作的核，NULL时为3*3；参数4：锚的位置，下面代表位于中心；参数5：迭代使用dilate的次数 */
	dilate(bw, bw, Mat(), Point(-1, -1), 1);
#ifdef With_Debug
	namedWindow("膨胀操作之后", WINDOW_NORMAL);
	imshow("膨胀操作之后", bw);
#endif // With_Debug

	/* 查找轮廓:必须是8位单通道图像，参数4：可以提取最外层及所有轮廓 */
	findContours(bw, vectorSpots, RETR_LIST, CHAIN_APPROX_SIMPLE);

	//LOG(INFO) << strImageName << ": Detected contours counts：" << to_string(vectorSpots.size());
	double areaSize = 0.0;
	for (size_t i = 0; i < vectorSpots.size(); ++i)	{
	    //LOG(INFO) << strImageName << ": Contour area size: " << to_string(fabs(contourArea(vectorSpots[i])));
		/* 这里的值也需要调试 */
		areaSize = fabs(contourArea(vectorSpots[i]));
		if (areaSize > MIN_SIZE_PIMPLES && areaSize < MAX_SIZE_PIMPLES) {
			//Rect minRect = boundingRect(Mat(vectorSpots[i]));
			Rect minRect = minAreaRect(Mat(vectorSpots[i])).boundingRect();
			/* 这里通过minAreaRect可能取到的矩形已经超出了图片边界，一般是最外的轮廓，所以要进行先处理 */
			if (minRect.x < 0)
				minRect.x = 0;
			if (minRect.y < 0)
				minRect.y = 0;
			if (minRect.x + minRect.width > imgMask.cols)
				minRect.width = imgMask.cols - minRect.x;
			if (minRect.y + minRect.height > imgMask.rows)
				minRect.height = imgMask.rows - minRect.y;
#ifndef WITH_SPOTS_AS_PIMPLES
			Mat maskCopy;
			imgMask.copyTo(maskCopy);
			Mat imgroi(maskCopy, minRect);

			int nShiftV = (minRect.height / 5 - 1) / 2; // adjustROI的参数所*的核是5pix*5pix
			int nShiftH = (minRect.width / 5 - 1) / 2;
			imgroi = imgroi.adjustROI(-nShiftV, -nShiftV, -nShiftH, -nShiftH); // top, bottom, left, right
			Mat imgroiHLS = imgroi.clone();
			cvtColor(imgroiHLS, imgroiHLS, COLOR_BGR2HLS);
			Scalar meanColorHLS = mean(imgroiHLS);
			Scalar meanColor = mean(imgroi);

			/* 这里根据颜色值进行一次过滤 */
			//if (color[0] < 10 & color[1] > 70 & color[2] > 50) { // HSV
			if (((int)(meanColorHLS[2]) >= MIN_COLOR_PIMPLES) && (meanColor[2] - meanColor[1] > MIN_COLOR_DIFF_G_R)) { // HLS的L值;RGB的G与B值差；

#endif // WITH_SPOTS_AS_PIMPLES
				double ratio = minRect.width * 1.0 / minRect.height;
				if ((ratio < MAX_RATIO) && (ratio > MIN_RATIO)) {
#ifdef With_Debug
					Point2f center;
					float radius = 0;
					minEnclosingCircle(Mat(vectorSpots[i]), center, radius); 

					if (1) {//(radius > 2 && radius < 50) {
						Mat matTest;
						srcImg.copyTo(matTest);
						string strSize = to_string(areaSize);
						//putText(matTest, format("c(%d:%d),s(%s)", (int)meanColorHLS[2], (int)(meanColor[2] - meanColor[1]), strSize.substr(0, 3).c_str()), cv::Point2f(20, 20), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 255), 1);
						putText(matTest, format("c(%d:%d),s(%s)", (int)meanColorHLS[2], (int)(meanColor[2] - meanColor[1]), strSize.substr(0, 3).c_str()), cv::Point2f(20, 20), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 0, 255), 2);
						rectangle(matTest, minRect, Scalar(0, 255, 0));
						circle(matTest, center, (int)(radius + 1), Scalar(0, 255, 0), 2, 8);
						namedWindow("当前斑点：", WINDOW_NORMAL);
						imshow("当前斑点：", matTest);
						waitKey();
					}
#endif // With_Debug		
					pimplesCount++;
				}
#ifndef WITH_SPOTS_AS_PIMPLES
			}
#endif // WITH_SPOTS_AS_PIMPLES
		}
	}

#ifdef With_Debug
	putText(matDebug, format("%d", pimplesCount), Point(20, 50), FONT_HERSHEY_SIMPLEX, 1.8, Scalar(0, 0, 255), 3);
	namedWindow("痘痘检测结果：", WINDOW_NORMAL);
	imshow("痘痘检测结果：", matDebug);
	waitKey();
#endif // With_Debug

	return pimplesCount;
}


int findBlackHeads(const string &strImageName, const Mat &srcImg, Mat &imgMask)
{
	Mat bw;
	vectorContours vectorSpots;
	cvtColor(imgMask, bw, COLOR_BGR2GRAY);
	int nBlackHeads = 0;

#ifdef With_Debug
	Mat matDebug;
	srcImg.copyTo(matDebug);
	namedWindow("自适应阈值化之前", WINDOW_NORMAL);
	imshow("自适应阈值化之前", bw);
#endif // With_Debug

	/* 自适应阈值化：图像分割，去除一定范围内的像素 */
	/* bw必须是单通道的8bit图像 */
	/* 第1个参数是输入图像，第2个参数是输出图像，第3个参数是满足条件的最大像素值，第4个参数是所用算法，
	第6个参数是用来计算阈值的块大小(必须是奇数)，第7个参数是需要从加权平均值减去的一个常量 */
	adaptiveThreshold(bw, bw, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 15, 5); // 目前调试这里使用15是最优的，可以再调试
	//adaptiveThreshold(bw, bw, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 13, 5);

#ifdef With_Debug
	namedWindow("自适应阈值化之后", WINDOW_NORMAL);
	imshow("自适应阈值化之后", bw);
#endif // With_Debug

	/* 膨胀操作：前两个参数是输入与输出；参数3：膨胀操作的核，NULL时为3*3；参数4：锚的位置，下面代表位于中心；参数5：迭代使用dilate的次数 */
	dilate(bw, bw, Mat(), Point(-1, -1), 1);
#ifdef With_Debug
	namedWindow("膨胀操作之后", WINDOW_NORMAL);
	imshow("膨胀操作之后", bw);
#endif // With_Debug

	/* 查找轮廓:必须是8位单通道图像，参数4：可以提取最外层及所有轮廓 */
	findContours(bw, vectorSpots, RETR_LIST, CHAIN_APPROX_SIMPLE);

	//LOG(INFO) << strImageName << ": Detected contours counts：" << to_string(vectorSpots.size());
	double areaSize = 0.0;
	for (size_t i = 0; i < vectorSpots.size(); ++i) {
		//LOG(INFO) << strImageName << ": Contour area size: " << to_string(fabs(contourArea(vectorSpots[i])));
		/* 这里的值也需要调试 */
		areaSize = fabs(contourArea(vectorSpots[i]));
		if (areaSize < MAX_SIZE_BLACKHEADS) {
			//Rect minRect = boundingRect(Mat(vectorSpots[i]));
			Rect minRect = minAreaRect(Mat(vectorSpots[i])).boundingRect();
			/* 这里通过minAreaRect可能取到的矩形已经超出了图片边界，一般是最外的轮廓，所以要进行先处理 */
			if (minRect.x < 0)
				minRect.x = 0;
			if (minRect.y < 0)
				minRect.y = 0;
			if (minRect.x + minRect.width > imgMask.cols)
				minRect.width = imgMask.cols - minRect.x;
			if (minRect.y + minRect.height > imgMask.rows)
				minRect.height = imgMask.rows - minRect.y;

			Mat maskCopy;
			imgMask.copyTo(maskCopy);
			Mat imgroi(maskCopy, minRect);

			//cvtColor(imgroi, imgroi, COLOR_BGR2HSV);
			Scalar color = mean(imgroi);

			/* 这里根据颜色值进行一次过滤 */
			if ((int)(color[0] + color[1] + color[2]) / 3 <= MAX_COLOR_BLACKHEADS) {
				double ratio = minRect.width * 1.0 / minRect.height;
				if ((ratio < MAX_RATIO) && (ratio > MIN_RATIO)) {
#ifdef With_Debug
					Point2f center;
					float radius = 0;
					minEnclosingCircle(Mat(vectorSpots[i]), center, radius);

					if (1) {//(radius > 2 && radius < 50) {
						Mat matTest;
						srcImg.copyTo(matTest);
						string strSize = to_string(areaSize);
						//putText(matTest, format("color(%d:%d:%d), areaSize(%s)", color[0], color[1], color[2], strSize.substr(0, 3).c_str()), cv::Point2f(center.x, center.y - radius), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255), 1);
						putText(matTest, format("color(%d:%d:%d-%d), areaSize(%s)", (int)color[0], (int)color[1], (int)color[2], (int)(color[0] + color[1] + color[2]) / 3, strSize.substr(0, 3).c_str()), cv::Point2f(20, 50), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 0, 255), 2);
						rectangle(matTest, minRect, Scalar(0, 255, 0));
						//circle(matTest, center, (int)(radius + 1), Scalar(0, 255, 0), 2, 8);
						namedWindow("当前黑头：", WINDOW_NORMAL);
						imshow("当前黑头：", matTest);
						waitKey();
					}
#endif // With_Debug		
					nBlackHeads++;
				}
			}
		}
	}

#ifdef With_Debug
	putText(matDebug, format("%d", nBlackHeads), Point(20, 50), FONT_HERSHEY_SIMPLEX, 1.8, Scalar(0, 0, 255), 3);
	namedWindow("黑头检测结果：", WINDOW_NORMAL);
	imshow("黑头检测结果：", matDebug);
	waitKey();
#endif // With_Debug

	return nBlackHeads;
}


float getMoistureAndOil(const string &strImageName, const Mat &srcImg, Mat &imgMask)
{
	int bins = 256;
	int histSize[] = {bins};
	int dims = 1;
	float range[] = {0, 255};
	const float *ranges[] = {range};
	MatND lightHist;
	int channelsLight[] = {1};
	int nTotalHigh = 0;
	int nTotalPoints = 0;
	float fPercent = 0.0;
	Mat bw;

	/* 这里将RGB颜色空间转换为HLS颜色空间，从而取白色的亮度，以此来作为反光度的参考 */
	cvtColor(imgMask, bw, COLOR_BGR2HLS);
	calcHist(&bw, 1, channelsLight, Mat(), lightHist, dims, histSize, ranges, true, false);

	for (int i = 0; i < 256; ++i) {
		float binValue = lightHist.at<float>(i);
		if (i >= MIN_COLOR_OIL) {
			/* 总的反光点 */
			nTotalHigh += (int)binValue;
		}
		/* 所有的点 */
		nTotalPoints += (int)binValue;
	}
	if (nTotalPoints) {
		/* 计算出反光点比例，近似于反光面积比例 */
		fPercent = nTotalHigh * 100.0 / nTotalPoints;
	}
	LOG(INFO) << "Oil percent: " << fPercent;
#if 0
	int size = 256;
	int scale = 1;
	Mat dstMat(size * scale, size, CV_8U, Scalar(0));
	double minValue = 0, maxValue = 0;
	minMaxLoc(lightHist, &minValue, &maxValue, 0, 0);
	LOG(INFO) << "MaxValue: " << maxValue;
	int hpt = saturate_cast<int>(0.9 * size);
	//LOG(INFO) << "######################" << strImageName;
	for (int i = 0; i < 256; ++i) {
		float binValue = lightHist.at<float>(i);
		LOG(INFO) << "i: " << i << ", value:" << binValue;
		int realValue = saturate_cast<int>(binValue * hpt / maxValue);
		rectangle(dstMat, Point(i * scale, size - 1), Point((i + 1) * scale - 1, size - realValue), Scalar(255));
	}
	//LOG(INFO) << "######################";
	imshow("油份反光直方图：", dstMat);
	waitKey(0);
#endif
	return fPercent;
}


bool findFaceSpots(const string &strImageName, const cv::Mat &matSrc, const vectorContours &faceContours, vectorInt &vectorIntResult)
{
	int nPimples = -1;
	int nBlackHeadsNose = 0;
	int nBlackHeadsFace = 0;
	float fOil[4] = {0.0}; // 左右脸颊，额头与鼻子（额头与鼻子最终组合成T区）

	for (int i = 0; i <= INDEX_CONTOUR_NOSE; ++i) {
		/* 创建一个通道并与原图大小相等的Mat */
		Mat mask(matSrc.size(), CV_8UC1);
		/* 矩阵赋值为全0，颜色表现为全黑 */
		mask = 0;
		/* 第一个参数是需要画轮廓的图像，第二个参数代表轮廓数组，第三个参数代表所用数组索引，第4个参数代表轮廓填充颜色，第5个参数是 */
		drawContours(mask, faceContours, i, Scalar(255), -1);
		/* 创建一个三通道的mask */
		Mat masked(matSrc.size(), CV_8UC3);
		masked = Scalar(255, 255, 255);
		/* 将画了轮廓的原图按照mask拷贝到masked；这里的mask只有轮廓部分颜色值是1，即只拷贝原图这块的内容到masked */
		matSrc.copyTo(masked, mask);
		/* imgSrc始终保持不变 */
		nPimples = findPimples(strImageName, matSrc, masked);
		/* 痘痘 */
		vectorIntResult[i] = nPimples;

		if (INDEX_CONTOUR_LEFT == i) {
			nBlackHeadsFace = findBlackHeads(strImageName, matSrc, masked);
			fOil[0] = getMoistureAndOil(strImageName, matSrc, masked);
		} else if (INDEX_CONTOUR_RIGHT == i) {
			nBlackHeadsFace += findBlackHeads(strImageName, matSrc, masked);
			fOil[1] = getMoistureAndOil(strImageName, matSrc, masked);
		}else if (INDEX_CONTOUR_FOREHEAD == i) {
			fOil[2] = getMoistureAndOil(strImageName, matSrc, masked);
		} else if (INDEX_CONTOUR_NOSE == i) {
			nBlackHeadsNose = findBlackHeads(strImageName, matSrc, masked);
			vectorIntResult[INDEX_VALUE_BLACKHEADS] = nBlackHeadsNose;
			fOil[3] = getMoistureAndOil(strImageName, matSrc, masked);
		}
	}

	/* 毛孔粗大度目前根据左右脸黑头的总数量来粗略评定 */
	if (nBlackHeadsFace >= NUMBER_PORE_ROUGH) {
		vectorIntResult[INDEX_VALUE_PORE_TYPE] = TYPE_SKIN_ROUGH;
	} else if (nBlackHeadsFace >= NUMBER_PORE_NORMAL) {
		vectorIntResult[INDEX_VALUE_PORE_TYPE] = TYPE_SKIN_NORMAL;
	} else {
		vectorIntResult[INDEX_VALUE_PORE_TYPE] = TYPE_SKIN_SMOOTH;
	}

	{
		/* oil and moisture */
		bool bWaterEnough[2] = {false}; // left+right,forehead+nose
		fOil[0] = (fOil[0] + fOil[1]) / 2; // both = (left + right) /2
		fOil[1] = (fOil[2] + fOil[3]) / 2; // T = (forehead + nose) / 2 
		for (int i = 0; i < 2; ++i) {
			if ((fOil[i] >= OIL_LEVEL_OVER) || (fOil[i] <= OIL_LEVEL_LOW) || (TYPE_SKIN_ROUGH == vectorIntResult[INDEX_VALUE_PORE_TYPE])) {
				/* 皮肤油脂分泌旺盛或分泌少，或者油脂分泌适中且皮肤纹理粗糙不规则均分类为水份不足状态 */
				bWaterEnough[i] = false;
			} else {
				bWaterEnough[i] = true;
			}
		}
		enumSkinOilType nOilTypeSingle[2] = { TYPE_OIL_LOW }; // 左、右及T区分别是油、干、中性的哪一种
		for (int i = 0; i < 2; ++i) {
			if ((fOil[i] >= OIL_LEVEL_OVER) && (!bWaterEnough[i])) {
				/* 油脂分泌旺盛，肌肤水份不足 */
				nOilTypeSingle[i] = TYPE_OIL_OVER;
			} else if ((fOil[i] < OIL_LEVEL_OVER) && (fOil[i]) > OIL_LEVEL_LOW && (!bWaterEnough[i])) {
				/* 油脂分泌适中，肌肤水份不足 */
				nOilTypeSingle[i] = TYPE_OIL_LOW;
			} else if ((fOil[i] < OIL_LEVEL_OVER) && (fOil[i]) > OIL_LEVEL_LOW && (bWaterEnough[i])) {
				/* 油脂分泌适中，肌肤水份充足 */
				nOilTypeSingle[i] = TYPE_OIL_BALANCE;
			} else if ((fOil[i]) <= OIL_LEVEL_LOW && (bWaterEnough[i])) {
				/* 油脂分泌少，肌肤水份充足 */
				nOilTypeSingle[i] = TYPE_OIL_BALANCE;
			} else if ((fOil[i]) <= OIL_LEVEL_LOW && (!bWaterEnough[i])) {
				/* 油脂分泌少，肌肤水份不足 */
				nOilTypeSingle[i] = TYPE_OIL_LOW;
			} else {
				nOilTypeSingle[i] = TYPE_OIL_BALANCE;
			}
		}

		if ((nOilTypeSingle[0] + nOilTypeSingle[1]) <= TYPE_OIL_BALANCE) {
			/* 干性: 两颊和T区均偏干或一个中性一个偏干 */
			vectorIntResult[INDEX_VALUE_OIL_TYPE] = TYPE_OIL_LOW;
		} else if ((TYPE_OIL_BALANCE == nOilTypeSingle[0]) && (TYPE_OIL_BALANCE == nOilTypeSingle[1])) {
			/* 中性：两颊和T区均为是中性 */
			vectorIntResult[INDEX_VALUE_OIL_TYPE] = TYPE_OIL_BALANCE;
		} else if ((nOilTypeSingle[0] + nOilTypeSingle[1]) >= (TYPE_OIL_BALANCE + TYPE_OIL_OVER)) {
			/* 两颊和T区均偏油或一个中性一个偏油 */
			vectorIntResult[INDEX_VALUE_OIL_TYPE] = TYPE_OIL_OVER;
		} else {
			/* 两颊和T区一个偏干，一个偏油 */
			vectorIntResult[INDEX_VALUE_OIL_TYPE] = TYPE_OIL_MIX;
		}

	}

#ifdef WITH_SPOTS_AS_PIMPLES
	/* 这里暂时对额头和下巴的脏数据进行简单处理 */
	if (vectorIntResult[INDEX_CONTOUR_FOREHEAD] > 3) {
		vectorIntResult[INDEX_CONTOUR_FOREHEAD] = 0;
	}
	if (vectorIntResult[INDEX_CONTOUR_JAW] > 3) {
		vectorIntResult[INDEX_CONTOUR_JAW] = 0;
	}
#endif // WITH_SPOTS_AS_PIMPLES

#ifdef With_Debug
	Mat matDebug;
	const string strPoreTypes[] = {"smooth", "normal", "rough"};
	matSrc.copyTo(matDebug);
	putText(matDebug, format("%s(%d)", strPoreTypes[vectorIntResult[INDEX_VALUE_PORE_TYPE]].c_str(), nBlackHeadsFace), Point(20, 50), FONT_HERSHEY_SIMPLEX, 1.8, Scalar(0, 0, 255), 3);
	namedWindow("毛孔检测结果：", WINDOW_NORMAL);
	imshow("毛孔检测结果：", matDebug);
	waitKey();
#endif // With_Debug

	return true;
}
