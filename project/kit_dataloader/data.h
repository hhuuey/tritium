#ifndef TRITUM_DATA_LOADER_H
#define TRITUM_DATA_LOADER_H

#include "Eigen/Core"
#include "Eigen/Dense"
#include "Cimg-3.2.0/CImg.h"

#include <string>
#include <memory>
#include <vector>

struct m_data {
    /// millisecond
    uint64_t timestamp_;
    /// groundtruth
    Eigen::Matrix3d r_;
    Eigen::Vector3d t_;
    /// imu
    Eigen::Vector3d acc_;
    Eigen::Vector3d omg_;
    Eigen::Vector3d mag_;
    /// multi-img
    std::vector<cimg_library::CImg<unsigned char>> img_;
    /// lidar: x, y, z, intensity, ring
    std::vector<std::array<double, 5>> lidar_;
};

class m_dataset {
public:
    m_dataset(const std::string &root) : root_(root) {}
    virtual m_data get_item(const size_t &index) = 0;
    virtual size_t size() = 0;

protected:
    virtual void init() {};
    std::string root_;
};


class m_dataloader {
public:
    m_dataloader(std::unique_ptr<m_dataset> &dataset, bool shuffle = false, size_t batch_size = 1) : dataset_(std::move(dataset)),
    shuffle_(shuffle), batch_size_(batch_size) {}

    bool load(m_data &data) {
        if (cur_index == dataset_->size()) return false;
        data = dataset_->get_item(cur_index++);
        return true;
    }

private:
    std::unique_ptr<m_dataset> dataset_;
    bool shuffle_;
    size_t batch_size_;
    size_t cur_index = 0;
};

#endif //TRITUM_DATA_LOADER_H
