#ifndef TRITUM_M_HASH_GRID_H
#define TRITUM_M_HASH_GRID_H

#include <cstdint>
#include <functional>

namespace tritium {

struct index_2d {
    uint64_t x_;
    uint64_t y_;
    bool operator < (const index_2d &other) {
        if (this->x_ == other.x_) {
            return this->y_ < other.y_;
        }
        return this->x_ < other.x_;
    }
};


struct index_3d {
    uint64_t x_;
    uint64_t y_;
    uint64_t z_;
    index_3d(uint64_t x, uint64_t y, uint64_t z) : x_(x), y_(y), z_(z) {}
    bool operator < (const index_3d &other) {
        if (this->x_ == other.x_) {
            if (this->y_ == other.y_) {
                return this->z_ < other.z_;
            }
            return this->y_ < other.y_;
        }
        return this->x_ < other.x_;
    }
};


// easy way
struct hash_2d {
    size_t operator()(const index_2d &idx) {
        return static_cast<uint64_t>(idx.x_) << 32 + static_cast<uint64_t>(idx.y_);
    }
};


struct hash_3d {
    size_t operator()(const index_3d &idx) {
        return static_cast<uint64_t>(idx.x_) << 32 + static_cast<uint64_t>(idx.y_);
    }
};


// better way
// boost::hash_combine
template<class T>
inline void hash_combine(size_t &seed, const T &v) {
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct hash_2d_bt {
    size_t operator()(const index_2d &idx) {
        size_t res = std::hash<uint64_t>{}(idx.x_);
        hash_combine(res, idx.y_);
        return res;
    }
};

struct hash_3d_bt {
    size_t operator()(const index_3d &idx) {
        size_t res = std::hash<uint64_t>{}(idx.x_);
        hash_combine(res, idx.y_);
        hash_combine(res, idx.z_);
        return res;
    }
};

}
#endif //TRITUM_M_HASH_GRID_H