#include <iostream>
#include <cmath>
// f(y) = y^2 - x // 平方根求解
// f`(y) = 2y
// y_n+1 = y_n - f(y)/f`(y) // 牛顿法
// y_n+1 = y_n - (y_n^2 - x) / 2y_n
// y_n+1 = 0.5 * (y_n + x / y_n)

double MySqrt(int x) {
    if (x <= 1) {
        return x;
    }
    double y = x;
    double epi = 1e-6;

    while (true) {
        double nextY = 0.5 * (y + x / y);
        if (std::abs(y - nextY) < epi) {
            return y;
        }
        y = nextY;
    }
}

int main()
{
//    int x = 25;
    int x = 26;
//    int x = 37;
    double result = MySqrt(x);
    double result1 = sqrt(x);
    std::cout << result << std::endl;
    std::cout << result1 << std::endl;
    return 0;
}