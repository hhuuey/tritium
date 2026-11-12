#ifndef TRITUM_DATA_KITTI_H
#define TRITUM_DATA_KITTI_H

#define cimg_use_png
#include "data.h"
#include "m_io.h"

#include <sstream>

/// odometry_gray 21.5G
class m_dataset_kitti : public m_dataset {
public:
    m_dataset_kitti(std::string &root) : m_dataset(root) {
        init();
    }
    virtual m_data get_item(const size_t &index) override {
        m_data data;
        /// load timestamp
        data.timestamp_= timestamps_[index][0] * 1.e3;
        /// load img
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(6) << index;
        std::string fileCam0 = root_ + "/image_0/" + ss.str() + ".png";
        std::string fileCam1 = root_ + "/image_1/" + ss.str() + ".png";
        data.img_.emplace_back(fileCam0.c_str());
        data.img_.emplace_back(fileCam1.c_str());
        /// load groundtruth
        Eigen::MatrixXd gtRT(3, 4);
        for (int i = 0; i < 12; i++) {
            gtRT(i / 4, i % 4) = groundtruth_[index][i];
        }
        data.r_ = gtRT.block<3, 3>(0, 0);
        data.t_ = gtRT.block<3, 1>(0, 3);
        return data;
    }
    virtual size_t size() override {
        return timestamps_.size();
    }

protected:
    virtual void init() override {
        std::string file_ts = root_ + "/times.txt";
        tritium::read_txt(file_ts, timestamps_);
        std::string file_gt = root_ + "/gt.txt";
        tritium::read_txt(file_gt, groundtruth_);
    }

private:
    std::vector<std::vector<double>> timestamps_; // size: N x 1
    std::vector<std::vector<double>> groundtruth_; // size: N x 12
};

#endif //TRITUM_DATA_KITTI_H
