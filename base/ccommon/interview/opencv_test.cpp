#include "Eigen/Dense"
#include "opencv2/opencv.hpp"
#include <opencv2/core/eigen.hpp>

#include <iostream>
#include <vector>


using namespace cv;
using namespace std;

void onMouse(int event, int x, int y, int flags, void* param)
{
    Mat *im = reinterpret_cast<Mat*>(param);
    switch (event)
    {
        case EVENT_LBUTTONDOWN:     //鼠标左键按下响应：返回坐标和灰度
            static int count = 0;
            static int last_u = 0;
            static int last_v = 0;
            std::cout<<"at("<<x<<","<<y<<")value is:"
                     <<static_cast<int>(im->at<uchar>(cv::Point(x,y)))<<std::endl;
            cv::circle(*im, cv::Point(x,y), 2, Scalar(255,0,0), 2);
            if (count) {
                cv::line(*im, cv::Point(x,y), cv::Point(last_u,last_v), Scalar(0,255,0));
            }
            count = (count + 1) % 2;
            last_u = x;
            last_v = y;
            imshow("img", *im);
            break;
    }
}

// 3点确定平面 ax + by + cz + d = 0
void ThreePointGetPlane(const std::vector<Eigen::Vector3d> &pts, Eigen::Vector4d &coeff)
{
    assert(pts.size() == 3);
    Eigen::Vector3d a = pts[2] - pts[0];
    Eigen::Vector3d b = pts[1] - pts[0];
    Eigen::Vector3d c = (a.cross(b)).normalized();
    double d = -c[0] * pts[0][0] - c[1] * pts[0][1] - c[2] * pts[0][2];
    coeff << c[0], c[1], c[2], d;
    return;
}

// 2个空间向量求夹角
// 返回弧度
double inline TwoVectorGetAngle(const Eigen::Vector3d &a, const Eigen::Vector3d &b)
{
    return atan2(a.cross(b).norm(), a.transpose() * b);
}

void TestThreePointGetPlane()
{
    Eigen::Vector3d p1 = {0,0,-0.25};
    Eigen::Vector3d p2 = {0,1,-1};
    Eigen::Vector3d p3 = {1,-1,0};

    std::vector<Eigen::Vector3d> pts = {p1, p2, p3};
    Eigen::Vector4d coeff;
    ThreePointGetPlane(pts, coeff);
    std::cout << "ceff:" << coeff;
}

void TestTwoVectorGetAngle()
{
    Eigen::Vector3d a (1, 0, 0);
    Eigen::Vector3d b (0.707, 0.707, 0);
    std::cout << TwoVectorGetAngle(a,b) / 3.14 * 180.0;
}

void Pixl2Camera(const std::vector<cv::Point2f> &pixls, std::vector<Eigen::Vector3d> &camera,
                 const Eigen::Matrix3d &cam_intric, const Eigen::Matrix3d &r, const Eigen::Vector3d &t)
{
    double fx = cam_intric(0,0);
    double fy = cam_intric(1,1);
    double cx = cam_intric(0,2);
    double cy = cam_intric(1,2);
    for (int i = 0; i < pixls.size(); ++i) {
        Eigen::Vector3d tmp;
        tmp << (pixls[i].x - cx) / fx, (pixls[i].y - cy) / fy, 1;
        tmp = r * tmp + t;
        camera.push_back(tmp);
    }
}

int main()
{
    std::string base_path = "D:\\20230202";
    std::string sequence = "06";
    std::string index = "000010.png";
    std::string left_image_path = base_path + "\\sequences\\" + sequence + "\\image_0\\" + index;
    std::string right_image_path = base_path + "\\sequences\\" + sequence + "\\image_1\\" + index;

    double theta_x = 0.2 / 180. * 3.1415926, theta_y = 0.6 / 180. * 3.1415926, theta_z = 0.3 / 180. * 3.1415926;

    std::vector<cv::Point2f> img_0_pts = {
            {383,352},{542,216},
            {1000,300},{777,228},
            {872,341},{717,245},
            {371, 4}, {378,125}};

    std::vector<cv::Point2f> img_1_pts = {
            {314,355}, {500,233},
            {924,290}, {785,239},
            {814,340}, {703,254},
            {326,005}, {333,126}};

    Eigen::Vector3d t = {3.798145000000e+02 / 7.070912000000e+02, 0., 0.}; // img_0 pts --> img_1 pts, base line = 0.5371506532679236

    Eigen::Matrix3d cam_intric;
    cam_intric << 7.070912000000e+02,0.000000000000e+00,6.018873000000e+02,
                  0.000000000000e+00,7.070912000000e+02,1.831104000000e+02,
                  0.000000000000e+00,0.000000000000e+00,1.000000000000e+00;

    Eigen::AngleAxisd ax_x(theta_x, Eigen::Vector3d::UnitX());
    Eigen::AngleAxisd ax_y(theta_y, Eigen::Vector3d::UnitY());
    Eigen::AngleAxisd ax_z(theta_z, Eigen::Vector3d::UnitZ());
    Eigen::Matrix3d R_distort = (ax_z * ax_y * ax_x).toRotationMatrix();

    Mat D, R, C, new_img, imgs;
    cv::eigen2cv(cam_intric, C);
    cv::eigen2cv(R_distort, R);
    std::vector<cv::Point2f> img_1_pts_distort;
    cv::undistortPoints(img_1_pts, img_1_pts_distort, C, D, R, C);

    std::vector<Eigen::Vector3d> cam_0_pts, cam_1_pts; // 都转化为cam_0相机坐标系
    Pixl2Camera(img_0_pts, cam_0_pts, cam_intric, Eigen::Matrix3d::Identity(), Eigen::Vector3d::Zero());
    Pixl2Camera(img_1_pts_distort, cam_1_pts, cam_intric, Eigen::Matrix3d::Identity(), t);

    std::vector<Eigen::Vector4d> planes; // 偶数为空间直线与cam0光心形成的平面, 奇数为空间直线与cam1光心形成的平面
    for (int i = 0; i < cam_0_pts.size(); i += 2) {
        Eigen::Vector4d plane;
        std::vector<Eigen::Vector3d> threePts0 = {Eigen::Vector3d::Zero(), cam_0_pts[i], cam_0_pts[i + 1]};
        ThreePointGetPlane(threePts0, plane);
        planes.push_back(plane);

        std::vector<Eigen::Vector3d> threePts1 = {t, cam_1_pts[i], cam_1_pts[i + 1]};
        ThreePointGetPlane(threePts1, plane);
        planes.push_back(plane);
    }

    std::vector<Eigen::Vector3d> ort; // 空间中的直线方向向量
    for (int i = 0; i < planes.size(); i += 2) {
        // 与两个平面法向量都垂直的直线是两个平面的交线
        Eigen::Vector3d v1 = planes[i].block(0,0,3,1);
        Eigen::Vector3d v2 = planes[i + 1].block(0,0,3,1);
        ort.push_back(v1.cross(v2).normalized());
    }

    std::cout << "1-2 angle: " << TwoVectorGetAngle(ort[0], ort[1]) / 3.1415926 * 180. << std::endl;
    std::cout << "1-3 angle: " << TwoVectorGetAngle(ort[0], ort[2]) / 3.1415926 * 180. << std::endl;
    std::cout << "1-4 angle: " << TwoVectorGetAngle(ort[0], ort[3]) / 3.1415926 * 180. << std::endl;
    std::cout << "2-3 angle: " << TwoVectorGetAngle(ort[1], ort[2]) / 3.1415926 * 180. << std::endl;
    std::cout << "2-4 angle: " << TwoVectorGetAngle(ort[1], ort[3]) / 3.1415926 * 180. << std::endl;
    std::cout << "3-4 angle: " << TwoVectorGetAngle(ort[2], ort[3]) / 3.1415926 * 180. << std::endl;


    Mat img_0 = imread(left_image_path);
    Mat img_1 = imread(right_image_path);
    Mat img_1_distort;
    Mat map1, map2;
    cv::initUndistortRectifyMap(C, D, R, C, img_0.size(), CV_32FC1, map1, map2);
    cv::remap(img_1, img_1_distort, map1, map2, INTER_LINEAR); // 对image1做虚拟的旋转


    for (int i = 0; i < img_0_pts.size(); i += 2) {
        cv::line(img_0, img_0_pts[i], img_0_pts[i + 1], Scalar(255,0,0));
        cv::putText(img_0, std::to_string(i/2 + 1), (img_0_pts[i] + img_0_pts[i + 1]) / 2, FONT_HERSHEY_COMPLEX, 1.0, Scalar(255,0,0));
    }

    for (int i = 0; i < img_1_pts.size(); i += 2) {
        cv::line(img_1, img_1_pts[i], img_1_pts[i + 1], Scalar(255,0,0));
        cv::putText(img_1, std::to_string(i/2 + 1), (img_1_pts[i] + img_1_pts[i + 1]) / 2, FONT_HERSHEY_COMPLEX, 1.0, Scalar(255,0,0));
        cv::line(img_1, img_1_pts_distort[i], img_1_pts_distort[i + 1], Scalar(255,255,0));}

    for (int i = 0; i < img_1_pts_distort.size(); i += 2) {
        cv::line(img_1_distort, img_1_pts_distort[i], img_1_pts_distort[i + 1], Scalar(255,0,0));
        cv::putText(img_1_distort, std::to_string(i / 2 + 1), (img_1_pts_distort[i] + img_1_pts_distort[i + 1]) / 2, FONT_HERSHEY_COMPLEX, 1.0, Scalar(255,0,0));
    }

    cv::vconcat(img_0, img_1, imgs);
    cv::vconcat(imgs, img_1_distort, imgs);
    cv::namedWindow("img",0);
    cv::setMouseCallback("img", onMouse, reinterpret_cast<void*> (&imgs));
    imshow("img", imgs);
    cv::waitKey(0);
    return 0;
}




