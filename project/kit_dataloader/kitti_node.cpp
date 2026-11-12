#include "data_kitti.h"

int main()
{
    std::string data_path = "E:/kitti2/zip/08";
    std::unique_ptr<m_dataset> dataset(std::make_unique<m_dataset_kitti>(data_path));
    m_dataloader dataloader(dataset);

    m_data data;
    (void)dataloader.load(data);
    cimg_library::CImg<unsigned char> image = data.img_[0];
    cimg_library::CImgDisplay main_disp1(image, "img0");
    while(dataloader.load(data)) {
        printf("load timestamp: %lld\n", data.timestamp_);
        image = data.img_[0];
        image.display(main_disp1);
        main_disp1.wait(100);
    }
    return 0;
}