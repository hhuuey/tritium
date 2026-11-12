#include <iostream>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <ostream>
#include <numeric>
#include <cmath>
#include <iterator>

template<class T, size_t N =0>
class PointX {
public:
    PointX(const std::initializer_list<T> &l): data_(l) {
        assert(N? data_.size() == N : true);
    }

    T& operator[] (const size_t& i) {
        assert(i < data_.size());
        return data_[i];
    }

    T operator[] (const size_t& i) const {
        assert(i < data_.size());
        return data_[i];
    }

    size_t Size() const {
        return data_.size();
    }

    template<class T1, size_t N1 = 0>
    PointX operator-(const PointX<T1, N1> &other) const {
        assert(this->Size() == other.Size());
        PointX res = *this;
        size_t i = 0;
        std::for_each(res.data_.begin(), res.data_.end(), [other, &i] (T &t)  { t -= other[i++];});
        return res;
    }

    template<class T1, size_t N1 = 0>
    bool operator <(const PointX<T1, N1> &other) const {
        assert(this->Size() == other.Size());
        std::vector<size_t> index(data_.size());
        std::iota(index.begin(), index.end(), 0);
        for (const auto &it : index) {
            if (data_[it] < other[it]) return true;
            if (data_[it] > other[it]) return false;
        }
        return false;
    }

    T Distance() {
        T sum = 0.;
        std::for_each(data_.begin(), data_.end(), [&sum] (const T&t) {sum += t * t;});
        return std::sqrt(sum);
    }

    friend std::ostream& operator<<(std::ostream& out, const PointX &p) {
        std::copy(p.data_.begin(), p.data_.end(), std::ostream_iterator<T>(out, " "));
        return out;
    }

private:
    std::vector<T> data_;
};

template <class T, size_t N1 = 0, size_t N2 = 0>
T GetDistance(const PointX<T, N1> &p1, const PointX<T, N2> &p2) {
    return (p1 - p2).Distance();
}

using PointXd = PointX<double>;
using Point3d = PointX<double, 3>;

int main() {
    PointXd p1({1.0, 2.0, 3.0});
    PointXd p2 = {1.0, 2.0, 2.0};
    Point3d p3({2.0, 3.0, 2.0});
    Point3d p4 = p3;
    p4[0] = 6.;

    std::cout << (p3 - p4) << std::endl;
    std::cout <<  (p3 - p4).Distance() << std::endl;

    std::cout << GetDistance(p1, p2) << std::endl;
    std::cout << GetDistance(p1, p3) << std::endl;
    std::cout << GetDistance(p1, {0., 0., 0.}) << std::endl;
    std::cout << GetDistance(p3, p4) << std::endl;

    std::cout << (p1 < p2 ?  "p1 <  p2" : "p1 >= p2") << std::endl;
    std::cout << (p1 < p3 ?  "p1 <  p3" : "p1 >= p3") << std::endl;
    return 0;
}
