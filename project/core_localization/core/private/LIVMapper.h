/* 
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.

Developer: Chunran Zheng <zhengcr@connect.hku.hk>

For commercial use, please contact me at <zhengcr@connect.hku.hk> or
Prof. Fu Zhang at <fuzhang@hku.hk>.

This file is subject to the terms and conditions outlined in the 'LICENSE' file,
which is included as part of this source code package.
*/

#ifndef LIV_MAPPER_H
#define LIV_MAPPER_H

#include "IMU_Processing.h"
#include "fast_livo/measurements.h"
#include "fast_livo/config.h"
#include "vio.h"
#include "preprocess.h"








#include <GeographicLib/LocalCartesian.hpp>

class LIVMapper
{
public:
  fast_livo::Config config_;
  fast_livo::Outputs outputs_;
  bool pos_opend = false, first_gps = false, first_imu_save = true;
  int ocount = 0, frame_counter = 0, pub_num = 1, scan_wait_num = 0;
  double last_t_from_lidar_end_time = 0;
  LIVMapper(const fast_livo::Config &config, fast_livo::Outputs outputs);
  ~LIVMapper();
  bool process_one();
  void trace_state(const std::string &stage);
  void initializeComponents();
  void initializeFiles();
  void finish();
  void gravityAlignment();
  void handleFirstFrame();
  void stateEstimationAndMapping();
  void handleVIO();
  void handleLIO();
  void handleRTK();
  void savePCD();
  void processImu();
  
  bool sync_packages(LidarMeasureGroup &meas);
  void prop_imu_once(StatesGroup &imu_prop_state, const double dt, V3D acc_avr, V3D angvel_avr);
  void imu_prop_callback();
  void transformLidar(const Eigen::Matrix3d rot, const Eigen::Vector3d t, const PointCloudXYZI::Ptr &input_cloud, PointCloudXYZI::Ptr &trans_cloud);
  void pointBodyToWorld(const PointType &pi, PointType &po);
 
  void RGBpointBodyToWorld(PointType const *const pi, PointType *const po);
  void standard_pcl_cbk(const fast_livo::CloudSample::ConstPtr &msg);
  void livox_pcl_cbk(const fast_livo::LivoxSample::ConstPtr &msg_in);
  void imu_cbk(const fast_livo::ImuSample::ConstPtr &msg_in);
  void img_cbk(const fast_livo::ImageSample::ConstPtr &msg_in);
  void rtk_cbk(const fast_livo::GnssSample::ConstPtr& gpsMsg);
  void InitializeRTK();
  void publish_img_rgb(const std::string &pubImage, VIOManagerPtr vio_manager);
  void publish_frame_world(const std::string &pubLaserCloudFullRes, VIOManagerPtr vio_manager);
  void publish_visual_sub_map(const std::string &pubSubVisualMap);
  void publish_effect_world(const std::string &pubLaserCloudEffect, const std::vector<PointToPlane> &ptpl_list);
  void publish_odometry(const std::string &pubOdomAftMapped);
  void publish_mavros(const std::string &mavros_pose_publisher);
  void publish_path(const std::string pubPath);
  void readParameters(const fast_livo::Config &config);
  template <typename T> void set_posestamp(T &out);
  template <typename T> void pointBodyToWorld(const Eigen::Matrix<T, 3, 1> &pi, Eigen::Matrix<T, 3, 1> &po);
  template <typename T> Eigen::Matrix<T, 3, 1> pointBodyToWorld(const Eigen::Matrix<T, 3, 1> &pi);
  cv::Mat getImageFromMsg(const fast_livo::ImageSample::ConstPtr &img_msg);

  std::mutex mtx_buffer, mtx_buffer_imu_prop;
  std::condition_variable sig_buffer;

  SLAM_MODE slam_mode_;
  std::unordered_map<VOXEL_LOCATION, VoxelOctoTree *> voxel_map;
  
  string root_dir;
  string lid_topic, imu_topic, seq_name, img_topic;
  V3D extT;
  M3D extR;

  int feats_down_size = 0, max_iterations = 0;

  double res_mean_last = 0.05;
  double gyr_cov = 0, acc_cov = 0, inv_expo_cov = 0;
  double blind_rgb_points = 0.0;
  double last_timestamp_lidar = -1.0, last_timestamp_imu = -1.0, last_timestamp_img = -1.0;
  double filter_size_surf_min = 0;
  double filter_size_pcd = 0;
  double _first_lidar_time = 0.0;
  double match_time = 0, solve_time = 0, solve_const_H_time = 0;

  bool lidar_map_inited = false, pcd_save_en = false, pub_effect_point_en = false, pose_output_en = false, ros_driver_fix_en = false, hilti_en = false;
  int pcd_save_interval = -1, pcd_index = 0;
  int pub_scan_num = 1;

  StatesGroup imu_propagate, latest_ekf_state;

  bool new_imu = false, state_update_flg = false, imu_prop_enable = true, ekf_finish_once = false;
  deque<fast_livo::ImuSample> prop_imu_buffer;
  fast_livo::ImuSample newest_imu;
  double latest_ekf_time;
  fast_livo::PoseSample imu_prop_odom;
  std::string pubImuPropOdom = "/LIVO2/imu_propagate";
  double imu_time_offset = 0.0;
  double lidar_time_offset = 0.0;

  bool gravity_align_en = false, gravity_align_finished = false;

  bool sync_jump_flag = false;

  bool lidar_pushed = false, imu_en, gravity_est_en, flg_reset = false, ba_bg_est_en = true;
  bool dense_map_en = false;
  int img_en = 1, imu_int_frame = 3;
  bool normal_en = true;
  bool exposure_estimate_en = false;
  double exposure_time_init = 0.0;
  bool inverse_composition_en = false;
  bool raycast_en = false;
  int lidar_en = 1;
  bool is_first_frame = false;
  int grid_size, patch_size, grid_n_width, grid_n_height, patch_pyrimid_level;
  double outlier_threshold;
  double plot_time;
  int frame_cnt;
  double img_time_offset = 0.0;
  deque<PointCloudXYZI::Ptr> lid_raw_data_buffer;
  deque<double> lid_header_time_buffer;
  deque<fast_livo::ImuSample::ConstPtr> imu_buffer;
  deque<cv::Mat> img_buffer;
  deque<double> img_time_buffer;
  vector<pointWithVar> _pv_list;
  vector<double> extrinT;
  vector<double> extrinR;
  vector<double> cameraextrinT;
  vector<double> cameraextrinR;
  double IMG_POINT_COV;

  PointCloudXYZI::Ptr visual_sub_map;
  PointCloudXYZI::Ptr feats_undistort;
  PointCloudXYZI::Ptr feats_down_body;
  PointCloudXYZI::Ptr feats_down_world;
  PointCloudXYZI::Ptr pcl_w_wait_pub;
  PointCloudXYZI::Ptr pcl_wait_pub;
  PointCloudXYZRGB::Ptr pcl_wait_save;
  PointCloudXYZI::Ptr pcl_wait_save_intensity;

  ofstream fout_pre, fout_out, fout_pcd_pos, fout_points;

  pcl::VoxelGrid<PointType> downSizeFilterSurf;

  V3D euler_cur;

  LidarMeasureGroup LidarMeasures;
  StatesGroup _state;
  StatesGroup  state_propagat;

  std::vector<fast_livo::PoseSample> path;
  fast_livo::PoseSample odomAftMapped;
  fast_livo::Quaternion geoQuat;
  fast_livo::PoseSample msg_body_pose;

  PreprocessPtr p_pre;
  ImuProcessPtr p_imu;
  VoxelMapManagerPtr voxelmap_manager;
  VIOManagerPtr vio_manager;
  std::ofstream vio_iteration_stream_;
  GeographicLib::LocalCartesian gps_trans_;

  std::string plane_pub = "/planner_normal";
  std::string voxel_pub = "/voxels";




  std::string pubLaserCloudFullRes = "/cloud_registered";
  std::string pubNormal = "visualization_marker";
  std::string pubSubVisualMap = "/cloud_visual_sub_map_before";
  std::string pubLaserCloudEffect = "/cloud_effected";
  std::string pubLaserCloudMap = "/Laser_map";
  std::string pubOdomAftMapped = "/aft_mapped_to_init";
  std::string pubPath = "/path";
  std::string pubLaserCloudDyn = "/dyn_obj";
  std::string pubLaserCloudDynRmed = "/dyn_obj_removed";
  std::string pubLaserCloudDynDbg = "/dyn_obj_dbg_hist";
  std::string pubImage = "/rgb_img";
  std::string mavros_pose_publisher = "/mavros/vision_pose/pose";


  int frame_num = 0;
  double aver_time_consu = 0;
  double aver_time_icp = 0;
  double aver_time_map_inre = 0;
  bool colmap_output_en = false;

  bool rtk_en = false;
  bool rtk_ini = false;
  bool rtk_good = false;
  std::deque<RTK> rtk_buffer;
  std::deque<RTK> rtk_proc_buffer;
  std::deque<std::vector<double>> livo_state_buffer;
  Sophus::SE3 T_W_to_G;
  Sophus::SE3 T_G_to_W;
  vector<double> T_I_R;

  int pcd_file_index = 0;
  bool debug_mode = false;
  std::string save_directory;
  std::string pcd_save_file;
  std::string rtk_save_file;
  std::string imu_save_file;
  std::string odom_save_file; 
  std::string cov_save_file; 
  
  double keyframe_time = 0.0;

  std::string pub_odom = "/odometry/fast_livo2";
  std::string pub_lidarRGB = "/synced_cloud";
  std::string pub_rtk = "/gps/odometry";
};
#endif