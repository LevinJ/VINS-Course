
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <thread>
#include <iomanip>

#include <cv.h>
#include <opencv2/opencv.hpp>
#include <highgui.h>
#include <eigen3/Eigen/Dense>
#include "System.h"

using namespace std;
using namespace cv;
using namespace Eigen;

const int nDelayTimes = 2;
string sData_path = "/home/dataset/EuRoC/MH-05/mav0/";
string sConfig_path = "../config/";

std::shared_ptr<System> pSystem;

void PubImuData()
{
	bool use_noisy_data = true;
	string sImu_data_file;
	if(use_noisy_data){
		sImu_data_file = sData_path + "imu_pose_noise.txt";
	}else{
		sImu_data_file = sData_path + "imu_pose.txt";
	}

	cout << "1 PubImuData start sImu_data_filea: " << sImu_data_file << endl;
	ifstream fsImu;
	fsImu.open(sImu_data_file.c_str());
	if (!fsImu.is_open())
	{
		cerr << "Failed to open imu file! " << sImu_data_file << endl;
		return;
	}

	std::string sImu_line;
	double dStampNSec = 0.0;
	Vector3d vAcc;
	Vector3d vGyr;
	Vec3 P;   // pose
	Qd   Q;   // Rotation
	while (std::getline(fsImu, sImu_line) && !sImu_line.empty()) // read imu data
	{
		std::istringstream ssImuData(sImu_line);
		ssImuData >> dStampNSec;
		ssImuData>>Q.w()>>Q.x()>>Q.y()>>Q.z();
		ssImuData>>P.x()>>P.y()>>P.z();
		ssImuData >> vGyr.x() >> vGyr.y() >> vGyr.z() >> vAcc.x() >> vAcc.y() >> vAcc.z();
		// cout << "Imu t: " << fixed << dStampNSec << " gyr: " << vGyr.transpose() << " acc: " << vAcc.transpose() << endl;
		pSystem->PubImuData(dStampNSec , vGyr, vAcc);
		usleep(5000*nDelayTimes);
	}
	fsImu.close();
}

void PubImageData()
{
	string sImage_file = sData_path + "cam_pose_tum.txt";

	cout << "1 PubImageData start sImage_file: " << sImage_file << endl;

	ifstream fsImage;
	fsImage.open(sImage_file.c_str());
	if (!fsImage.is_open())
	{
		cerr << "Failed to open image file! " << sImage_file << endl;
		return;
	}

	std::string sImage_line;
	double dStampNSec;
	
	// cv::namedWindow("SOURCE IMAGE", CV_WINDOW_AUTOSIZE);
	int img_id = 0;
	while (std::getline(fsImage, sImage_line) && !sImage_line.empty())
	{
		std::istringstream ssImuData(sImage_line);
		ssImuData >> dStampNSec;

		std::ostringstream stringStream;
		stringStream << sData_path<<"keyframe/all_points_"<<img_id++<<".txt";
		string imagePath =  stringStream.str();
		cout << "Image t : " << fixed << dStampNSec << " Name: " << imagePath << endl;


		pSystem->PubImageData_sim(dStampNSec, imagePath);
		// cv::imshow("SOURCE IMAGE", img);
		// cv::waitKey(0);
		usleep(50000*nDelayTimes);
	}
	fsImage.close();
}

#ifdef __APPLE__
// support for MacOS
void DrawIMGandGLinMainThrd(){
	string sImage_file = sConfig_path + "MH_05_cam0.txt";

	cout << "1 PubImageData start sImage_file: " << sImage_file << endl;

	ifstream fsImage;
	fsImage.open(sImage_file.c_str());
	if (!fsImage.is_open())
	{
		cerr << "Failed to open image file! " << sImage_file << endl;
		return;
	}

	std::string sImage_line;
	double dStampNSec;
	string sImgFileName;

	pSystem->InitDrawGL();
	while (std::getline(fsImage, sImage_line) && !sImage_line.empty())
	{
		std::istringstream ssImuData(sImage_line);
		ssImuData >> dStampNSec >> sImgFileName;
		// cout << "Image t : " << fixed << dStampNSec << " Name: " << sImgFileName << endl;
		string imagePath = sData_path + "cam0/data/" + sImgFileName;

		Mat img = imread(imagePath.c_str(), 0);
		if (img.empty())
		{
			cerr << "image is empty! path: " << imagePath << endl;
			return;
		}
		//pSystem->PubImageData(dStampNSec / 1e9, img);
		cv::Mat show_img;
		cv::cvtColor(img, show_img, CV_GRAY2RGB);
		if (SHOW_TRACK)
		{
			for (unsigned int j = 0; j < pSystem->trackerData[0].cur_pts.size(); j++)
			{
				double len = min(1.0, 1.0 *  pSystem->trackerData[0].track_cnt[j] / WINDOW_SIZE);
				cv::circle(show_img,  pSystem->trackerData[0].cur_pts[j], 2, cv::Scalar(255 * (1 - len), 0, 255 * len), 2);
			}

			cv::namedWindow("IMAGE", CV_WINDOW_AUTOSIZE);
			cv::imshow("IMAGE", show_img);
		  // cv::waitKey(1);
		}

		pSystem->DrawGLFrame();
		usleep(50000*nDelayTimes);
	}
	fsImage.close();

} 
#endif

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		cerr << "./run_sim PATH_TO_FOLDER/MH-05/mav0 PATH_TO_CONFIG/config \n"
			<< "For example: ./run_sim /home/stevencui/dataset/EuRoC/MH-05/mav0/ ../config/"<< endl;
		return -1;
	}
	sData_path = argv[1];
	sConfig_path = argv[2];

	pSystem.reset(new System(sConfig_path + "sim_config.yaml"));
	
	std::thread thd_BackEnd(&System::ProcessBackEnd, pSystem);
		
	// sleep(5);
	std::thread thd_PubImuData(PubImuData);

	std::thread thd_PubImageData(PubImageData);

#ifdef __linux__	
	std::thread thd_Draw(&System::Draw, pSystem);
#elif __APPLE__
	DrawIMGandGLinMainThrd();
#endif

	thd_PubImuData.join();
	thd_PubImageData.join();

	// thd_BackEnd.join();
#ifdef __linux__	
	thd_Draw.join();
#endif

	cout << "main end... see you ..." << endl;
	return 0;
}
