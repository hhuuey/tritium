//
// Created by huanghh on 2023/2/9.
//

#ifndef TRITUM_M_LOCAL_MAP_H
#define TRITUM_M_LOCAL_MAP_H

#include <m_hash_grid.h>

namespace tritium {

enum class LABEL_TYPE {
    UNKNOWN,
    GROUND,
    LANE_BOUNDARY,
    ROAD_BOUNDARY,
    STOP_LINE,
    CROSS_LINE,
    ROAD_MARK,
    TREE,
    BUILDING,
    COUNT
};

struct point {
    double x_;
    double y_;
    double z_;
    LABEL_TYPE label_ = LABEL_TYPE::UNKNOWN;
};

class grid {
public:
    grid(const point &p) : p_(p) { label_index_[static_cast<int>(p.label_)] = 1; }
    void vote(const point &p);

private:
    point p_;
    size_t label_index_[static_cast<int>(LABEL_TYPE::COUNT)];
};

class local_map {
public:
    local_map(const double &resolution, const double &radius) : resolution_(resolution), radius_(radius){}
    void insert(const point &p) {
        //构建或重新插入新的观测点
        index_3d i(p.x_ / resolution_, p.y_ / resolution_, p.z_ / resolution_);
        if (map_index_.find(i) != map_index_.end()) {
            map_index_[i].vote(p);
        } else {
            map_index_.insert(std::make_pair(i, grid(p)));
        }
    }
    void get_map();

private:
    std::unordered_map<index_3d, grid, hash_3d> map_index_;
    const double resolution_;
    const double radius_;
};


}

#endif //TRITUM_M_LOCAL_MAP_H
