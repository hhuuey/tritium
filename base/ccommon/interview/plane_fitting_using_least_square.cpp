//
// Created by huang on 2023/2/6.
//
#include <Eigen/Core>
#include <Eigen/Dense>

#include <vector>
#include <iostream>

struct point {
    point(double x_o, double y_o, double z_o) : x(x_o), y(y_o), z(z_o) {};
    double x;
    double y;
    double z;
};

int main(int argc, char* argv[])
{
    std::vector<point> planePoints;
    planePoints.emplace_back(1., 2., 0.01);
    planePoints.emplace_back(-1., 2., -0.03);
    planePoints.emplace_back(1., -2., 0.);
    planePoints.emplace_back(-1., -2., 0.05);
    planePoints.emplace_back(0., -0., -0.02);

    Eigen::MatrixXd A(planePoints.size(), 3);
    Eigen::VectorXd b(planePoints.size());
    //将观测点输入矩阵
    for (int i = 0; i < planePoints.size(); i++)
    {
        A(i, 0) = planePoints[i].x;
        A(i, 1) = planePoints[i].y;
        A(i, 2) = 1;
        b(i) = planePoints[i].z;
    }
    Eigen::MatrixXd AT = A.transpose();
    //使用最小二乘法求得系数向量的三种方法
    Eigen::Vector3d x = -(AT*A).inverse()*AT*b; // 正规方程
    Eigen::Vector3d x1 = -A.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(b); // SVD分解
    Eigen::Vector3d x2 = -A.colPivHouseholderQr().solve(b); //QR分解
    std::cout << x.x() << "," << x.y() << " " << x.z() << std::endl;
    std::cout << x1.x() << "," << x1.y() << " " << x1.z() << std::endl;
    std::cout << x2.x() << "," << x2.y() << " " << x2.z() << std::endl;

    Eigen::Vector3d n(x(0), x(1), 1);
    n = n.normalized();

    Eigen::Vector3d zdir(0, 0, 1);
    //求解两向量的旋转向量，点乘求夹角、叉乘求旋转方向。
    Eigen::AngleAxisd rotateVector(acos(n.dot(zdir)), n.cross(zdir).normalized());
    //获取旋转矩阵
    Eigen::Matrix3d zRotateMatrix = rotateVector.matrix();
    return 0;
}