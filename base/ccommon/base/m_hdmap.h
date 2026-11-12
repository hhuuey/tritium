#ifndef TRITUM_M_HDMAP_H
#define TRITUM_M_HDMAP_H

#include <vector>
#include <string>

namespace tritium {

enum class line_tpye {
    YELLOW_SINGLE_DASH_LINE,
    YELLOW_SINGLE_SOLID_LINE,
    WHILE_SINGLE_DASH_LINE,
    WHILE_SINGLE_SOLID_LINE,
    YELLOW_DOUBLE_DASH_LINE,
    YELLOW_DOUBLE_SOLID_LINE,
    WHILE_DOUBLE_DASH_LINE,
    WHILE_DOUBLE_SOLID_LINE
};

struct point {
    double x;
    double y;
    double z;
};


struct basis_map_element {
    std::string map_id;
    std::vector<point> geometry;
};

struct boundary : basis_map_element {

};


struct lane : basis_map_element {
    std::string road_id;
    size_t lane_index; // 左1, 2, 3 ...车道
    boundary left_boundary;
    boundary right_boundary;
    std::string next_lane_id;
    std::string prev_lane_id;
};

struct road : basis_map_element {
    boundary left_boundary;
    boundary right_boundary;
    std::string next_road_id;
    std::string prev_road_id;
};

}
#endif //TRITUM_M_HDMAP_H
