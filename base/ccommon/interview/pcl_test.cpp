#include <iostream>
#include <unordered_map>
#include <thread>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/visualization/cloud_viewer.h>

std::unordered_map<uint32_t, std::vector<int>> color_map = {
{0, {0, 0, 0}},
{  1, {0, 0, 255}},
{ 10, {245, 150, 100}},
{ 11, {245, 230, 100}},
{ 13, {250, 80, 100}},
{ 15, {150, 60, 30}},
{ 16, {255, 0, 0}},
{ 18, {180, 30, 80}},
{ 20, {255, 0, 0}},
{ 30, {30, 30, 255}},
{ 31, {200, 40, 255}},
{ 32, {90, 30, 150}},
{ 40, {255, 0, 255}},
{ 44, {255, 150, 255}},
{ 48, {75, 0, 75}},
{ 49, {75, 0, 175}},
{ 50, {0, 200, 255}},
{ 51, {50, 120, 255}},
{ 52, {0, 150, 255}},
{ 60, {170, 255, 150}},
{ 70, {0, 175, 0}},
{ 71, {0, 60, 135}},
{ 72, {80, 240, 150}},
{ 80, {150, 240, 255}},
{ 81, {0, 0, 255}},
{ 99, {255, 255, 50}},
{252, {245, 150, 100}},
{256, {255, 0, 0}},
{253, {200, 40, 255}},
{254, {30, 30, 255}},
{255, {90, 30, 150}},
{257, {250, 80, 100}},
{258, {180, 30, 80}},
{259, {255, 0, 0}}};

int loadBin_with_Label(const std::string& binName, const std::string& labelName, pcl::PointCloud<pcl::PointXYZL>::Ptr& p_cloud_l,
                                                                                 pcl::PointCloud<pcl::PointXYZRGB>::Ptr& cld) {
    int32_t num = 1000000;
    float* data = (float*)malloc(num * sizeof(float));
    uint32_t* data_label = (uint32_t*)malloc(num * sizeof(uint32_t));

    // 点
    float* px = data + 0;
    float* py = data + 1;
    float* pz = data + 2;
    float* pr = data + 3;           //反射强度
    uint32_t* pl = data_label + 0;  // label

    FILE* stream;
    FILE* label_stream;
    const char* filenameInput = binName.c_str();
    const char* labelnameInput = labelName.c_str();

    stream = fopen(filenameInput, "rb");
    label_stream = fopen(labelnameInput, "rb");

    num = fread(data, sizeof(float), num, stream) / 4;  //读入点云数据，大概10万+个点
    auto r = fread(data_label, sizeof(uint32_t), num, label_stream);

    // 创建点云
    // pcl::PointCloud<pcl::PointXYZI>::Ptr p_cloud_i(new pcl::PointCloud<pcl::PointXYZI>());
    for (size_t i = 0; i < num; i++) {
        pcl::PointXYZL tmp;
        tmp.x = *px;
        tmp.y = *py;
        tmp.z = *pz;

        uint32_t upper_half = (*pl) >> 16;
        uint32_t lower_half = (*pl) & 0xffff; // label = (upper_half << 16) + lower_half   # reconstruct full label
        tmp.label = lower_half;
        p_cloud_l->points.push_back(tmp);

        pcl::PointXYZRGB cld_rgb;
        cld_rgb.x = tmp.x;
        cld_rgb.y = tmp.y;
        cld_rgb.z = tmp.z;

        std::uint8_t r(255), g(255), b(255);
        if (color_map.find(tmp.label) != color_map.end()) {
            auto color = color_map[tmp.label];
            b = color[0];
            g = color[1];
            r = color[2];
        }
        std::uint32_t rgb = (static_cast<std::uint32_t>(r) << 16 |
                             static_cast<std::uint32_t>(g) << 8 | static_cast<std::uint32_t>(b));
        cld_rgb.rgb = *reinterpret_cast<float*>(&rgb);
        cld->points.push_back(cld_rgb);

        px += 4;
        py += 4;
        pz += 4;
        pr += 4;
        pl += 1;
    }

    p_cloud_l->width = p_cloud_l->size();
    p_cloud_l->height = 1;

    cld->width = cld->size();
    cld->height = 1;

    fclose(stream);
    fclose(label_stream);

    std::cout << "load point cloud size: " <<  p_cloud_l->width << std::endl;
    return 1;
}

void displayCloud(pcl::PointCloud<pcl::PointXYZI>::Ptr cloud, const std::string& window_name)
{
    if (cloud->size() < 1)
    {
        std::cout << window_name << " display failure. Cloud contains no points\n";
        return;
    }

    boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer(window_name));
    pcl::visualization::PointCloudColorHandlerGenericField<pcl::PointXYZI> point_cloud_color_handler(cloud, "intensity");

    viewer->addPointCloud< pcl::PointXYZI >(cloud, point_cloud_color_handler, "id");
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 2, "id");

//    viewer->registerKeyboardCallback(keyboardEventOccurred, (void*)viewer.get());

    while (!viewer->wasStopped()){
        viewer->spinOnce(50);
    }
//    close_window = false;
    viewer->close();
}

int main () {
    std::string base_path = "E:\\kitti2\\semantic\\dataset\\sequences\\00\\";
    std::string index = "000005";
    std::string bin_path = base_path + "\\velodyne\\" + index + ".bin";
    std::string label_path = base_path + "\\labels\\" + index + ".label";

    pcl::PointCloud<pcl::PointXYZL>::Ptr pl(new pcl::PointCloud<pcl::PointXYZL>);
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZRGB>);

    loadBin_with_Label(bin_path, label_path, pl, cloud);

    pcl::visualization::PCLVisualizer::Ptr viewer (new pcl::visualization::PCLVisualizer ("3D Viewer"));
    viewer->setBackgroundColor (255, 255, 255);



    pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> rgb(cloud);
    viewer->addPointCloud<pcl::PointXYZRGB> (cloud, rgb, "sample cloud");
    viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
    viewer->addCoordinateSystem (1.0);
    viewer->initCameraParameters ();

    while (!viewer->wasStopped ())
    {
        viewer->spinOnce (100);
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }

    return 0;

    pcl::PointCloud <pcl::PointXYZ> cloud1;

    // Fill in the cloud data
    cloud1.width = 5;
    cloud1.height = 1;
    cloud1.is_dense = false;
    cloud1.resize(cloud1.width * cloud1.height);
    for (auto &point: cloud1) {
        point.x = 1024 * rand() / (RAND_MAX + 1.0f);
        point.y = 1024 * rand() / (RAND_MAX + 1.0f);
        point.z = 1024 * rand() / (RAND_MAX + 1.0f);
    }
    pcl::io::savePCDFileASCII("D:\\project\\clion\\tritium\\test\\test_pcd.pcd", cloud1);
    std::cerr << "Saved " << cloud1.size() << " data points to test_pcd.pcd." << std::endl;
    for (const auto &point: cloud1)
        std::cerr << "    " << point.x << " " << point.y << " " << point.z << std::endl;
    return (0);
}

/*
labels:
0 : "unlabeled"
1 : "outlier"
10: "car"
11: "bicycle"
13: "bus"
15: "motorcycle"
16: "on-rails"
18: "truck"
20: "other-vehicle"
30: "person"
31: "bicyclist"
32: "motorcyclist"
40: "road"
44: "parking"
48: "sidewalk"
49: "other-ground"
50: "building"
51: "fence"
52: "other-structure"
60: "lane-marking"
70: "vegetation"
71: "trunk"
72: "terrain"
80: "pole"
81: "traffic-sign"
99: "other-object"
252: "moving-car"
253: "moving-bicyclist"
254: "moving-person"
255: "moving-motorcyclist"
256: "moving-on-rails"
257: "moving-bus"
258: "moving-truck"
259: "moving-other-vehicle"

color_map: # bgr
  0: [0, 0, 0]
  1: [0, 0, 255]
 10: [245, 150, 100]
 11: [245, 230, 100]
 13: [250, 80, 100]
 15: [150, 60, 30]
 16: [255, 0, 0]
 18: [180, 30, 80]
 20: [255, 0, 0]
 30: [30, 30, 255]
 31: [200, 40, 255]
 32: [90, 30, 150]
 40: [255, 0, 255]
 44: [255, 150, 255]
 48: [75, 0, 75]
 49: [75, 0, 175]
 50: [0, 200, 255]
 51: [50, 120, 255]
 52: [0, 150, 255]
 60: [170, 255, 150]
 70: [0, 175, 0]
 71: [0, 60, 135]
 72: [80, 240, 150]
 80: [150, 240, 255]
 81: [0, 0, 255]
 99: [255, 255, 50]
252: [245, 150, 100]
256: [255, 0, 0]
253: [200, 40, 255]
254: [30, 30, 255]
255: [90, 30, 150]
257: [250, 80, 100]
258: [180, 30, 80]
259: [255, 0, 0]

*/