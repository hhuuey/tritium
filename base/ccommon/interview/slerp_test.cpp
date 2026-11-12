#include <iostream>
#include <vector>
#include <list>
#include <deque>
#include <array>
#include <algorithm>
#include <map>
#include <assert.h>
#include <iterator>
#include <iomanip>
#include <cmath>
#include <Eigen/Core>
#include <Eigen/Dense>

#define USE_EIGEN 1

using TimestampStuct = uint64_t;
using PoseStruct = std::array<double, 7>;
using QuatStruct = std::array<double, 4>;



#define REGISTER_TIMESTAMP(Timestamp) \
template<class T> \
inline TimestampStuct GetTimestamp(const T &ts, std::decay_t<decltype(ts.Timestamp)>* = nullptr) { \
    return ts.Timestamp;\
}

#define REGISTER_POSE(Pose) \
template<class T> \
inline PoseStruct GetPose(const T &ts, std::decay_t<decltype(ts.Pose)>* = nullptr) { \
    return ts.Pose;\
} \
template<class T> \
inline void SetPose(T &ts, const PoseStruct &p, std::decay_t<decltype(ts.Pose)>* = nullptr) { \
   ts.Pose = p;\
}

#define REGISTER_TIMESTAMP_POSE(Timestamp, Pose) \
REGISTER_TIMESTAMP(Timestamp)\
REGISTER_POSE(Pose)\


struct TimePose {
    TimestampStuct timestamp_; // microseconds
    PoseStruct pose_;          // t_x, t_y, t_z, r_w, r_x, r_y, r_z
};

struct TimePose1 {
    TimestampStuct timestamp1_; // microseconds
    PoseStruct pose1_;          // t_x, t_y, t_z, r_w, r_x, r_y, r_z
};

REGISTER_TIMESTAMP_POSE(timestamp_, pose_);
REGISTER_TIMESTAMP_POSE(timestamp1_, pose1_);

QuatStruct operator *(const QuatStruct &p, const QuatStruct &q) {
    QuatStruct res;
    res[0] = p[0] * q[0] - p[1] * q[1] - p[2] * q[2] - p[3] *q[3];
    res[1] = p[1] * q[0] + p[0] * q[1] - p[3] * q[2] + p[2] *q[3];
    res[2] = p[2] * q[0] + p[3] * q[1] + p[0] * q[2] - p[1] *q[3];
    res[3] = p[3] * q[0] - p[2] * q[1] + p[1] * q[2] + p[0] *q[3];
    return res;
}

QuatStruct operator -(const QuatStruct &p) {
    QuatStruct res{p[0], -p[1], -p[2], -p[3]};
    return res;
}

template <class T1,  class T2, template<class Elem1, class Elem2>  class Ts>
void Interp(const Ts<T1, std::allocator<T1>> &ts, T2 & t) {
    assert(ts.size() >= 2);
    std::map<TimestampStuct, T1> ts_map;
    std::for_each(ts.begin(), ts.end(), [&ts_map] (const T1 &t) {ts_map.insert(std::make_pair(GetTimestamp(t), t));});
    Interp(ts_map, t);
}

template <class T1,  class T2>
void Interp(const std::map<TimestampStuct, T1> &ts, T2 & t) {
    // precondition
    assert(ts.size() >= 2);
    for (const auto &it : ts) {
        assert(GetTimestamp(it.second) == it.first);
    }

    // find nearest two frame
    const auto &tmp = ts.lower_bound(GetTimestamp(t));
    const auto &right = tmp == ts.end() ? std::prev(tmp) : (tmp == ts.begin() ? std::next(tmp) : tmp);
    const auto &left = std::prev(right);
    double ratio = 1.0 * (GetTimestamp(t) - left->first) / ((right->first - left->first));

    // translation
    PoseStruct res;
    PoseStruct lp = GetPose(left->second);
    PoseStruct rp = GetPose(right->second);
    std::array<double, 3> lt{lp[0], lp[1], lp[2]};
    std::array<double, 3> rt{rp[0], rp[1], rp[2]};
    for (const size_t i : {0, 1, 2}) {
        res[i] = lt[i] + ratio * (rt[i] - lt[i]);
    }

    // rotation
#ifdef USE_EIGEN
    Eigen::Quaterniond lq(lp[3], lp[4], lp[5], lp[6]);
    Eigen::Quaterniond rq(rp[3], rp[4], rp[5], rp[6]);

    Eigen::Quaterniond dq = lq.slerp(ratio, rq);
    Eigen::Quaterniond q = lq * dq;
    res[3] = q.w();
    res[4] = q.x();
    res[5] = q.y();
    res[6] = q.z();
#else
    QuatStruct lq, rq;
    std::copy_n(lp.begin() + 3, 4, lq.begin());
    std::copy_n(rp.begin() + 3, 4, rq.begin());
    QuatStruct dq = (-lq) * rq;
    double half_theta = std::acos(dq[0]);
    double ration_q = std::sin(half_theta * ratio) > std::numeric_limits<double>::min() ? std::sin(half_theta * ratio) / std::sin(half_theta) : 0.;
    QuatStruct dq_intp = {std::cos(half_theta * ratio), ration_q * dq[1], ration_q * dq[2], ration_q * dq[3]};
    QuatStruct q = lq * dq_intp;
    std::copy(q.begin(), q.end(), res.begin() + 3);
#endif

    SetPose(t, res);
}


int main() {
    TimePose t1 = {0,{0,0,0,1,0,0,0}};
    TimePose t2 = {6, {0,4,0,0.8,0.6,0,0}};
    TimePose t3 = {13, {0,4,0,0.6,0.8,0,0}};
    std::vector<TimePose> tp_vec = {t1, t2, t3};
    std::list<TimePose> tp_lst = {t1, t2, t3};
    std::map<TimestampStuct , TimePose> tp_map = {{GetTimestamp(t1), t1}, {GetTimestamp(t2), t2}};

    TimePose1 t = {12, {}};

    Interp(tp_vec, t);
    Interp(tp_lst, t);
    Interp(tp_map, t);

    const auto &pose = GetPose(t);
    std::cout << std::fixed << std::setprecision(2);
    std::copy(pose.begin(), pose.end(), std::ostream_iterator<double>(std::cout, " "));

    return 0;
}
