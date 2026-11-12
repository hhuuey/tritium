#include <iostream>
#include <cmath>
//  Eigen
#include <Eigen/Core>
#include <Eigen/Dense>
//  OpenCV
#include <opencv2/opencv.hpp>
#include "ceres/ceres.h"

//  namespace
using namespace std;

struct TestResidual {
    TestResidual(double x, double y) : x_(x), y_(y) {}

    template<typename T>
    bool operator() (const T * const a, const T * const b, const T * const c, T *  residual) const {
        residual[0] = y_ - exp(a[0] * x_ * x_ + b[0] * x_ + c[0]);
        return true;
    }

private:
    const double x_;
    const double y_;
};

int main() {
    /*--------  初始参数配置  --------*/
    //  实际曲线参数
    double ar = 1.0, br = 1.0, cr = 1.0;
    //  估计曲线参数初始值
    double ae = 2.0, be = 1.0, ce = 5.0;
    //  采样观测数据点个数
    const int N = 100;
    //  噪声标准差及其倒数
    double w_sigma = 1.0;

    //  随机数生成器
    cv::RNG rng;
    double data[2 * N];
    /*--------  观测数据生成  --------*/
    vector<double> x_data, y_data;
    for (int i = 0; i < N; i++) {
        double x = i / 100.0;
        x_data.push_back(x);
        y_data.push_back(exp(ar * x * x + br * x + cr) + rng.gaussian(w_sigma * w_sigma));
        data[2 * i] = x_data.back();
        data[2 * i + 1] = y_data.back();
    }

    /*ceres*/
    ceres::Problem problem;
    double a_ceres = 2.0, b_ceres = 1.0, c_ceres = 5.0;
    for (int i = 0; i < N; ++i) {
        ceres::CostFunction *costFunction = new ceres::AutoDiffCostFunction<TestResidual, 1, 1, 1, 1>(
                new TestResidual(data[2 * i],
                                 data[2 * i + 1]));
        problem.AddResidualBlock(costFunction, new ceres::CauchyLoss(0.5), &a_ceres, &b_ceres, &c_ceres);
    }

    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_QR;
    options.minimizer_progress_to_stdout = true;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    std::cout << summary.BriefReport() << std::endl;
    std::cout << "final a b c: " << a_ceres << ", " << b_ceres << ", " << c_ceres << std::endl;


    size_t iterations = 100;
    double lastCost = 0;
    Eigen::Vector3d x = Eigen::Vector3d::Zero();
    x[0] = ae;
    x[1] = be;
    x[2] = ce;
    for (size_t i = 0; i < iterations; ++i) {
        double loss = 0.0;
        Eigen::Matrix3d H = Eigen::Matrix3d::Zero();
        Eigen::Vector3d g = Eigen::Vector3d::Zero();
        for (int i = 0; i < N; ++i) {
            double x_i = x_data[i];
            double y_i = y_data[i];
            double tmp_i = exp(x[0] * x_i * x_i + x[1] * x_i + x[2]);
            double err_i = y_i - tmp_i;
            Eigen::Vector3d J;
            J[0] = -x_i * x_i * tmp_i;
            J[1] = -x_i * tmp_i;
            J[2] = -tmp_i;
            H += J * J.transpose();
            g += -err_i * J;
            loss += 0.5 * err_i * err_i;
        }
        Eigen::Vector3d dx = H.ldlt().solve(g);

    }

enum class {
    
}
    /*--------  Gauss-Newton    --------*/
    //  迭代次数
    int iterations = 100;
    //  迭代cost以及上一次迭代cost
    double cost, lastCost = 0;
    for (int i = 0; i < iterations; ++i) {
        double loss = 0.;
        Eigen::Matrix3d H = Eigen::Matrix3d::Zero();
        Eigen::Vector3d g = Eigen::Vector3d::Zero(), dx;
        for (int j = 0; j < N; ++j) {
            Eigen::Vector3d J;
            double x_j = x_data[j];
            double y_j = y_data[j];
            double tmp_j = exp(ae * x_j * x_j + be * x_j + ce);
            double err_j = y_j - tmp_j;
            // err = y - exp(ax^2 + bx + c)
            J[0] = -x_j * x_j * tmp_j;
            J[1] = -x_j * tmp_j;
            J[2] = -tmp_j;
            H += J * J.transpose();
            g += -err_j * J;
            loss += 0.5 * err_j * err_j;
        }
        dx = H.ldlt().solve(g);
        if (i > 0 && loss > lastCost) break;
        ae += dx[0];
        be += dx[1];
        ce += dx[2];
        lastCost = loss;
        cout << "iteration [" << i << "] total cost:" << cost << ", update:" << dx.transpose() << " estimated params:"
             << ae << "," << be << "," << ce << endl;
    }
    cout << "estimated abc = " << ae << " , " << be << " , " << ce << endl;

    return 0;
}

//{
//    ae = 2.0;
//    be = 1.0;
//    ce = 5.0;
//    //  开始求解时间t1
//    chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
//    for (int iter = 0; iter < iterations; iter++) {
//        //  增量方程左侧值初始化:Hessian = J^T W^{-1} J in Gauss-Newton
//        Eigen::Matrix3d H = Eigen::Matrix3d::Zero();
//        //  增量方程右侧值初始化:g = - W^{-1} ei J
//        Eigen::Vector3d g = Eigen::Vector3d::Zero();
//        //  估计同真实的差距值，归零化
//        cost = 0;
//
//        for(int i = 0; i < N; i++) {
//            //  获取第i个数据点
//            double xi = x_data[i], yi = y_data[i];
//            //  误差
//            double ei = yi - exp(ae * xi * xi + be * xi + ce);
//            //  雅克比矩阵
//            Eigen::Vector3d J;
//            J[0] = - xi * xi * exp(ae * xi * xi + be * xi + ce);
//            J[1] = - xi * exp(ae * xi * xi + be * xi + ce);
//            J[2] = - exp(ae * xi * xi + be * xi + ce);
//            //  增量方程系数
////            double inv_sigma = 1.0 / w_sigma;
////            H += inv_sigma * inv_sigma * J * J.transpose(); // 3 * 3
////            g += -inv_sigma * inv_sigma * ei * J;           // 3 * 1
//            H += J * J.transpose(); // 3 * 3
//            g += -ei * J;           // 3 * 1
//            //  差距
//            cost += ei * ei;
//        }
//        //  求解增量方程Hdx=g
//        Eigen::Vector3d dx = H.ldlt().solve(g);
//        if (isnan(dx[0])) {
//            cout << "result is nan !" << endl;
//            break;
//        }
//
//        if (iter > 0 && cost >= lastCost) {
//            cout << "cost:" << cost << ">= last cost :" << lastCost <<", break." << endl;
//            break;
//        }
//
//        //  更新估计值
//        ae += dx[0];
//        be += dx[1];
//        ce += dx[2];
//        //  更新差距
//        lastCost = cost;
//        cout << "iteration [" << iter << "] total cost:" << cost << ", update:" << dx.transpose() << " estimated params:" << ae << "," << be << "," << ce << endl;
//    }
//
//    //  结束求解时间t2
//    chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
//    //  求解花费时间t2 - t1
//    chrono::duration<double> time_used = chrono::duration_cast<chrono::duration<double>>(t2 - t1);
//    cout << "solve time cost = " << time_used.count() << "seconds. " << endl;
//    //  最终估计量
//    cout << "estimated abc = " << ae << " , " << be << " , " << ce << endl;
//    return 0;
//}

//#include "ceres/ceres.h"
//#include "glog/logging.h"
//#include <cstdlib>
//
//// clang-format on
//using ceres::AutoDiffCostFunction;
//using ceres::CauchyLoss;
//using ceres::CostFunction;
//using ceres::Problem;
//using ceres::Solve;
//using ceres::Solver;
//
//struct ExponentialResidual {
//    ExponentialResidual(double x, double y) : x_(x), y_(y) {}
//
//    template <typename T>
//    bool operator()(const T* const m, const T* const c, T* residual) const {
//        residual[0] = y_ - exp(m[0] * x_ + c[0]);
//        return true;
//    }
//
//private:
//    const double x_;
//    const double y_;
//};
//
//int main(int argc, char** argv) {
//    google::InitGoogleLogging(argv[0]);
//    // Data generated using the following octave code.
//
//    const int kNumObservations = 66;
//    std::srand(std::time(nullptr));    // use current time as seed for random generator
//    double m_r = 0.3;
//    double c_r = 0.1;
//    double data[kNumObservations * 2] = {};
//    for (int i = 0; i < kNumObservations; i += 2) {
//        data[i] = 1.0 * (std::rand() % 1000) / 100.0;     // x_ (0.0, 10.0)
//        data[i + 1] = exp(m_r * data[i] + c_r);          // y_
////        data[i + 1] = exp(m_r * data[i] + c_r) + 1.0 * rand() / RAND_MAX / 100; // y_
//        std::cout << data[i] << std::endl;
//    }
//
//    double m = 0.0;
//    double c = 0.0;
//
//    Problem problem;
//    for (int i = 0; i < kNumObservations; ++i) {
//        CostFunction* cost_function =
//                new AutoDiffCostFunction<ExponentialResidual, 1, 1, 1>(
//                        new ExponentialResidual(data[2 * i], data[2 * i + 1]));
//        problem.AddResidualBlock(cost_function, new CauchyLoss(0.5), &m, &c);
////        problem.AddResidualBlock(cost_function, nullptr, &m, &c);
//    }
//
//    Solver::Options options;
//    options.linear_solver_type = ceres::DENSE_QR;
//    options.minimizer_progress_to_stdout = true;
//
//    Solver::Summary summary;
//    Solve(options, &problem, &summary);
//    std::cout << summary.BriefReport() << "\n";
//    std::cout << "Initial m: " << 0.0 << " c: " << 0.0 << "\n";
//    std::cout << "Final   m: " << m << " c: " << c << "\n";
//    return 0;
//}
