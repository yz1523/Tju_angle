/**
* This file is part of ORB-SLAM3
*
* Copyright (C) 2017-2020 Carlos Campos, Richard Elvira, Juan J. Gómez Rodríguez, José M.M. Montiel and Juan D. Tardós, University of Zaragoza.
* Copyright (C) 2014-2016 Raúl Mur-Artal, José M.M. Montiel and Juan D. Tardós, University of Zaragoza.
*
* ORB-SLAM3 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM3 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
* the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with ORB-SLAM3.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include<iostream>
#include<algorithm>
#include<fstream>
#include<iomanip>
#include<chrono>

#include<opencv2/core/core.hpp>

#include<System.h>

using namespace std;

void LoadImages(const string &strPathLeft, const string &strPathRight, const string &strPathTimes,
                vector<string> &vstrImageLeft, vector<string> &vstrImageRight, vector<double> &vTimeStamps);

int LoadiGPSDirection(vector<double> &vTimestampsiGPS,ORB_SLAM3::iGPS::Direction* iGPSDirection,const string &strSettingsFile);

int LoadFTDirection(vector<double> &vTimestampsiGPS,ORB_SLAM3::iGPS::Direction* iGPSDirection,const string &strSettingsFile);

int LoadCamPose(vector<double> &vTimeStampsGT,vector<Eigen::VectorXf> &vCameraPose,const string &strFilePath);

int main(int argc, char **argv)
{  
    if(argc < 5)
    {
        cerr << endl << "Usage: ./stereo_euroc path_to_vocabulary path_to_settings path_to_sequence_folder_1 path_to_times_file_1 (path_to_image_folder_2 path_to_times_file_2 ... path_to_image_folder_N path_to_times_file_N) (trajectory_file_name)" << endl;

        return 1;
    }

    const int num_seq = (argc-3)/2;
    cout << "num_seq = " << num_seq << endl;
    bool bFileName= (((argc-3) % 2) == 1);
    string file_name;
    if (bFileName)
    {
        file_name = string(argv[argc-1]);
        cout << "file name: " << file_name << endl;
    }

    // Load all sequences:
    int seq;
    vector< vector<string> > vstrImageLeft;
    vector< vector<string> > vstrImageRight;
    vector< vector<double> > vTimestampsCam;
    vector<int> nImages;

    vstrImageLeft.resize(num_seq);
    vstrImageRight.resize(num_seq);
    vTimestampsCam.resize(num_seq);
    nImages.resize(num_seq);

    int tot_images = 0;
    for (seq = 0; seq<num_seq; seq++)
    {
        cout << "Loading images for sequence " << seq << "...";

        string pathSeq(argv[(2*seq) + 3]);
        string pathTimeStamps(argv[(2*seq) + 4]);

        string pathCam0 = pathSeq + "/mav0/cam0/data";
        string pathCam1 = pathSeq + "/mav0/cam1/data";

        LoadImages(pathCam0, pathCam1, pathTimeStamps, vstrImageLeft[seq], vstrImageRight[seq], vTimestampsCam[seq]);
        cout << "LOADED!" << endl;

        nImages[seq] = vstrImageLeft[seq].size();
        tot_images += nImages[seq];
    }

    // Read rectification parameters
    cv::FileStorage fsSettings(argv[2], cv::FileStorage::READ);
    if(!fsSettings.isOpened())
    {
        cerr << "ERROR: Wrong path to settings" << endl;
        return -1;
    }

    cv::Mat K_l, K_r, P_l, P_r, R_l, R_r, D_l, D_r;
    fsSettings["LEFT.K"] >> K_l;
    fsSettings["RIGHT.K"] >> K_r;

    fsSettings["LEFT.P"] >> P_l;
    fsSettings["RIGHT.P"] >> P_r;

    fsSettings["LEFT.R"] >> R_l;
    fsSettings["RIGHT.R"] >> R_r;

    fsSettings["LEFT.D"] >> D_l;
    fsSettings["RIGHT.D"] >> D_r;

    int rows_l = fsSettings["LEFT.height"];
    int cols_l = fsSettings["LEFT.width"];
    int rows_r = fsSettings["RIGHT.height"];
    int cols_r = fsSettings["RIGHT.width"];

    if(K_l.empty() || K_r.empty() || P_l.empty() || P_r.empty() || R_l.empty() || R_r.empty() || D_l.empty() || D_r.empty() ||
            rows_l==0 || rows_r==0 || cols_l==0 || cols_r==0)
    {
        cerr << "ERROR: Calibration parameters to rectify stereo are missing!" << endl;
        return -1;
    }

    cv::Mat M1l,M2l,M1r,M2r;
    cv::initUndistortRectifyMap(K_l,D_l,R_l,P_l.rowRange(0,3).colRange(0,3),cv::Size(cols_l,rows_l),CV_32F,M1l,M2l);
    cv::initUndistortRectifyMap(K_r,D_r,R_r,P_r.rowRange(0,3).colRange(0,3),cv::Size(cols_r,rows_r),CV_32F,M1r,M2r);

    ORB_SLAM3::iGPS::Direction* iGPSDirection = new ORB_SLAM3::iGPS::Direction[50000];
    vector<double> vTimestampsiGPSDir;
    int ret = LoadFTDirection(vTimestampsiGPSDir,iGPSDirection,argv[2]);
    if(1 == ret)
    {
        cout << "Read fsSettings fails";
        return 1;
    }
    else if(2 == ret)
    {
        cout<< "Read iGPSDataFile fails";
        return 1;
    }

    const string strFilePath = "./E/E01.txt";   //GT File Path
    cout << "strFilePath = " << strFilePath <<endl;
    vector<double> vTimeStampsGT; //Camera Pose TimeStamps
    vector<Eigen::VectorXf> vCameraPose;
    ret = LoadCamPose(vTimeStampsGT,vCameraPose,strFilePath);   //Notice!: EuRoC GT form is different from Tum, code needs change.
    if(2 == ret)
    {
        cout<< "Read CamPoseFile fails";
        return 1;
    }

    // Vector for tracking time statistics
    vector<float> vTimesTrack;
    vTimesTrack.resize(tot_images);

    cout << endl << "-------" << endl;
    cout.precision(17);

    // Create SLAM system. It initializes all system threads and gets ready to process frames.
    ORB_SLAM3::System SLAM(argv[1],argv[2],ORB_SLAM3::System::STEREO, true);

    if(!vTimestampsiGPSDir.empty())
        SLAM.LoadiGPSDirection(vTimestampsiGPSDir,iGPSDirection);

    if(!vTimeStampsGT.empty())
        SLAM.LoadCameraPose(vTimeStampsGT,vCameraPose);

    cv::Mat imLeft, imRight, imLeftRect, imRightRect;
    for (seq = 0; seq<num_seq; seq++)
    {

        // Seq loop

        double t_rect = 0;
        double t_track = 0;
        int num_rect = 0;
        int proccIm = 0;
        for(int ni=0; ni<nImages[seq]; ni++, proccIm++)
        {
            // Read left and right images from file
            imLeft = cv::imread(vstrImageLeft[seq][ni],cv::IMREAD_UNCHANGED);
            imRight = cv::imread(vstrImageRight[seq][ni],cv::IMREAD_UNCHANGED);

            if(imLeft.empty())
            {
                cerr << endl << "Failed to load image at: "
                     << string(vstrImageLeft[seq][ni]) << endl;
                return 1;
            }

            if(imRight.empty())
            {
                cerr << endl << "Failed to load image at: "
                     << string(vstrImageRight[seq][ni]) << endl;
                return 1;
            }

#ifdef REGISTER_TIMES
    #ifdef COMPILEDWITHC11
            std::chrono::steady_clock::time_point t_Start_Rect = std::chrono::steady_clock::now();
    #else
            std::chrono::monotonic_clock::time_point t_Start_Rect = std::chrono::monotonic_clock::now();
    #endif
#endif
            cv::remap(imLeft,imLeftRect,M1l,M2l,cv::INTER_LINEAR);
            cv::remap(imRight,imRightRect,M1r,M2r,cv::INTER_LINEAR);

#ifdef REGISTER_TIMES
    #ifdef COMPILEDWITHC11
            std::chrono::steady_clock::time_point t_End_Rect = std::chrono::steady_clock::now();
    #else
            std::chrono::monotonic_clock::time_point t_End_Rect = std::chrono::monotonic_clock::now();
    #endif
            t_rect = std::chrono::duration_cast<std::chrono::duration<double,std::milli> >(t_End_Rect - t_Start_Rect).count();
            SLAM.InsertRectTime(t_rect);
#endif
            double tframe = vTimestampsCam[seq][ni];
    #ifdef COMPILEDWITHC11
            std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    #else
            std::chrono::monotonic_clock::time_point t1 = std::chrono::monotonic_clock::now();
    #endif

            // Pass the images to the SLAM system
            SLAM.TrackStereo(imLeftRect,imRightRect,tframe, vector<ORB_SLAM3::IMU::Point>(), vstrImageLeft[seq][ni]);

    #ifdef COMPILEDWITHC11
            std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
    #else
            std::chrono::monotonic_clock::time_point t2 = std::chrono::monotonic_clock::now();
    #endif

#ifdef REGISTER_TIMES
            t_track = t_rect + std::chrono::duration_cast<std::chrono::duration<double,std::milli> >(t2 - t1).count();
            SLAM.InsertTrackTime(t_track);
#endif

            double ttrack= std::chrono::duration_cast<std::chrono::duration<double> >(t2 - t1).count();

            vTimesTrack[ni]=ttrack;

            // Wait to load the next frame
            double T=0;
            if(ni<nImages[seq]-1)
                T = vTimestampsCam[seq][ni+1]-tframe;
            else if(ni>0)
                T = tframe-vTimestampsCam[seq][ni-1];

            if(ttrack<T)
                usleep((T-ttrack)*1e6);
        }

        if(seq < num_seq - 1)
        {
            cout << "Changing the dataset" << endl;

            SLAM.ChangeDataset();
        }

    }
    // Stop all threads
    SLAM.Shutdown();

    // Save camera trajectory
    if (bFileName)
    {
        const string kf_file =  "kf_" + string(argv[argc-1]) + ".txt";
        const string f_file =  "f_" + string(argv[argc-1]) + ".txt";
        SLAM.SaveTrajectoryEuRoC(f_file);
        SLAM.SaveKeyFrameTrajectoryEuRoC(kf_file);
    }
    else
    {
        SLAM.SaveTrajectoryEuRoC("CameraTrajectory.txt");
        SLAM.SaveKeyFrameTrajectoryEuRoC("KeyFrameTrajectory.txt");
    }

    return 0;
}

void LoadImages(const string &strPathLeft, const string &strPathRight, const string &strPathTimes,
                vector<string> &vstrImageLeft, vector<string> &vstrImageRight, vector<double> &vTimeStamps)
{
    ifstream fTimes;
    fTimes.open(strPathTimes.c_str());
    vTimeStamps.reserve(5000);
    vstrImageLeft.reserve(5000);
    vstrImageRight.reserve(5000);
    while(!fTimes.eof())
    {
        string s;
        getline(fTimes,s);
        if(!s.empty())
        {
            stringstream ss;
            ss << s;
            vstrImageLeft.push_back(strPathLeft + "/" + ss.str() + ".png");
            vstrImageRight.push_back(strPathRight + "/" + ss.str() + ".png");
            double t;
            ss >> t;
            vTimeStamps.push_back(t/1e9);

        }
    }
}


int LoadCamPose(vector<double> &vTimeStampsGT,vector<Eigen::VectorXf> &vCameraPose,const string &strFilePath)
{
    // Load Camera pose file
    ifstream fTimes;
    fTimes.open(strFilePath);

    if(!fTimes)
        return 2;

    long index = 0;

    while(!fTimes.eof()) {

        string s;
        getline(fTimes, s);
        if(s[0] == '#')
            continue;

        if (!s.empty())
        {
            stringstream ss;
            ss << s;

            double t, x, y, z, qw, qx, qy, qz;  // Frame
            Eigen::VectorXf CameraPose(7);

            /* If EuRoC dataset*/
            //ss >> t; ss >> x; ss >> y; ss >> z; ss >> qw; ss >> qx; ss >> qy; ss >> qz;

            /*
             * If FT dataset*/
            ss >> t; ss >> x; ss >> y; ss >> z; ss >> qx; ss >> qy; ss >> qz; ss >> qw;   //Tum form

            //cout << "time tx ty tz qx qy qz qw= " << t << " " << tx << " " << ty << " " << tz << " " << qx << " " << qy << " " << qz << " " << qw << " " << tx <<endl;
            CameraPose <<x,y,z,qx,qy,qz,qw;
            vTimeStampsGT.push_back(t);
            vCameraPose.push_back(CameraPose);
        }

        index++;

    }
    cout << "end read iGPS data" << endl;
    return 0;
}

//iGPSDirection - ChannelTransmitterTimeDirection
int LoadiGPSDirection(vector<double> &vTimestampsiGPS,ORB_SLAM3::iGPS::Direction* iGPSDirection,const string &strSettingsFile)
{
    int nChannel,nRotationSpeed;
    string nPath;
    cv::FileStorage fSettings(strSettingsFile, cv::FileStorage::READ);
    if(fSettings.isOpened())
    {
        cv::FileNode node = fSettings["iGPS.Channel"];
        if(!node.empty() && node.isInt())
        {
            nChannel = node.operator int();
        }
        node = fSettings["iGPS.RotationSpeed"];
        if(!node.empty() && node.isInt())
        {
            nRotationSpeed = node.operator int();
        }
        node = fSettings["iGPS.Path"];
        if(!node.empty() && node.isString())
        {
            node>> nPath;
            cout << " nPath = " << nPath <<endl;
        }
    }
    else
        return 1;
    //cout << "nChannel = " << nChannel<< endl;
    //cout << "nRotationSpeed = "  << nRotationSpeed << endl;
    cout << "iGPS Data FilePath: " << nPath << endl;
    cout << "start loading igps" << endl;
    //vTimestampsiGPS.reserve(5000);
    ifstream fTimes;
    fTimes.open(nPath);
    if(!fTimes)
        return 2;
    long index = 0;
    while(!fTimes.eof()) {

        string s;
        getline(fTimes, s);
        if(s[0] == '#')
            continue;

        if (!s.empty())
        {
            stringstream ss;
            ss << s;

            double trm, t, d1, d2, d3, a1, a2;  // Frame
            int ch;
            //ss >> ch; ss >> trm;
            ss >> t; ss>> ch; ss >> d1; ss >> d2; ss >> d3; ss >> a1; ss >> a2;  //Euler Angle

            iGPSDirection[index].channel = ch;
            iGPSDirection[index].transmitter = nRotationSpeed;
            iGPSDirection[index].time = t;
            iGPSDirection[index].dir = Eigen::Vector3d(d1,d2,d3);
            iGPSDirection[index].dirAngle = Eigen::Vector2d(a1,a2);
            vTimestampsiGPS.push_back(t);
            //vTimestampsiGPS.push_back(t);
            //iGPSPosition.push_back(PositionCoord);
        }
        index++;
    }
    cout << "end read iGPS data" << endl;
    //for(int i = 0 ; i < 5000 ; i ++ )
    //cout << "iGPSDirection = " << iGPSDirection[i].channel << " "<< iGPSDirection[i].transmitter << " " << iGPSDirection[i].time << " " << iGPSDirection[i].dir.transpose() << " " << iGPSDirection[i].dirAngle.transpose() <<endl;
    return 0;
}

//iGPSDirection - ChannelTransmitterTimeDirection
int LoadFTDirection(vector<double> &vTimestampsiGPS,ORB_SLAM3::iGPS::Direction* iGPSDirection,const string &strSettingsFile)
{
    int nChannel,nRotationSpeed;
    string nPath;
    cv::FileStorage fSettings(strSettingsFile, cv::FileStorage::READ);
    if(fSettings.isOpened())
    {
        cv::FileNode node = fSettings["iGPS.Channel"];
        if(!node.empty() && node.isInt())
        {
            nChannel = node.operator int();
        }
        node = fSettings["iGPS.RotationSpeed"];
        if(!node.empty() && node.isInt())
        {
            nRotationSpeed = node.operator int();
        }
        node = fSettings["iGPS.Path"];
        if(!node.empty() && node.isString())
        {
            node>> nPath;
            cout << " nPath = " << nPath <<endl;
        }
    }
    else
        return 1;
    //cout << "nChannel = " << nChannel<< endl;
    //cout << "nRotationSpeed = "  << nRotationSpeed << endl;
    cout << "iGPS Data FilePath: " << nPath << endl;
    cout << "start loading igps" << endl;
    //vTimestampsiGPS.reserve(5000);
    ifstream fTimes;
    fTimes.open(nPath);
    if(!fTimes)
        return 2;
    long index = 0;
    while(!fTimes.eof()) {

        string s;
        getline(fTimes, s);
        if(s[0] == '#')
            continue;

        if (!s.empty())
        {
            stringstream ss;
            ss << s;

            double trm, t, d1, d2, d3, a1, a2;  // Frame
            int ch;
            //ss >> ch; ss >> trm;
            ss >> ch; ss>> trm; ss >> t;  ss>>d1; ss >> d2; ss >> d3;
            a1 = 0.0;
            a2 = 0.0;
            iGPSDirection[index].channel = ch;
            iGPSDirection[index].transmitter = trm;
            iGPSDirection[index].time = t;
            iGPSDirection[index].dir = Eigen::Vector3d(d1,d2,d3);
            iGPSDirection[index].dirAngle = Eigen::Vector2d(a1,a2);

            vTimestampsiGPS.push_back(t);
            //iGPSPosition.push_back(PositionCoord);
        }
        index++;
    }
    cout << "end read iGPS data" << endl;
    //for(int i = 0 ; i < 20000 ; i ++ )
    //    cout << "iGPSDirection = " << iGPSDirection[i].channel << " "<< iGPSDirection[i].transmitter << " " << iGPSDirection[i].time << " " << iGPSDirection[i].dir.transpose() << " " << iGPSDirection[i].dirAngle.transpose() <<endl;
    return 0;
}
