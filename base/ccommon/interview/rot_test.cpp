#include "Eigen/Dense"
#include <iostream>

int main()
{
    Eigen::Matrix3d rx,ry,rz;
    double theta_x = 0.3, theta_y=0.2, theta_z=0.4;

    rx << 1.0, 0.0, 0.0,
    0.0, cos(theta_x), -sin(theta_x),
    0.0, sin(theta_x),  cos(theta_x);

    ry << cos(theta_y), 0.0,sin(theta_y),
    0.0,1.0,0.0,
    -sin(theta_y),0.0,cos(theta_y);

    rz << cos(theta_z), -sin(theta_z), 0.0,
    sin(theta_z), cos(theta_z),0.0,
    0.,0.0, 1.0;

    Eigen::AngleAxisd ax_x(theta_x, Eigen::Vector3d::UnitX());

    std::cout << ax_x.toRotationMatrix() << std::endl << std::endl;
    std::cout << rx << std::endl;
    std::cout << "-----------------------------------" << std::endl;

    Eigen::AngleAxisd ax_y(theta_y, Eigen::Vector3d::UnitY());

    std::cout << ax_y.toRotationMatrix() << std::endl << std::endl;
    std::cout << ry << std::endl;
    std::cout << "-----------------------------------" << std::endl;

    Eigen::AngleAxisd ax_z(theta_z, Eigen::Vector3d::UnitZ());

    std::cout << ax_z.toRotationMatrix() << std::endl << std::endl;
    std::cout << rz << std::endl;
    std::cout << "-----------------------------------" << std::endl;
    return 0;
}
