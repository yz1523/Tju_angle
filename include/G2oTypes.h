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


#ifndef G2OTYPES_H
#define G2OTYPES_H

#include "Thirdparty/g2o/g2o/core/base_vertex.h"
#include "Thirdparty/g2o/g2o/core/base_binary_edge.h"
#include "Thirdparty/g2o/g2o/types/types_sba.h"
#include "Thirdparty/g2o/g2o/core/base_multi_edge.h"
#include "Thirdparty/g2o/g2o/core/base_unary_edge.h"

#include<opencv2/core/core.hpp>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/Dense>

#include <Frame.h>
#include <KeyFrame.h>

#include"Converter.h"
#include <math.h>

namespace ORB_SLAM3
{

class KeyFrame;
class Frame;
class GeometricCamera;

typedef Eigen::Matrix<double, 6, 1> Vector6d;
typedef Eigen::Matrix<double, 9, 1> Vector9d;
typedef Eigen::Matrix<double, 12, 1> Vector12d;
typedef Eigen::Matrix<double, 15, 1> Vector15d;
typedef Eigen::Matrix<double, 12, 12> Matrix12d;
typedef Eigen::Matrix<double, 15, 15> Matrix15d;
typedef Eigen::Matrix<double, 9, 9> Matrix9d;

Eigen::Matrix3d ExpSO3(const double x, const double y, const double z);
Eigen::Matrix3d ExpSO3(const Eigen::Vector3d &w);

Eigen::Vector3d LogSO3(const Eigen::Matrix3d &R);

Eigen::Matrix3d InverseRightJacobianSO3(const Eigen::Vector3d &v);
Eigen::Matrix3d RightJacobianSO3(const Eigen::Vector3d &v);
Eigen::Matrix3d RightJacobianSO3(const double x, const double y, const double z);

Eigen::Matrix3d Skew(const Eigen::Vector3d &w);
Eigen::Matrix3d InverseRightJacobianSO3(const double x, const double y, const double z);

Eigen::Matrix3d NormalizeRotation(const Eigen::Matrix3d &R);


class ImuCamPose
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    ImuCamPose(){}
    ImuCamPose(KeyFrame* pKF);
    ImuCamPose(Frame* pF);
    ImuCamPose(Eigen::Matrix3d &_Rwc, Eigen::Vector3d &_twc, KeyFrame* pKF);

    void SetParam(const std::vector<Eigen::Matrix3d> &_Rcw, const std::vector<Eigen::Vector3d> &_tcw, const std::vector<Eigen::Matrix3d> &_Rbc,
                  const std::vector<Eigen::Vector3d> &_tbc, const double &_bf);

    void Update(const double *pu); // update in the imu reference
    void UpdateW(const double *pu); // update in the world reference
    Eigen::Vector2d Project(const Eigen::Vector3d &Xw, int cam_idx=0) const; // Mono
    Eigen::Vector3d ProjectStereo(const Eigen::Vector3d &Xw, int cam_idx=0) const; // Stereo
    bool isDepthPositive(const Eigen::Vector3d &Xw, int cam_idx=0) const;

public:
    // For IMU
    Eigen::Matrix3d Rwb;
    Eigen::Vector3d twb;

    // For set of cameras
    std::vector<Eigen::Matrix3d> Rcw;
    std::vector<Eigen::Vector3d> tcw;
    std::vector<Eigen::Matrix3d> Rcb, Rbc;
    std::vector<Eigen::Vector3d> tcb, tbc;
    double bf;
    std::vector<GeometricCamera*> pCamera;

    // For posegraph 4DoF
    Eigen::Matrix3d Rwb0;
    Eigen::Matrix3d DR;

    int its;
};

class InvDepthPoint
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    InvDepthPoint(){}
    InvDepthPoint(double _rho, double _u, double _v, KeyFrame* pHostKF);

    void Update(const double *pu);

    double rho;
    double u, v; // they are not variables, observation in the host frame

    double fx, fy, cx, cy, bf; // from host frame

    int its;
};

// Optimizable parameters are IMU pose
class VertexPose : public g2o::BaseVertex<6,ImuCamPose>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    VertexPose(){}
    VertexPose(KeyFrame* pKF){
        setEstimate(ImuCamPose(pKF));
    }
    VertexPose(Frame* pF){
        setEstimate(ImuCamPose(pF));
    }


    virtual bool read(std::istream& is);
    virtual bool write(std::ostream& os) const;

    virtual void setToOriginImpl() {
        }

    virtual void oplusImpl(const double* update_){
        _estimate.Update(update_);
        updateCache();
    }
};

class VertexPose4DoF : public g2o::BaseVertex<4,ImuCamPose>
{
    // Translation and yaw are the only optimizable variables
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    VertexPose4DoF(){}
    VertexPose4DoF(KeyFrame* pKF){
        setEstimate(ImuCamPose(pKF));
    }
    VertexPose4DoF(Frame* pF){
        setEstimate(ImuCamPose(pF));
    }
    VertexPose4DoF(Eigen::Matrix3d &_Rwc, Eigen::Vector3d &_twc, KeyFrame* pKF){

        setEstimate(ImuCamPose(_Rwc, _twc, pKF));
    }

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    virtual void setToOriginImpl() {
        }

    virtual void oplusImpl(const double* update_){
        double update6DoF[6];
        update6DoF[0] = 0;
        update6DoF[1] = 0;
        update6DoF[2] = update_[0];
        update6DoF[3] = update_[1];
        update6DoF[4] = update_[2];
        update6DoF[5] = update_[3];
        _estimate.UpdateW(update6DoF);
        updateCache();
    }
};

class VertexVelocity : public g2o::BaseVertex<3,Eigen::Vector3d>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    VertexVelocity(){}
    VertexVelocity(KeyFrame* pKF);
    VertexVelocity(Frame* pF);

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    virtual void setToOriginImpl() {
        }

    virtual void oplusImpl(const double* update_){
        Eigen::Vector3d uv;
        uv << update_[0], update_[1], update_[2];
        setEstimate(estimate()+uv);
    }
};

class VertexGyroBias : public g2o::BaseVertex<3,Eigen::Vector3d>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    VertexGyroBias(){}
    VertexGyroBias(KeyFrame* pKF);
    VertexGyroBias(Frame* pF);

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    virtual void setToOriginImpl() {
        }

    virtual void oplusImpl(const double* update_){
        Eigen::Vector3d ubg;
        ubg << update_[0], update_[1], update_[2];
        setEstimate(estimate()+ubg);
    }
};


class VertexAccBias : public g2o::BaseVertex<3,Eigen::Vector3d>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    VertexAccBias(){}
    VertexAccBias(KeyFrame* pKF);
    VertexAccBias(Frame* pF);

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    virtual void setToOriginImpl() {
        }

    virtual void oplusImpl(const double* update_){
        Eigen::Vector3d uba;
        uba << update_[0], update_[1], update_[2];
        setEstimate(estimate()+uba);
    }
};


// Gravity direction vertex
class GDirection
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    GDirection(){}
    GDirection(Eigen::Matrix3d pRwg): Rwg(pRwg){}

    void Update(const double *pu)
    {
        Rwg=Rwg*ExpSO3(pu[0],pu[1],0.0);
    }

    Eigen::Matrix3d Rwg, Rgw;

    int its;
};

class VertexGDir : public g2o::BaseVertex<2,GDirection>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    VertexGDir(){}
    VertexGDir(Eigen::Matrix3d pRwg){
        setEstimate(GDirection(pRwg));
    }

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    virtual void setToOriginImpl() {
        }

    virtual void oplusImpl(const double* update_){
        _estimate.Update(update_);
        updateCache();
    }
};

// scale vertex
class VertexScale : public g2o::BaseVertex<1,double>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    VertexScale(){
        setEstimate(1.0);
    }
    VertexScale(double ps){
        setEstimate(ps);
    }

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    virtual void setToOriginImpl(){
        setEstimate(1.0);
    }

    virtual void oplusImpl(const double *update_){
        setEstimate(estimate()*exp(*update_));
    }
};

// Inverse depth point (just one parameter, inverse depth at the host frame)
class VertexInvDepth : public g2o::BaseVertex<1,InvDepthPoint>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    VertexInvDepth(){}
    VertexInvDepth(double invDepth, double u, double v, KeyFrame* pHostKF){
        setEstimate(InvDepthPoint(invDepth, u, v, pHostKF));
    }

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    virtual void setToOriginImpl() {
        }

    virtual void oplusImpl(const double* update_){
        _estimate.Update(update_);
        updateCache();
    }
};

class EdgeMono : public g2o::BaseBinaryEdge<2,Eigen::Vector2d,g2o::VertexSBAPointXYZ,VertexPose>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    EdgeMono(int cam_idx_=0): cam_idx(cam_idx_){
    }

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError(){
        const g2o::VertexSBAPointXYZ* VPoint = static_cast<const g2o::VertexSBAPointXYZ*>(_vertices[0]);
        const VertexPose* VPose = static_cast<const VertexPose*>(_vertices[1]);
        const Eigen::Vector2d obs(_measurement);
        _error = obs - VPose->estimate().Project(VPoint->estimate(),cam_idx);
    }


    virtual void linearizeOplus();

    bool isDepthPositive()
    {
        const g2o::VertexSBAPointXYZ* VPoint = static_cast<const g2o::VertexSBAPointXYZ*>(_vertices[0]);
        const VertexPose* VPose = static_cast<const VertexPose*>(_vertices[1]);
        return VPose->estimate().isDepthPositive(VPoint->estimate(),cam_idx);
    }

    Eigen::Matrix<double,2,9> GetJacobian(){
        linearizeOplus();
        Eigen::Matrix<double,2,9> J;
        J.block<2,3>(0,0) = _jacobianOplusXi;
        J.block<2,6>(0,3) = _jacobianOplusXj;
        return J;
    }

    Eigen::Matrix<double,9,9> GetHessian(){
        linearizeOplus();
        Eigen::Matrix<double,2,9> J;
        J.block<2,3>(0,0) = _jacobianOplusXi;
        J.block<2,6>(0,3) = _jacobianOplusXj;
        return J.transpose()*information()*J;
    }

public:
    const int cam_idx;
};

class EdgeMonoOnlyPose : public g2o::BaseUnaryEdge<2,Eigen::Vector2d,VertexPose>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    EdgeMonoOnlyPose(const cv::Mat &Xw_, int cam_idx_=0):Xw(Converter::toVector3d(Xw_)),
        cam_idx(cam_idx_){}

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError(){
        const VertexPose* VPose = static_cast<const VertexPose*>(_vertices[0]);
        const Eigen::Vector2d obs(_measurement);
        _error = obs - VPose->estimate().Project(Xw,cam_idx);
    }

    virtual void linearizeOplus();

    bool isDepthPositive()
    {
        const VertexPose* VPose = static_cast<const VertexPose*>(_vertices[0]);
        return VPose->estimate().isDepthPositive(Xw,cam_idx);
    }

    Eigen::Matrix<double,6,6> GetHessian(){
        linearizeOplus();
        return _jacobianOplusXi.transpose()*information()*_jacobianOplusXi;
    }

public:
    const Eigen::Vector3d Xw;
    const int cam_idx;
};

class EdgeStereo : public g2o::BaseBinaryEdge<3,Eigen::Vector3d,g2o::VertexSBAPointXYZ,VertexPose>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    EdgeStereo(int cam_idx_=0): cam_idx(cam_idx_){}

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError(){
        const g2o::VertexSBAPointXYZ* VPoint = static_cast<const g2o::VertexSBAPointXYZ*>(_vertices[0]);
        const VertexPose* VPose = static_cast<const VertexPose*>(_vertices[1]);
        const Eigen::Vector3d obs(_measurement);
        _error = obs - VPose->estimate().ProjectStereo(VPoint->estimate(),cam_idx);
    }


    virtual void linearizeOplus();

    Eigen::Matrix<double,3,9> GetJacobian(){
        linearizeOplus();
        Eigen::Matrix<double,3,9> J;
        J.block<3,3>(0,0) = _jacobianOplusXi;
        J.block<3,6>(0,3) = _jacobianOplusXj;
        return J;
    }

    Eigen::Matrix<double,9,9> GetHessian(){
        linearizeOplus();
        Eigen::Matrix<double,3,9> J;
        J.block<3,3>(0,0) = _jacobianOplusXi;
        J.block<3,6>(0,3) = _jacobianOplusXj;
        return J.transpose()*information()*J;
    }

public:
    const int cam_idx;
};


class EdgeStereoOnlyPose : public g2o::BaseUnaryEdge<3,Eigen::Vector3d,VertexPose>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    EdgeStereoOnlyPose(const cv::Mat &Xw_, int cam_idx_=0):
        Xw(Converter::toVector3d(Xw_)), cam_idx(cam_idx_){}

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError(){
        const VertexPose* VPose = static_cast<const VertexPose*>(_vertices[0]);
        const Eigen::Vector3d obs(_measurement);
        _error = obs - VPose->estimate().ProjectStereo(Xw, cam_idx);
    }

    virtual void linearizeOplus();

    Eigen::Matrix<double,6,6> GetHessian(){
        linearizeOplus();
        return _jacobianOplusXi.transpose()*information()*_jacobianOplusXi;
    }

public:
    const Eigen::Vector3d Xw; // 3D point coordinates
    const int cam_idx;
};

class VertexTime : public g2o::BaseVertex<1,double>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    VertexTime(){
        setEstimate(1.0);
    }
    VertexTime(double t){
        setEstimate(t);
    }

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    virtual void setToOriginImpl(){
        setEstimate(1.0);
    }

    virtual void oplusImpl(const double *update_){
        setEstimate(estimate() + *update_);
    }
};

class EdgeSE3ProjectXYZTd: public g2o::BaseMultiEdge<2, Eigen::Vector2d>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    EdgeSE3ProjectXYZTd(cv::Point2f kpUnSpeed, double td):_speed(kpUnSpeed),_td(td){
        resize(3);
    };

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError()  {
        const g2o::VertexSE3Expmap* v1 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[1]);
        const g2o::VertexSBAPointXYZ* v2 = static_cast<const g2o::VertexSBAPointXYZ*>(_vertices[0]);
        const VertexTime* v3 = static_cast<const VertexTime*>(_vertices[2]);
        Eigen::Vector2d speed(_speed.x,_speed.y);
        //std::cout<< "_measurement = "<< _measurement << endl;
        //_measurement = _measurement - (v3->estimate() - _td) * speed;
        //std::cout<< "_measurement = "<< _measurement << endl;
        Eigen::Vector2d obs(_measurement);
        _error = obs - (v3->estimate() - _td) * speed - pCamera->project(v1->estimate().map(v2->estimate()));
    }

    bool isDepthPositive() {
        const g2o::VertexSE3Expmap* v1 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[1]);
        const g2o::VertexSBAPointXYZ* v2 = static_cast<const g2o::VertexSBAPointXYZ*>(_vertices[0]);
        return ((v1->estimate().map(v2->estimate()))(2)>0.0);
    }
    virtual void linearizeOplus();

    cv::Point2f _speed;
    double _td;
    GeometricCamera* pCamera;
};

class EdgeInertial : public g2o::BaseMultiEdge<9,Vector9d>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    EdgeInertial(IMU::Preintegrated* pInt);

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError();
    virtual void linearizeOplus();

    Eigen::Matrix<double,24,24> GetHessian(){
        linearizeOplus();
        Eigen::Matrix<double,9,24> J;
        J.block<9,6>(0,0) = _jacobianOplus[0];
        J.block<9,3>(0,6) = _jacobianOplus[1];
        J.block<9,3>(0,9) = _jacobianOplus[2];
        J.block<9,3>(0,12) = _jacobianOplus[3];
        J.block<9,6>(0,15) = _jacobianOplus[4];
        J.block<9,3>(0,21) = _jacobianOplus[5];
        return J.transpose()*information()*J;
    }

    Eigen::Matrix<double,18,18> GetHessianNoPose1(){
        linearizeOplus();
        Eigen::Matrix<double,9,18> J;
        J.block<9,3>(0,0) = _jacobianOplus[1];
        J.block<9,3>(0,3) = _jacobianOplus[2];
        J.block<9,3>(0,6) = _jacobianOplus[3];
        J.block<9,6>(0,9) = _jacobianOplus[4];
        J.block<9,3>(0,15) = _jacobianOplus[5];
        return J.transpose()*information()*J;
    }

    Eigen::Matrix<double,9,9> GetHessian2(){
        linearizeOplus();
        Eigen::Matrix<double,9,9> J;
        J.block<9,6>(0,0) = _jacobianOplus[4];
        J.block<9,3>(0,6) = _jacobianOplus[5];
        return J.transpose()*information()*J;
    }

    const Eigen::Matrix3d JRg, JVg, JPg;
    const Eigen::Matrix3d JVa, JPa;
    IMU::Preintegrated* mpInt;
    const double dt;
    Eigen::Vector3d g;
};


// Edge inertial whre gravity is included as optimizable variable and it is not supposed to be pointing in -z axis, as well as scale
class EdgeInertialGS : public g2o::BaseMultiEdge<9,Vector9d>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    // EdgeInertialGS(IMU::Preintegrated* pInt);
    EdgeInertialGS(IMU::Preintegrated* pInt);

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError();
    virtual void linearizeOplus();

    const Eigen::Matrix3d JRg, JVg, JPg;
    const Eigen::Matrix3d JVa, JPa;
    IMU::Preintegrated* mpInt;
    const double dt;
    Eigen::Vector3d g, gI;

    Eigen::Matrix<double,27,27> GetHessian(){
        linearizeOplus();
        Eigen::Matrix<double,9,27> J;
        J.block<9,6>(0,0) = _jacobianOplus[0];
        J.block<9,3>(0,6) = _jacobianOplus[1];
        J.block<9,3>(0,9) = _jacobianOplus[2];
        J.block<9,3>(0,12) = _jacobianOplus[3];
        J.block<9,6>(0,15) = _jacobianOplus[4];
        J.block<9,3>(0,21) = _jacobianOplus[5];
        J.block<9,2>(0,24) = _jacobianOplus[6];
        J.block<9,1>(0,26) = _jacobianOplus[7];
        return J.transpose()*information()*J;
    }

    Eigen::Matrix<double,27,27> GetHessian2(){
        linearizeOplus();
        Eigen::Matrix<double,9,27> J;
        J.block<9,3>(0,0) = _jacobianOplus[2];
        J.block<9,3>(0,3) = _jacobianOplus[3];
        J.block<9,2>(0,6) = _jacobianOplus[6];
        J.block<9,1>(0,8) = _jacobianOplus[7];
        J.block<9,3>(0,9) = _jacobianOplus[1];
        J.block<9,3>(0,12) = _jacobianOplus[5];
        J.block<9,6>(0,15) = _jacobianOplus[0];
        J.block<9,6>(0,21) = _jacobianOplus[4];
        return J.transpose()*information()*J;
    }

    Eigen::Matrix<double,9,9> GetHessian3(){
        linearizeOplus();
        Eigen::Matrix<double,9,9> J;
        J.block<9,3>(0,0) = _jacobianOplus[2];
        J.block<9,3>(0,3) = _jacobianOplus[3];
        J.block<9,2>(0,6) = _jacobianOplus[6];
        J.block<9,1>(0,8) = _jacobianOplus[7];
        return J.transpose()*information()*J;
    }



    Eigen::Matrix<double,1,1> GetHessianScale(){
        linearizeOplus();
        Eigen::Matrix<double,9,1> J = _jacobianOplus[7];
        return J.transpose()*information()*J;
    }

    Eigen::Matrix<double,3,3> GetHessianBiasGyro(){
        linearizeOplus();
        Eigen::Matrix<double,9,3> J = _jacobianOplus[2];
        return J.transpose()*information()*J;
    }

    Eigen::Matrix<double,3,3> GetHessianBiasAcc(){
        linearizeOplus();
        Eigen::Matrix<double,9,3> J = _jacobianOplus[3];
        return J.transpose()*information()*J;
    }

    Eigen::Matrix<double,2,2> GetHessianGDir(){
        linearizeOplus();
        Eigen::Matrix<double,9,2> J = _jacobianOplus[6];
        return J.transpose()*information()*J;
    }
};



class EdgeGyroRW : public g2o::BaseBinaryEdge<3,Eigen::Vector3d,VertexGyroBias,VertexGyroBias>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    EdgeGyroRW(){}

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError(){
        const VertexGyroBias* VG1= static_cast<const VertexGyroBias*>(_vertices[0]);
        const VertexGyroBias* VG2= static_cast<const VertexGyroBias*>(_vertices[1]);
        _error = VG2->estimate()-VG1->estimate();
    }

    virtual void linearizeOplus(){
        _jacobianOplusXi = -Eigen::Matrix3d::Identity();
        _jacobianOplusXj.setIdentity();
    }

    Eigen::Matrix<double,6,6> GetHessian(){
        linearizeOplus();
        Eigen::Matrix<double,3,6> J;
        J.block<3,3>(0,0) = _jacobianOplusXi;
        J.block<3,3>(0,3) = _jacobianOplusXj;
        return J.transpose()*information()*J;
    }

    Eigen::Matrix3d GetHessian2(){
        linearizeOplus();
        return _jacobianOplusXj.transpose()*information()*_jacobianOplusXj;
    }
};


class EdgeAccRW : public g2o::BaseBinaryEdge<3,Eigen::Vector3d,VertexAccBias,VertexAccBias>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    EdgeAccRW(){}

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError(){
        const VertexAccBias* VA1= static_cast<const VertexAccBias*>(_vertices[0]);
        const VertexAccBias* VA2= static_cast<const VertexAccBias*>(_vertices[1]);
        _error = VA2->estimate()-VA1->estimate();
    }

    virtual void linearizeOplus(){
        _jacobianOplusXi = -Eigen::Matrix3d::Identity();
        _jacobianOplusXj.setIdentity();
    }

    Eigen::Matrix<double,6,6> GetHessian(){
        linearizeOplus();
        Eigen::Matrix<double,3,6> J;
        J.block<3,3>(0,0) = _jacobianOplusXi;
        J.block<3,3>(0,3) = _jacobianOplusXj;
        return J.transpose()*information()*J;
    }

    Eigen::Matrix3d GetHessian2(){
        linearizeOplus();
        return _jacobianOplusXj.transpose()*information()*_jacobianOplusXj;
    }
};

class ConstraintPoseImu
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    ConstraintPoseImu(const Eigen::Matrix3d &Rwb_, const Eigen::Vector3d &twb_, const Eigen::Vector3d &vwb_,
                       const Eigen::Vector3d &bg_, const Eigen::Vector3d &ba_, const Matrix15d &H_):
                       Rwb(Rwb_), twb(twb_), vwb(vwb_), bg(bg_), ba(ba_), H(H_)
    {
        H = (H+H)/2;
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double,15,15> > es(H);
        Eigen::Matrix<double,15,1> eigs = es.eigenvalues();
        for(int i=0;i<15;i++)
            if(eigs[i]<1e-12)
                eigs[i]=0;
        H = es.eigenvectors()*eigs.asDiagonal()*es.eigenvectors().transpose();
    }
    ConstraintPoseImu(const cv::Mat &Rwb_, const cv::Mat &twb_, const cv::Mat &vwb_,
                       const IMU::Bias &b, const cv::Mat &H_)
    {
        Rwb = Converter::toMatrix3d(Rwb_);
        twb = Converter::toVector3d(twb_);
        vwb = Converter::toVector3d(vwb_);
        bg << b.bwx, b.bwy, b.bwz;
        ba << b.bax, b.bay, b.baz;
        for(int i=0;i<15;i++)
            for(int j=0;j<15;j++)
                H(i,j)=H_.at<float>(i,j);
        H = (H+H)/2;
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double,15,15> > es(H);
        Eigen::Matrix<double,15,1> eigs = es.eigenvalues();
        for(int i=0;i<15;i++)
            if(eigs[i]<1e-12)
                eigs[i]=0;
        H = es.eigenvectors()*eigs.asDiagonal()*es.eigenvectors().transpose();
    }

    Eigen::Matrix3d Rwb;
    Eigen::Vector3d twb;
    Eigen::Vector3d vwb;
    Eigen::Vector3d bg;
    Eigen::Vector3d ba;
    Matrix15d H;
};

class EdgePriorPoseImu : public g2o::BaseMultiEdge<15,Vector15d>
{
public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
        EdgePriorPoseImu(ConstraintPoseImu* c);

        virtual bool read(std::istream& is){return false;}
        virtual bool write(std::ostream& os) const{return false;}

        void computeError();
        virtual void linearizeOplus();

        Eigen::Matrix<double,15,15> GetHessian(){
            linearizeOplus();
            Eigen::Matrix<double,15,15> J;
            J.block<15,6>(0,0) = _jacobianOplus[0];
            J.block<15,3>(0,6) = _jacobianOplus[1];
            J.block<15,3>(0,9) = _jacobianOplus[2];
            J.block<15,3>(0,12) = _jacobianOplus[3];
            return J.transpose()*information()*J;
        }

        Eigen::Matrix<double,9,9> GetHessianNoPose(){
            linearizeOplus();
            Eigen::Matrix<double,15,9> J;
            J.block<15,3>(0,0) = _jacobianOplus[1];
            J.block<15,3>(0,3) = _jacobianOplus[2];
            J.block<15,3>(0,6) = _jacobianOplus[3];
            return J.transpose()*information()*J;
        }
        Eigen::Matrix3d Rwb;
        Eigen::Vector3d twb, vwb;
        Eigen::Vector3d bg, ba;
};

// Priors for biases
class EdgePriorAcc : public g2o::BaseUnaryEdge<3,Eigen::Vector3d,VertexAccBias>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    EdgePriorAcc(const cv::Mat &bprior_):bprior(Converter::toVector3d(bprior_)){}

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError(){
        const VertexAccBias* VA = static_cast<const VertexAccBias*>(_vertices[0]);
        _error = bprior - VA->estimate();
    }
    virtual void linearizeOplus();

    Eigen::Matrix<double,3,3> GetHessian(){
        linearizeOplus();
        return _jacobianOplusXi.transpose()*information()*_jacobianOplusXi;
    }

    const Eigen::Vector3d bprior;
};

class EdgePriorGyro : public g2o::BaseUnaryEdge<3,Eigen::Vector3d,VertexGyroBias>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    EdgePriorGyro(const cv::Mat &bprior_):bprior(Converter::toVector3d(bprior_)){}

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError(){
        const VertexGyroBias* VG = static_cast<const VertexGyroBias*>(_vertices[0]);
        _error = bprior - VG->estimate();
    }
    virtual void linearizeOplus();

    Eigen::Matrix<double,3,3> GetHessian(){
        linearizeOplus();
        return _jacobianOplusXi.transpose()*information()*_jacobianOplusXi;
    }

    const Eigen::Vector3d bprior;
};


class Edge4DoF : public g2o::BaseBinaryEdge<6,Vector6d,VertexPose4DoF,VertexPose4DoF>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    Edge4DoF(const Eigen::Matrix4d &deltaT){
        dTij = deltaT;
        dRij = deltaT.block<3,3>(0,0);
        dtij = deltaT.block<3,1>(0,3);
    }

    virtual bool read(std::istream& is){return false;}
    virtual bool write(std::ostream& os) const{return false;}

    void computeError(){
        const VertexPose4DoF* VPi = static_cast<const VertexPose4DoF*>(_vertices[0]);
        const VertexPose4DoF* VPj = static_cast<const VertexPose4DoF*>(_vertices[1]);
        _error << LogSO3(VPi->estimate().Rcw[0]*VPj->estimate().Rcw[0].transpose()*dRij.transpose()),
                 VPi->estimate().Rcw[0]*(-VPj->estimate().Rcw[0].transpose()*VPj->estimate().tcw[0])+VPi->estimate().tcw[0] - dtij;
    }

    // virtual void linearizeOplus(); // numerical implementation

    Eigen::Matrix4d dTij;
    Eigen::Matrix3d dRij;
    Eigen::Vector3d dtij;
};

//iGPS transmitter pose estimation
class EdgeiGPSSE3Graph : public g2o::BaseUnaryEdge<3,Eigen::Vector3d,g2o::VertexSE3Expmap>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW;

    EdgeiGPSSE3Graph(){};

    virtual bool read(std::istream& is) override{};

    virtual bool write(std::ostream& os) const override{};

    virtual void computeError() override {
        //const g2o::VertexSE3Expmap * v1 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[0]);  //vP
        const g2o::VertexSE3Expmap * v2 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[0]);  //vRT
        Eigen::Matrix3d Rwc = Tcw.rotation().inverse().toRotationMatrix();
        Eigen::Vector3d twc = - Rwc * Tcw.translation();
        //std::cout << "Rwc = " <<Rwc <<std::endl;
        //std::cout << "twc = " <<twc <<std::endl;

        g2o::SE3Quat Ti = v2->estimate();
        Eigen::Matrix3d Ri = Ti.rotation().toRotationMatrix();
        Eigen::Vector3d ti = Ti.translation();

        Eigen::Vector3d obs(_measurement);

        //std::cout << "Tcw = " <<Tcw <<std::endl;
        //std::cout << "Ti = " <<Ti <<std::endl;

        Eigen::Vector3d temp = twc - ti;
        double norm = sqrt(temp.x()*temp.x() + temp.y()*temp.y() +temp.z()*temp.z());

        _error = twc - ti - norm * Ri * obs;
        //cout <<  "twc = "<< twc <<endl;
        //cout << "ti = " <<ti <<endl;
        //cout << "temp = " <<temp <<endl;
        //cout << "norm * Ri * obs = " <<norm * Ri * obs <<endl;
        //cout << "Ri = " <<Ri <<endl;
        //cout << "_error = " <<_error <<endl;


        //if(_error.norm()>10)
        //{
        //    cout <<  "test" <<endl;
        //}
        //cout <<  "(twc - ti).norm() = "<< (twc - ti).norm() <<endl;
        //cout <<  "obs = "<< obs <<endl;

    }
    //virtual void linearizeOplus()
    //{
    //    _jacobianOplusXi.setZero();
    //}
    g2o::SE3Quat Tcw;
};

//Tightly-coupled vision and iGPS direction optimization
//class EdgeiGPSDir6DoFPose:public g2o::BaseBinaryEdge<3,Eigen::Vector3d,g2o::VertexSE3Expmap, VertexScale>
//{
//public:
//    EIGEN_MAKE_ALIGNED_OPERATOR_NEW;
//
//    EdgeiGPSDir6DoFPose(Eigen::Matrix4d eTci):Tci(eTci) {};
//
//    virtual bool read(std::istream& is) override{};
//
//    virtual bool write(std::ostream& os) const override{};
//
//    virtual void computeError() override
//    {
//        _error = Eigen::Vector3d(0.0,0.0,0.0);   //error initialization
//        const g2o::VertexSE3Expmap * v1 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[0]);    //Tcw
//        Eigen::Matrix3d rwc = v1->estimate().rotation().toRotationMatrix().transpose();
//        Eigen::Vector3d tcw = v1->estimate().translation();
//        Eigen::Vector3d t = - rwc * tcw;
//        const VertexScale * v2 = static_cast<const VertexScale*>(_vertices[1]);    //iGPS Position Scale
//        double scale = v2->estimate();
//        //t = scale * t;
//
//        Eigen::Vector3d obs(_measurement);
//        Eigen::Matrix3d Rci = Tci.block<3,3>(0,0);
//        Eigen::Vector3d GT = Rci * obs;
//        //Eigen::Vector3d iGPSPosition = Tci.block<3,1>(0,3);    //scaled iGPS Position w.r.t camera frame
//        Eigen::Vector3d iGPSPosition = scale * Tci.block<3,1>(0,3);    //scaled iGPS Position w.r.t camera frame
//
//        Eigen::Vector3d measurement = (t-iGPSPosition)/(t-iGPSPosition).norm();
//
//        Eigen::Vector3d result = measurement - GT;
//        _error = measurement - GT;
//        //cout << "CamDir = " << CamDir.transpose() <<endl;
//        if(result.norm()>0.1)
//        {
//            cout << "scale = " << scale <<endl;
//            cout << "t = " << t.transpose() <<endl;
//            cout << "iGPSPosition = " << iGPSPosition.transpose() <<endl;
//            cout << "measurement = " << measurement.transpose() <<endl;
//            cout << "GT = " << GT.transpose() <<endl;
//            cout << "Differences = " << result.transpose() <<endl;
//        }
//    }
//
//private:
//    //double _precision;
//    Eigen::Matrix4d Tci;
//};


class EdgeiGPSDirUptoScale6DoFPose:public g2o::BaseMultiEdge<3,Eigen::Vector3d>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW;

    EdgeiGPSDirUptoScale6DoFPose()
    {
        resize(3);
    };

    virtual bool read(std::istream& is) override{};

    virtual bool write(std::ostream& os) const override{};

    virtual void computeError() override
    {
        _error = Eigen::Vector3d(0.0,0.0,0.0);   //error initialization
        const g2o::VertexSE3Expmap * v1 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[0]);    //Tcw
        Eigen::Matrix3d rwc = v1->estimate().rotation().toRotationMatrix().transpose();
        Eigen::Vector3d tcw = v1->estimate().translation();
        Eigen::Vector3d t = - rwc * tcw;   //up-to-scale cam pose
        const VertexScale * v2 = static_cast<const VertexScale*>(_vertices[1]);    // Scale between iGPS and cam
        double scale = v2->estimate();

        //t = scale * t;
        const g2o::VertexSE3Expmap* v3 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[2]);  // iGPS Position in camera frame
        Eigen::Matrix3d Rci = v3->estimate().rotation().toRotationMatrix();
        Eigen::Vector3d tci = v3->estimate().translation();

        Eigen::Vector3d obs(_measurement);
        Eigen::Vector3d GT = Rci * obs;
        //Eigen::Vector3d iGPSPosition = Tci.block<3,1>(0,3);    //scaled iGPS Position w.r.t camera frame
        Eigen::Vector3d iGPSPosition = scale * tci;    //scaled iGPS Position w.r.t camera frame

        Eigen::Vector3d measurement = (t-iGPSPosition)/(t-iGPSPosition).norm();

        Eigen::Vector3d result = measurement - GT;
        _error = measurement - GT;
        //cout << "CamDir = " << CamDir.transpose() <<endl;
        if(result.norm()>0.1)
        {
            cout << "scale = " << scale <<endl;
            cout << "t = " << t.transpose() <<endl;
            cout << "iGPSPosition = " << iGPSPosition.transpose() <<endl;
            cout << "measurement = " << measurement.transpose() <<endl;
            cout << "GT = " << GT.transpose() <<endl;
            cout << "Differences = " << result.transpose() <<endl;
        }
    }
};

/*
class EdgeiGPSDir6DoFPose:public g2o::BaseMultiEdge<3,Eigen::Vector3d>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW;

    EdgeiGPSDir6DoFPose()
    {
        resize(2);
    };

    virtual bool read(std::istream& is) override{};

    virtual bool write(std::ostream& os) const override{};

    virtual void computeError() override
    {
        _error = Eigen::Vector3d(0.0,0.0,0.0);   //error initialization
        const g2o::VertexSE3Expmap * v1 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[0]);    //Tcw
        Eigen::Matrix3d rwc = v1->estimate().rotation().toRotationMatrix().transpose();
        Eigen::Vector3d tcw = v1->estimate().translation();
        Eigen::Vector3d t = - rwc * tcw;   //up-to-scale cam pose

        const g2o::VertexSE3Expmap* v2 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[1]);  // iGPS Position in camera frame
        Eigen::Matrix3d Rci = v2->estimate().rotation().toRotationMatrix();
        Eigen::Vector3d tci = v2->estimate().translation();

        Eigen::Vector3d obs(_measurement);
        Eigen::Vector3d GT = Rci * obs;
        //Eigen::Vector3d iGPSPosition = Tci.block<3,1>(0,3);    //scaled iGPS Position w.r.t camera frame
        Eigen::Vector3d iGPSPosition = tci;    //scaled iGPS Position w.r.t camera frame

        Eigen::Vector3d measurement = (t-iGPSPosition)/(t-iGPSPosition).norm();

        Eigen::Vector3d result = measurement - GT;
        _error = measurement - GT;
        //cout << "_error = " << _error <<endl;

        //cout << "CamDir = " << CamDir.transpose() <<endl;
        if(result.norm()>0.1)
        {
          cout << "t = " << t.transpose() <<endl;
          cout << "iGPSPosition = " << iGPSPosition.transpose() <<endl;
          cout << "measurement = " << measurement.transpose() <<endl;
          cout << "GT = " << GT.transpose() <<endl;
          cout << "Differences = " << result.transpose() <<endl;
        }
    }
};
*/


class EdgeiGPSDir6DoFPose:public g2o::BaseMultiEdge<4,Eigen::Vector4d>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW;

    EdgeiGPSDir6DoFPose()
    {
        resize(1);
    };

    virtual bool read(std::istream& is) override{};

    virtual bool write(std::ostream& os) const override{};

    virtual void computeError() override
    {
        _error = Eigen::Vector4d(0.0,0.0,0.0,0.0);   //error initialization
        const g2o::VertexSE3Expmap * v1 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[0]);    //Tcw
        Eigen::Matrix3d rcw = v1->estimate().rotation().toRotationMatrix();
        Eigen::Vector3d tcw = v1->estimate().translation();
        Eigen::Vector3d twc = - rcw.transpose() * tcw;   //up-to-scale cam pose

        Eigen::Matrix4d Twc;
        Twc.setIdentity();
        Twc.block<3,3>(0,0) = rcw.transpose();
        Twc.block<3,1>(0,3) = twc;
        Eigen::Vector4d iGPSPosInCam4d = Eigen::Vector4d( miGPSReceive[channel].x(), miGPSReceive[channel].y(),miGPSReceive[channel].z() , 1.0);
        Eigen::Vector4d iGPSPosition4d = Twc * iGPSPosInCam4d;
        Eigen::Vector3d iGPSPosition3d = iGPSPosition4d.block<3,1>(0,0);

        Eigen::Matrix3d Rci = Tci.rotation().toRotationMatrix();
        Eigen::Vector3d tci = Tci.translation();

        //Eigen::Vector4d obs4d(_measurement);
        //Eigen::Vector3d obs = obs4d.block<3,1>(0,0);
        Eigen::Vector3d GT = Rci * Dir;
        GT.normalize();
        //Eigen::Vector3d iGPSPosition = Tci.block<3,1>(0,3);    //scaled iGPS Position w.r.t camera frame
        Eigen::Vector3d iGPSPosition = tci;    //scaled iGPS Position w.r.t camera frame

        //Eigen::Vector3d measurement = (twc-iGPSPosition)/(twc-iGPSPosition).norm();
        Eigen::Vector3d measurement = (iGPSPosition3d-iGPSPosition)/(iGPSPosition3d-iGPSPosition).norm();

        Eigen::Vector3d result = measurement - GT;
        //cout << "result = "<< result.transpose() <<endl;

        double angle = acos((measurement.transpose() * GT).norm());

        _error = Eigen::Vector4d(result.x(),result.y(),result.z(),angle);
        //if(_error.norm()>0.05)
        //{
        //    cout << "iGPSPosition3d = "<< iGPSPosition3d.transpose() <<endl;
        //    cout << "iGPSPosition = "<< iGPSPosition.transpose() <<endl;
        //    cout << "measurement = "<< measurement.transpose() <<endl;
        //    cout << "GT = "<< GT.transpose() <<endl <<endl;
        //    cout << "_error = "<< _error.transpose() <<endl;
        //}
    }
    int channel;
    map<int, Eigen::Vector3d> miGPSReceive;
    g2o::SE3Quat Tci;
    Eigen::Vector3d Dir;
};

/*
class EdgeiGPSDir6DoFPose:public g2o::BaseMultiEdge<2,Eigen::Vector2d>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW;

    EdgeiGPSDir6DoFPose()
    {
        resize(1);
    };

    virtual bool read(std::istream& is) override{};

    virtual bool write(std::ostream& os) const override{};

    virtual void computeError() override
    {
        _error = Eigen::Vector2d(0.0,0.0);   //error initialization
        const g2o::VertexSE3Expmap * v1 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[0]);    //Tcw
        Eigen::Matrix3d rcw = v1->estimate().rotation().toRotationMatrix();
        Eigen::Vector3d tcw = v1->estimate().translation();
        Eigen::Vector3d twc = - rcw.transpose() * tcw;   //up-to-scale cam pose

        Eigen::Matrix4d Twc;
        Twc.setIdentity();
        Twc.block<3,3>(0,0) = rcw.transpose();
        Twc.block<3,1>(0,3) = twc;
        Eigen::Vector4d iGPSPosInCam4d = Eigen::Vector4d( miGPSReceive[channel].x(), miGPSReceive[channel].y(),miGPSReceive[channel].z() , 1.0);
        Eigen::Vector4d iGPSPosition4d = Twc * iGPSPosInCam4d;
        Eigen::Vector3d iGPSPosition3d = iGPSPosition4d.block<3,1>(0,0);

        Eigen::Matrix3d Rci = Tci.rotation().toRotationMatrix();
        Eigen::Vector3d tci = Tci.translation();

        Eigen::Vector3d iGPS_GT = Dir;
        iGPS_GT.normalize();
        Eigen::Vector3d iGPSPosition = tci;

        Eigen::Vector3d iGPS_meas = (iGPSPosition3d-iGPSPosition)/(iGPSPosition3d-iGPSPosition).norm();
        iGPS_meas = Rci.transpose() * iGPS_meas;

        //converter to Panoramic imagng model, origin of frame in iGPS transmitter
        double u_meas, v_meas;
        double u_GT  , v_GT;
        CalculatePixelCoordinate(iGPS_meas,u_meas,v_meas);
        CalculatePixelCoordinate(iGPS_GT,  u_GT,  v_GT);

        //cout << " iGPS_GT   = " << iGPS_GT <<endl;
        //cout << " iGPS_meas = " << iGPS_meas <<endl;
        //cout << " u_meas , v_meas   = " << u_meas << " " << v_meas <<endl;
        //cout << " u_GT , v_GT = " << u_GT << " " << v_GT <<endl;
        _error = Eigen::Vector2d(u_meas - u_GT, v_meas - v_GT);
        //if(_error.norm()>0.01)
        //{
            //cout << "_error = " << _error <<endl;
        //}
    }

    int channel;
    map<int, Eigen::Vector3d> miGPSReceive;
    g2o::SE3Quat Tci;
    Eigen::Vector3d Dir;

    void CalculatePixelCoordinate(const Eigen::Vector3d direction,double& u, double& v)
    {
        double Dx = direction.x(), Dy = direction.y(), Dz = direction.z();
        double pi = 3.14159265;
        double elevator = asin(Dz);
        double azimuth = 0.0;
        azimuth = asin(Dy/cos(elevator));

        //if(Dy>0.0)
        //{
        //    if(Dx>0.0)
        //    {
        //        azimuth = asin(Dy/cos(elevator));
        //    }
        //    else
        //    {
        //        azimuth = pi - asin(Dy/cos(elevator));
        //    }
        //}
        //else
        //{
        //    if(Dx>0.0)
        //    {
        //        azimuth = 2 * pi + asin(Dy/cos(elevator));
        //    }
        //    else
        //    {
        //        azimuth = pi - asin(Dy/cos(elevator));
        //    }
        //}

        double yita = tan(elevator);
        double yipth = azimuth;
        int nC = 5000,nR = 5000;
        double Resolution_x = 2 * pi / nC;
        double Resolution_y = 2 * pi / nR;
        u = yipth / Resolution_x;
        v = nR / 2 - yita / Resolution_y;
    }

};
*/

class Edge6DoFPoseVertex:public g2o::BaseUnaryEdge<6,Vector6d,g2o::VertexSE3Expmap>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW;

    Edge6DoFPoseVertex(){}

    virtual bool read(std::istream& is) override{};

    virtual bool write(std::ostream& os) const override{};

    virtual void computeError() override
    {
        _error = Vector6d(0.0,0.0,0.0,0.0,0.0,0.0);   //error initialization
        const g2o::VertexSE3Expmap * v1 = static_cast<const g2o::VertexSE3Expmap*>(_vertices[0]);    //Tcw
        Eigen::Matrix3d rwc = v1->estimate().rotation().toRotationMatrix();
        Eigen::Vector3d t = v1->estimate().translation();

        Vector6d obs;
        obs<< v1->estimate().rotation().x(),v1->estimate().rotation().y(),v1->estimate().rotation().z(),t;
        _error = _measurement - obs;

    }
};

} //namespace ORB_SLAM2

#endif // G2OTYPES_H
