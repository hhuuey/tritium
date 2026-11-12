#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <limits>

enum class ErrorCode {
    RANSAC_SUCCESS,
    RANSAC_ERROR_INSUFFICIENT_DATA,
    RANSAC_ERROR_MAX_ITERATIONS_REACHED
};

// ========================2D直线拟合=============================================
struct Point {
    double x, y;
    bool operator==(const Point& other) const {
        double epsilon = 1e-6;  // 设置一个很小的阈值，根据需要调整
        double distance = std::sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y));
        return distance < epsilon;
    }
};

// 计算点到直线距离
double DistanceToLine(double a, double b, double c, const Point& p) {
    return std::abs(a * p.x + b * p.y + c) / std::sqrt(a * a + b * b);
}


// 计算直线参数
std::vector<double> GetLineParam(const Point &p1, const Point &p2) {
    // 如果点完全重复
    if (p1 == p2) {
        return std::vector<double>();
    }
    // 计算直线参数 (a, b, c)
    double a = p2.y - p1.y;
    double b = p1.x - p2.x;
    double c = p1.y * (p2.x - p1.x) - p1.x * (p2.y - p1.y);
    return {a, b, c};
}

// ========================3D平面拟合=============================================
struct Point3D {
    double x, y, z;
    bool operator==(const Point3D& other) const {
        double epsilon = 1e-6;  // 设置一个很小的阈值，根据需要调整
        double distance = std::sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y) + (z - other.z) * (z - other.z));
        return distance < epsilon;
    }
};

// 计算点到平面距离
double DistanceToPlane(double a, double b, double c, double d, const Point3D& p) {
    return std::abs(a * p.x + b * p.y + c * p.z + d) / std::sqrt(a * a + b * b + c * c);
}

// 计算平面参数
std::vector<double> GetPlaneParam(const Point3D &p1, const Point3D &p2, const Point3D &p3) {
    // 如果点完全重复
    if (p1 == p2 || p1 == p3 || p2 == p3) {
        return std::vector<double>();
    }
    // 计算平面参数 (a, b, c, d)
    double x1 = p1.x;
    double y1 = p1.y;
    double z1 = p1.z;
    double x2 = p2.x;
    double y2 = p2.y;
    double z2 = p2.z;
    double x3 = p3.x;
    double y3 = p3.y;
    double z3 = p3.z;

    double a = ((y2 - y1) * (z3 - z1) - (z2 - z1) * (y3 - y1));
    double b = ((z2 - z1) * (x3 - x1) - (x2 - x1) * (z3 - z1));
    double c = ((x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1));
    double d = -(a * x1 + b * y1 + c * z1);
    return {a, b, c, d};
}

// RANSAC拟合直线
ErrorCode FitLineRANSAC(const std::vector<Point>& data, double distanceThreshold, int maxIterations, double& bestA, double& bestB, double& bestC) {
    int dataSize = data.size();
    if (dataSize < 2) {
        return ErrorCode::RANSAC_ERROR_INSUFFICIENT_DATA;
    }

    int maxInliers = 0;

    for (int i = 0; i < maxIterations; ++i) {
        // 随机选择两数据点
        int index1 = std::rand() % dataSize;
        int index2 = std::rand() % dataSize;
        Point p1 = data[index1];
        Point p2 = data[index2];

        // 计算直线参数 (a, b, c)
        std::vector<double> param = GetLineParam(p1, p2);
        if (param.empty()) continue;

        double a = param[0];
        double b = param[1];
        double c = param[2];

        // 统计内点数量
        int inliers = 0;
        for (const Point& point : data) {
            double dist = DistanceToLine(a, b, c, point);
            if (dist < distanceThreshold) {
                inliers++;
            }
        }

        if (inliers > maxInliers) {
            maxInliers = inliers;
            bestA = a;
            bestB = b;
            bestC = c;
        }
    }

    if (maxInliers == 0) {
        return ErrorCode::RANSAC_ERROR_MAX_ITERATIONS_REACHED;
    }

    return ErrorCode::RANSAC_SUCCESS;
}

int main() {
    // 随机生成一些数据点
    const int numPoints = 100;
    std::vector<Point> data;
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    for (int i = 0; i < numPoints; ++i) {
        double x = i / 10.0;
        double y = 2.0 * x + 1.0 + (std::rand() % 20 - 10);  // 生成带有噪声的数据
        data.push_back({x, y});
    }

    double bestA, bestB, bestC;
    double distanceThreshold = 2.0;
    int maxIterations = 1000;

    ErrorCode result = FitLineRANSAC(data, distanceThreshold, maxIterations, bestA, bestB, bestC);

    if (result == ErrorCode::RANSAC_SUCCESS) {
        std::cout << "Best Fit Line: " << bestA << "x + " << bestB << "y + " << bestC << " = 0" << std::endl;
    } else if (result == ErrorCode::RANSAC_ERROR_INSUFFICIENT_DATA) {
        std::cout << "Error: Insufficient data to perform RANSAC." << std::endl;
    } else if (result == ErrorCode::RANSAC_ERROR_MAX_ITERATIONS_REACHED) {
        std::cout << "Error: Maximum iterations reached without finding a valid line." << std::endl;
    }

    return 0;
}