# tritium
commonly used base code

## common
* m_time.h

计时器类，在对象析构时，解析中间各个环节耗时

* m_sort.h

排序方法包括插入排序, 冒泡排序, 快速排序, 归并排序

* m_io.h

文件接口包括创建/删除文件夹， 递归枚举文件夹的文件， 创建和读取类csv文件

* template/auto_register.h

自动注册工厂类，可以使用字符串（"msg1"）调用对应的具体消息类型(class msg1)

* template/det_member_variable_exist.h

判断一个类内部是否有变量名称为MemberName的成员， 提供"判断有无"以及"赋值"的两个方法

* m_hash_grid.h

2d栅格及3d体素哈希函数的实现方法

* base/singleton.h

单例的宏定义实现

* base/m_hdmap.h

基础高精地图元素，车道(lane), 道路(road), 边界线(boundary)

* base/m_threadpool.h

挪用的线程池库

* base/m_struct.h

基础数据结构类型，循环队列， 二叉树

## test
* ceres_test

分別使用ceres 和Eigen手写高斯牛顿进行非线性优化求解y = exp(ax^2 + bx + c)进行曲线拟合

* opencv_test

手动选取了空间4条直线在双目相机（cam0, cam1）的像素坐标

1. 记录了cam1相机在旋转小角度后的图像的变化

2. 调整后的cam1像素来计算真实的空间直线导致的角度偏差

3. 可视化cam0原始直线投影, cam1原始直线投影以及有角度偏差的直线重投影回cam1， cam1旋转后的图片这三张图片

* slerp_test

【高翔SLAM常见的面试题】

pose的时间插值是常见需求。写一个6自由度pose的插值函数。pose和时间戳是自定义的结构体，例如 std::vector<TimedPose> 之类。TimedPose.time_stamp为时间，TimedPose.pose是位姿。

让函数支持其他的常见有序容器，如std::list<TimedPose>和std::map<double, TimedPose>。

让函数支持自定义取时间戳的方式和取pose的方式，因为其他的结构体中时间戳变量和pose变量名称可能不同，例如TimedPose里的时间戳字段是time_stamp，而TimedPose2里的则是time_。

* geojson_test

常见的点线面geojson文件生成

* pcl_test

* rot_test

* pointx_test

写一个求两点的距离函数。要求支持常见的2维、3维至任意有限高维点【高翔SLAM常见的面试题】

* m_thread_test

不用库写一个并行的for循环。输入两个迭代器和一个lambda，例如for_each(begin, end, [](int i){cout<<i;}); 【高翔SLAM常见的面试题】

* plane_fitting_using_least_square

最小二乘法拟合平面

## note

* find_package.md 

find_package命令两种查找方式及生成自定义findlibrary.cmake文件方法

* win_common_library_install.md 

windows编译常用库过程, PCL, OpenCV, Boost, glog, gflags, Eigen, ceres, libpng, zlib

## script

## app

## todo

hash_list

euroc/nuscense dataset loader

boost-geometry