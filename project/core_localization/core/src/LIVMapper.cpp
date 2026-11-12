/* 
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.

Developer: Chunran Zheng <zhengcr@connect.hku.hk>

For commercial use, please contact me at <zhengcr@connect.hku.hk> or
Prof. Fu Zhang at <fuzhang@hku.hk>.

This file is subject to the terms and conditions outlined in the 'LICENSE' file,
which is included as part of this source code package.
*/

#include "LIVMapper.h"
#include "camera_factory.h"
#include "fast_livo/rotation.h"
#include <filesystem>
#include <pcl/conversions.h>
#include <pcl/io/pcd_io.h>



LIVMapper::LIVMapper(const fast_livo::Config &config, fast_livo::Outputs outputs)
    : extT(0, 0, 0),
      extR(M3D::Identity()), config_(config), outputs_(std::move(outputs))
{
  extrinT.assign(3, 0.0);
  extrinR.assign(9, 0.0);
  cameraextrinT.assign(3, 0.0);
  cameraextrinR.assign(9, 0.0);

  p_pre.reset(new Preprocess());
  p_imu.reset(new ImuProcess());

  readParameters(config);
  VoxelMapConfig voxel_config;
  loadVoxelConfig(config, voxel_config);

  visual_sub_map.reset(new PointCloudXYZI());
  feats_undistort.reset(new PointCloudXYZI());
  feats_down_body.reset(new PointCloudXYZI());
  feats_down_world.reset(new PointCloudXYZI());
  pcl_w_wait_pub.reset(new PointCloudXYZI());
  pcl_wait_pub.reset(new PointCloudXYZI());
  pcl_wait_save.reset(new PointCloudXYZRGB());
  pcl_wait_save_intensity.reset(new PointCloudXYZI());
  voxelmap_manager.reset(new VoxelMapManager(voxel_config, voxel_map));
  voxelmap_manager->voxel_map_pub_ = outputs_.planes;
  if(outputs_.trace)
  {
    voxelmap_manager->trace_system = [this](const std::string &name,const Eigen::MatrixXd &matrix) {
      fast_livo::Outputs::Trace trace;
      trace.stage="lio-kernel/"+name;
      trace.time=keyframe_time;
      trace.state={static_cast<double>(matrix.rows()),static_cast<double>(matrix.cols())};
      for(Eigen::Index row=0;row<matrix.rows();++row)
        for(Eigen::Index column=0;column<matrix.cols();++column) trace.state.push_back(matrix(row,column));
      outputs_.trace(trace);
    };
  }
  vio_manager.reset(new VIOManager());
  root_dir = config.laserMapping_outputfilepath + "/";
  std::filesystem::create_directories(root_dir + "Log/PCD");
  std::filesystem::create_directories(root_dir + "Log/result");
  std::filesystem::create_directories(root_dir + "Log/Colmap/images");
  std::filesystem::create_directories(root_dir + "Log/Colmap/sparse/0");
  std::filesystem::create_directories(root_dir + "debug/pcd");
  initializeFiles();
  initializeComponents();
  if (!config.debug_vio_trace_file.empty())
  {
    vio_iteration_stream_.open(config.debug_vio_trace_file);
    if (!vio_iteration_stream_) throw std::runtime_error("无法创建 VIO 迭代追踪文件");
    vio_manager->iteration_trace = [this](int level, int iteration, int measurements, float error, float last) {
      vio_iteration_stream_ << std::setprecision(17) << keyframe_time << " " << level << " " << iteration
                            << " " << measurements << " " << std::setprecision(9) << error << " " << last;
      for (std::size_t i = 0; i < vio_manager->visual_submap->voxel_points.size(); ++i)
        if (vio_manager->visual_submap->voxel_points[i])
          vio_iteration_stream_ << " " << vio_manager->visual_submap->errors[i];
      vio_iteration_stream_ << "\n";
      if (!vio_iteration_stream_) throw std::runtime_error("VIO 迭代追踪写入失败");
    };
  }

}

LIVMapper::~LIVMapper()
{
  auto *camera = vio_manager ? vio_manager->cam : nullptr;
  vio_manager.reset();
  delete camera;
}

void LIVMapper::readParameters(const fast_livo::Config &config)
{
  lid_topic = config.common_lid_topic;
  imu_topic = config.common_imu_topic;
  ros_driver_fix_en = config.common_ros_driver_bug_fix;
  img_en = config.common_img_en;
  lidar_en = config.common_lidar_en;
  img_topic = config.common_img_topic;

  normal_en = config.vio_normal_en;
  inverse_composition_en = config.vio_inverse_composition_en;
  max_iterations = config.vio_max_iterations;
  IMG_POINT_COV = config.vio_img_point_cov;
  raycast_en = config.vio_raycast_en;
  exposure_estimate_en = config.vio_exposure_estimate_en;
  inv_expo_cov = config.vio_inv_expo_cov;
  grid_size = config.vio_grid_size;
  grid_n_height = config.vio_grid_n_height;
  patch_pyrimid_level = config.vio_patch_pyrimid_level;
  patch_size = config.vio_patch_size;
  outlier_threshold = config.vio_outlier_threshold;

  exposure_time_init = config.time_offset_exposure_time_init;
  img_time_offset = config.time_offset_img_time_offset;
  imu_time_offset = config.time_offset_imu_time_offset;
  lidar_time_offset = config.time_offset_lidar_time_offset;
  imu_prop_enable = config.uav_imu_rate_odom;
  gravity_align_en = config.uav_gravity_align_en;

  seq_name = config.evo_seq_name;
  pose_output_en = config.evo_pose_output_en;
  gyr_cov = config.imu_gyr_cov;
  acc_cov = config.imu_acc_cov;
  imu_int_frame = config.imu_imu_int_frame;
  imu_en = config.imu_imu_en;
  gravity_est_en = config.imu_gravity_est_en;
  ba_bg_est_en = config.imu_ba_bg_est_en;

  p_pre->blind = config.preprocess_blind;
  filter_size_surf_min = config.preprocess_filter_size_surf;
  hilti_en = config.preprocess_hilti_en;
  p_pre->lidar_type = config.preprocess_lidar_type;
  p_pre->N_SCANS = config.preprocess_scan_line;
  p_pre->point_filter_num = config.preprocess_point_filter_num;
  p_pre->feature_enabled = config.preprocess_feature_extract_enabled;

  pcd_save_interval = config.pcd_save_interval;
  pcd_save_en = config.pcd_save_pcd_save_en;
  colmap_output_en = config.pcd_save_colmap_output_en;
  filter_size_pcd = config.pcd_save_filter_size_pcd;
  extrinT = config.extrin_calib_extrinsic_T;
  extrinR = config.extrin_calib_extrinsic_R;
  cameraextrinT = config.extrin_calib_Pcl;
  cameraextrinR = config.extrin_calib_Rcl;
  plot_time = config.debug_plot_time;
  frame_cnt = config.debug_frame_cnt;

  blind_rgb_points = config.publish_blind_rgb_points;
  pub_scan_num = config.publish_pub_scan_num;
  pub_effect_point_en = config.publish_pub_effect_point_en;
  dense_map_en = config.publish_dense_map_en;

  debug_mode = config.gps_debug_mode;
  p_pre->blind_sqr = p_pre->blind * p_pre->blind;

  //rtk_en = config.gps_gps_en;
  T_I_R = config.gps_extrinsic_T;
  save_directory = config.laserMapping_outputfilepath;
  pcd_save_file = save_directory + "/debug/pcd/";
  rtk_save_file = save_directory + "/debug/rtk.txt";
  imu_save_file = save_directory + "/debug/imu.txt";
  odom_save_file = save_directory + "/debug/odom.txt"; 
  cov_save_file  = save_directory + "/debug/cov.txt"; 
}

void LIVMapper::initializeComponents() 
{
  downSizeFilterSurf.setLeafSize(filter_size_surf_min, filter_size_surf_min, filter_size_surf_min);
  extT << VEC_FROM_ARRAY(extrinT);
  extR << MAT_FROM_ARRAY(extrinR);

  voxelmap_manager->extT_ << VEC_FROM_ARRAY(extrinT);
  voxelmap_manager->extR_ << MAT_FROM_ARRAY(extrinR);

  vio_manager->cam = create_camera(config_);

  vio_manager->grid_size = grid_size;
  vio_manager->patch_size = patch_size;
  vio_manager->outlier_threshold = outlier_threshold;
  vio_manager->setImuToLidarExtrinsic(extT, extR);
  vio_manager->setLidarToCameraExtrinsic(cameraextrinR, cameraextrinT);
  vio_manager->state = &_state;
  vio_manager->state_propagat = &state_propagat;
  vio_manager->max_iterations = max_iterations;
  vio_manager->img_point_cov = IMG_POINT_COV;
  vio_manager->normal_en = normal_en;
  vio_manager->inverse_composition_en = inverse_composition_en;
  vio_manager->raycast_en = raycast_en;
  vio_manager->grid_n_width = grid_n_width;
  vio_manager->grid_n_height = grid_n_height;
  vio_manager->patch_pyrimid_level = patch_pyrimid_level;
  vio_manager->exposure_estimate_en = exposure_estimate_en;
  vio_manager->colmap_output_en = colmap_output_en;
  vio_manager->root_dir = root_dir;
  vio_manager->initializeVIO();

  p_imu->set_extrinsic(extT, extR);
  p_imu->set_gyr_cov_scale(V3D(gyr_cov, gyr_cov, gyr_cov));
  p_imu->set_acc_cov_scale(V3D(acc_cov, acc_cov, acc_cov));
  p_imu->set_inv_expo_cov(inv_expo_cov);
  p_imu->set_gyr_bias_cov(V3D(0.0001, 0.0001, 0.0001));
  p_imu->set_acc_bias_cov(V3D(0.0001, 0.0001, 0.0001));
  p_imu->set_imu_init_frame_num(imu_int_frame);

  if (!imu_en) p_imu->disable_imu();
  if (!gravity_est_en) p_imu->disable_gravity_est();
  if (!ba_bg_est_en) p_imu->disable_bias_est();
  if (!exposure_estimate_en) p_imu->disable_exposure_est();

  slam_mode_ = (img_en && lidar_en) ? LIVO : imu_en ? ONLY_LIO : ONLY_LO;
}

void LIVMapper::initializeFiles() 
{
  if (pcd_save_en && colmap_output_en)
  {
    for (const auto &directory : {root_dir + "Log/Colmap/images", root_dir + "Log/Colmap/sparse/0"})
    {
      std::filesystem::remove_all(directory);
      std::filesystem::create_directories(directory);
    }
  }
  if(colmap_output_en) fout_points.open(root_dir + "Log/Colmap/sparse/0/points3D.txt", std::ios::out);
  if(pcd_save_interval > 0) fout_pcd_pos.open(root_dir + "Log/PCD/scans_pos.json", std::ios::out);
  fout_pre.open(DEBUG_FILE_DIR("mat_pre.txt"), std::ios::out);
  fout_out.open(DEBUG_FILE_DIR("mat_out.txt"), std::ios::out);
}



void LIVMapper::handleFirstFrame() 
{
  if (!is_first_frame)
  {
    _first_lidar_time = LidarMeasures.last_lio_update_time;
    p_imu->first_lidar_time = _first_lidar_time; // Only for IMU data log
    is_first_frame = true;
    cout << "FIRST LIDAR FRAME!" << endl;
  }
}

void LIVMapper::gravityAlignment() 
{
  if (!p_imu->imu_need_init && !gravity_align_finished) 
  {
    std::cout << "Gravity Alignment Starts" << std::endl;
    V3D ez(0, 0, -1), gz(_state.gravity);
    Quaterniond G_q_I0 = Quaterniond::FromTwoVectors(gz, ez);
    M3D G_R_I0 = G_q_I0.toRotationMatrix();

    _state.pos_end = G_R_I0 * _state.pos_end;
    _state.rot_end = G_R_I0 * _state.rot_end;
    _state.vel_end = G_R_I0 * _state.vel_end;
    _state.gravity = G_R_I0 * _state.gravity;
    gravity_align_finished = true;
    std::cout << "Gravity Alignment Finished" << std::endl;
  }
}

void LIVMapper::processImu() 
{
  // double t0 = omp_get_wtime();

  const bool imu_initializing = p_imu->imu_need_init;
  p_imu->Process2(LidarMeasures, _state, feats_undistort, T_G_to_W);
  if (imu_initializing && !p_imu->imu_need_init)
  {
    p_imu->fout_imu.open(DEBUG_FILE_DIR("imu.txt"), std::ios::out);
    if (!p_imu->fout_imu) throw std::runtime_error("无法创建 IMU 调试输出文件");
  }

  if (gravity_align_en) gravityAlignment();

  state_propagat = _state;
  voxelmap_manager->state_ = _state;
  voxelmap_manager->feats_undistort_ = feats_undistort;

  // double t_prop = omp_get_wtime();

  // std::cout << "[ Mapping ] feats_undistort: " << feats_undistort->size() << std::endl;
  // std::cout << "[ Mapping ] predict cov: " << _state.cov.diagonal().transpose() << std::endl;
  // std::cout << "[ Mapping ] predict sta: " << state_propagat.pos_end.transpose() << state_propagat.vel_end.transpose() << std::endl;
}

void LIVMapper::stateEstimationAndMapping() 
{
  switch (LidarMeasures.lio_vio_flg) 
  {
    case VIO:
      handleVIO();
      break;
    case LIO:
    case LO:
      handleLIO();
      fast_livo::log_info("[HandleLIO] Position after: [%f, %f, %f]", _state.pos_end(0), _state.pos_end(1), _state.pos_end(2));
      fast_livo::log_info("[HandleLIO] p_cov after: %.12f, %.12f, %.12f", _state.cov(3,3), _state.cov(4,4), _state.cov(5,5));

      // if(rtk_en && rtk_ini)
      // {
      //   handleRTK();
      // }
      break;
  }
}

void LIVMapper::handleRTK()
{
  fast_livo::log_info("%s[ RTK Update ]%s", BLUE, RESET);
  if (LidarMeasures.measures.empty()) 
  {
      return; 
  }

  auto rtk_data = LidarMeasures.measures.back().rtk; 
  if(rtk_data.timestamp < 0.00001)
  {
    return;
  }

  std::deque temp_measures = LidarMeasures.measures;

  //fast_livo::log_info("[HandleRTK] rtk_data.p: [%.6f, %.6f, %.6f]", rtk_data.p[0], rtk_data.p[1], rtk_data.p[2]);
  Eigen::Vector3d z_k = rtk_data.p; 
  z_k[2] = _state.pos_end(2);
  Eigen::Vector3d T_I_to_R;
  T_I_to_R[0] = T_I_R[0];
  T_I_to_R[1] = T_I_R[1];
  T_I_to_R[2] = T_I_R[2];
  fast_livo::log_info("[HandleRTK] RTK_W: [%f, %f, %f]", z_k(0), z_k(1), z_k(2));
  fast_livo::log_info("[HandleRTK] Position before: [%f, %f, %f]", _state.pos_end(0), _state.pos_end(1), _state.pos_end(2));
  fast_livo::log_info("[HandleRTK] p_cov before: %.12f, %.12f, %.12f", _state.cov(3,3), _state.cov(4,4), _state.cov(5,5));
  fast_livo::log_info("[HandleRTK] Rotation R_before(R1): [%.6f, %.6f, %.6f]", _state.rot_end(0,0), _state.rot_end(0,1), _state.rot_end(0,2));
  fast_livo::log_info("[HandleRTK] Rotation R_before(R2): [%.6f, %.6f, %.6f]", _state.rot_end(1,0), _state.rot_end(1,1), _state.rot_end(1,2));
  fast_livo::log_info("[HandleRTK] Rotation R_before(R3): [%.6f, %.6f, %.6f]", _state.rot_end(2,0), _state.rot_end(2,1), _state.rot_end(2,2));

  Eigen::Matrix3d R_cov = Eigen::Matrix3d::Identity() * 1e-4 * 5;

  
  Eigen::Matrix<double, 3, DIM_STATE> H;
  H.setZero();
  H.block<3, 3>(0, 3) = Eigen::Matrix3d::Identity();

  Eigen::Matrix<double, DIM_STATE, DIM_STATE> P = _state.cov;
  Eigen::Matrix<double, 3, 3> S = H * P * H.transpose() + R_cov;
  Eigen::Matrix<double, DIM_STATE, 3> K = P * H.transpose() * S.inverse();
  Eigen::Vector3d y = z_k - (_state.pos_end + _state.rot_end * T_I_to_R);
  fast_livo::log_info("[HandleRTK] y: [%f, %f, %f]", y(0), y(1), y(2));
  Eigen::Matrix<double, DIM_STATE, 1> dx = K * y;
  fast_livo::log_info("[HandleRTK] dx: [%f, %f, %f, %f, %f, %f]", dx(0), dx(1), dx(2), dx(3), dx(4), dx(5));
  _state += dx;
  
  if (_state.gravity.norm() > 0.1) { 
      _state.gravity += dx.segment<3>(16);
  }

  Eigen::Matrix<double, DIM_STATE, DIM_STATE> I = Eigen::Matrix<double, DIM_STATE, DIM_STATE>::Identity();
  _state.cov = (I - K * H) * P;
  fast_livo::log_info("[HandleRTK] Position after: [%f, %f, %f]", _state.pos_end(0), _state.pos_end(1), _state.pos_end(2));
  fast_livo::log_info("[HandleRTK] p_cov after: %.12f, %.12f, %.12f", _state.cov(3,3), _state.cov(4,4), _state.cov(5,5));
  fast_livo::log_info("[HandleRTK] Rotation R_after(R1): [%.6f, %.6f, %.6f]", _state.rot_end(0,0), _state.rot_end(0,1), _state.rot_end(0,2));
  fast_livo::log_info("[HandleRTK] Rotation R_after(R2): [%.6f, %.6f, %.6f]", _state.rot_end(1,0), _state.rot_end(1,1), _state.rot_end(1,2));
  fast_livo::log_info("[HandleRTK] Rotation R_after(R3): [%.6f, %.6f, %.6f]", _state.rot_end(2,0), _state.rot_end(2,1), _state.rot_end(2,2));
  // std::cout << "[RTK Update] Pos Correction: " << dx.segment<3>(3).transpose() << std::endl;
}

void LIVMapper::handleVIO() 
{
  euler_cur = RotMtoEuler(_state.rot_end);
  fout_pre << std::setw(20) << LidarMeasures.last_lio_update_time - _first_lidar_time << " " << euler_cur.transpose() * 57.3 << " "
            << _state.pos_end.transpose() << " " << _state.vel_end.transpose() << " " << _state.bias_g.transpose() << " "
            << _state.bias_a.transpose() << " " << V3D(_state.inv_expo_time, 0, 0).transpose() << std::endl;
    
  if (pcl_w_wait_pub->empty() || (pcl_w_wait_pub == nullptr)) 
  {
    std::cout << "[ VIO ] No point!!!" << std::endl;
    return;
  }
    
  //std::cout << "[ VIO ] Raw feature num: " << pcl_w_wait_pub->points.size() << std::endl;

  if (fabs((LidarMeasures.last_lio_update_time - _first_lidar_time) - plot_time) < (frame_cnt / 2 * 0.1)) 
  {
    vio_manager->plot_flag = true;
  } 
  else 
  {
    vio_manager->plot_flag = false;
  }

  vio_manager->processFrame(LidarMeasures.measures.back().img, _pv_list, voxelmap_manager->voxel_map_, LidarMeasures.last_lio_update_time - _first_lidar_time);

  if (imu_prop_enable) 
  {
    ekf_finish_once = true;
    latest_ekf_state = _state;
    latest_ekf_time = LidarMeasures.last_lio_update_time;
    state_update_flg = true;
  }

  // int size_sub_map = vio_manager->visual_sub_map_cur.size();
  // visual_sub_map->reserve(size_sub_map);
  // for (int i = 0; i < size_sub_map; i++) 
  // {
  //   PointType temp_map;
  //   temp_map.x = vio_manager->visual_sub_map_cur[i]->pos_[0];
  //   temp_map.y = vio_manager->visual_sub_map_cur[i]->pos_[1];
  //   temp_map.z = vio_manager->visual_sub_map_cur[i]->pos_[2];
  //   temp_map.intensity = 0.;
  //   visual_sub_map->push_back(temp_map);
  // }

  publish_frame_world(pubLaserCloudFullRes, vio_manager);
  publish_img_rgb(pubImage, vio_manager);

  euler_cur = RotMtoEuler(_state.rot_end);
  fout_out << std::setw(20) << LidarMeasures.last_lio_update_time - _first_lidar_time << " " << euler_cur.transpose() * 57.3 << " "
            << _state.pos_end.transpose() << " " << _state.vel_end.transpose() << " " << _state.bias_g.transpose() << " "
            << _state.bias_a.transpose() << " " << V3D(_state.inv_expo_time, 0, 0).transpose() << " " << feats_undistort->points.size() << std::endl;
}

void LIVMapper::handleLIO() 
{    
  fast_livo::log_info("%s[ LIO Update ]%s", BLUE, RESET);
  fast_livo::log_info("[HandleLIO] Position before: [%f, %f, %f]", _state.pos_end(0), _state.pos_end(1), _state.pos_end(2));
  fast_livo::log_info("[HandleLIO] P_cov before: %.12f, %.12f, %.12f", _state.cov(3,3), _state.cov(4,4), _state.cov(5,5));
  euler_cur = RotMtoEuler(_state.rot_end);
  fout_pre << setw(20) << LidarMeasures.last_lio_update_time - _first_lidar_time << " " << euler_cur.transpose() * 57.3 << " "
           << _state.pos_end.transpose() << " " << _state.vel_end.transpose() << " " << _state.bias_g.transpose() << " "
           << _state.bias_a.transpose() << " " << V3D(_state.inv_expo_time, 0, 0).transpose() << endl;
           
  if (feats_undistort->empty() || (feats_undistort == nullptr)) 
  {
    std::cout << "[ LIO ]: No point!!!" << std::endl;
    return;
  }

  double t0 = omp_get_wtime();

  downSizeFilterSurf.setInputCloud(feats_undistort);
  downSizeFilterSurf.filter(*feats_down_body);
  
  double t_down = omp_get_wtime();

  feats_down_size = feats_down_body->points.size();
  voxelmap_manager->feats_down_body_ = feats_down_body;
  transformLidar(_state.rot_end, _state.pos_end, feats_down_body, feats_down_world);
  voxelmap_manager->feats_down_world_ = feats_down_world;
  voxelmap_manager->feats_down_size_ = feats_down_size;
  
  if (!lidar_map_inited) 
  {
    lidar_map_inited = true;
    voxelmap_manager->BuildVoxelMap();
  }

  double t1 = omp_get_wtime();

  V3D rtk_data = LidarMeasures.measures.back().rtk.p; 
  if(rtk_ini && LidarMeasures.measures.back().rtk.timestamp > 0.00001)
  {
    rtk_good = true;
  }
  else
  {
    rtk_good = false;
  }
  
  fast_livo::log_info("[HandleLIO] RTW_W: [%f, %f, %f]", rtk_data(0), rtk_data(1), rtk_data(2));
  voxelmap_manager->StateEstimation(state_propagat, rtk_good, rtk_data);
  _state = voxelmap_manager->state_;
  _pv_list = voxelmap_manager->pv_list_;

  double t2 = omp_get_wtime();

  if (imu_prop_enable) 
  {
    ekf_finish_once = true;
    latest_ekf_state = _state;
    latest_ekf_time = LidarMeasures.last_lio_update_time;
    state_update_flg = true;
  }

  if (pose_output_en) 
  {
    std::ofstream outFile, evoFile;
    if (!pos_opend) 
    {
      evoFile.open(root_dir + "Log/result/" + seq_name + ".txt", std::ios::out);
      pos_opend = true;
      if (!evoFile.is_open()) fast_livo::log_info("open fail\n");
    } 
    else 
    {
      evoFile.open(root_dir + "Log/result/" + seq_name + ".txt", std::ios::app);
      if (!evoFile.is_open()) fast_livo::log_info("open fail\n");
    }
    Eigen::Matrix4d outT;
    Eigen::Quaterniond q(_state.rot_end);
    evoFile << std::fixed;
    evoFile << LidarMeasures.last_lio_update_time << " " << _state.pos_end[0] << " " << _state.pos_end[1] << " " << _state.pos_end[2] << " "
            << q.x() << " " << q.y() << " " << q.z() << " " << q.w() << std::endl;
  }
  
  euler_cur = RotMtoEuler(_state.rot_end);
  geoQuat = fast_livo::quaternion_from_rpy(euler_cur(0), euler_cur(1), euler_cur(2));
  publish_odometry(pubOdomAftMapped);

  double t3 = omp_get_wtime();

  PointCloudXYZI::Ptr world_lidar(new PointCloudXYZI());
  transformLidar(_state.rot_end, _state.pos_end, feats_down_body, world_lidar);
  for (size_t i = 0; i < world_lidar->points.size(); i++) 
  {
    voxelmap_manager->pv_list_[i].point_w << world_lidar->points[i].x, world_lidar->points[i].y, world_lidar->points[i].z;
    M3D point_crossmat = voxelmap_manager->cross_mat_list_[i];
    M3D var = voxelmap_manager->body_cov_list_[i];
    var = (_state.rot_end * extR) * var * (_state.rot_end * extR).transpose() +
          (-point_crossmat) * _state.cov.block<3, 3>(0, 0) * (-point_crossmat).transpose() + _state.cov.block<3, 3>(3, 3);
    voxelmap_manager->pv_list_[i].var = var;
  }
  voxelmap_manager->UpdateVoxelMap(voxelmap_manager->pv_list_);
  //std::cout << "[ LIO ] Update Voxel Map" << std::endl;
  _pv_list = voxelmap_manager->pv_list_;
  
  double t4 = omp_get_wtime();

  if(voxelmap_manager->config_setting_.map_sliding_en)
  {
    voxelmap_manager->mapSliding();
  }
  
  PointCloudXYZI::Ptr laserCloudFullRes(dense_map_en ? feats_undistort : feats_down_body);
  int size = laserCloudFullRes->points.size();
  PointCloudXYZI::Ptr laserCloudWorld(new PointCloudXYZI(size, 1));

  for (int i = 0; i < size; i++) 
  {
    RGBpointBodyToWorld(&laserCloudFullRes->points[i], &laserCloudWorld->points[i]);
  }
  *pcl_w_wait_pub = *laserCloudWorld;

  if (!img_en) publish_frame_world(pubLaserCloudFullRes, vio_manager);
  if (pub_effect_point_en) publish_effect_world(pubLaserCloudEffect, voxelmap_manager->ptpl_list_);
  if (voxelmap_manager->config_setting_.is_pub_plane_map_) voxelmap_manager->pubVoxelMap();
  publish_path(pubPath);
  publish_mavros(mavros_pose_publisher);

  frame_num++;
  aver_time_consu = aver_time_consu * (frame_num - 1) / frame_num + (t4 - t0) / frame_num;

  // aver_time_icp = aver_time_icp * (frame_num - 1) / frame_num + (t2 - t1) / frame_num;
  // aver_time_map_inre = aver_time_map_inre * (frame_num - 1) / frame_num + (t4 - t3) / frame_num;
  // aver_time_solve = aver_time_solve * (frame_num - 1) / frame_num + (solve_time) / frame_num;
  // aver_time_const_H_time = aver_time_const_H_time * (frame_num - 1) / frame_num + solve_const_H_time / frame_num;
  // printf("[ mapping time ]: per scan: propagation %0.6f downsample: %0.6f match: %0.6f solve: %0.6f  ICP: %0.6f  map incre: %0.6f total: %0.6f \n"
  //         "[ mapping time ]: average: icp: %0.6f construct H: %0.6f, total: %0.6f \n",
  //         t_prop - t0, t1 - t_prop, match_time, solve_time, t3 - t1, t5 - t3, t5 - t0, aver_time_icp, aver_time_const_H_time, aver_time_consu);

  // printf("\033[1;36m[ LIO mapping time ]: current scan: icp: %0.6f secs, map incre: %0.6f secs, total: %0.6f secs.\033[0m\n"
  //         "\033[1;36m[ LIO mapping time ]: average: icp: %0.6f secs, map incre: %0.6f secs, total: %0.6f secs.\033[0m\n",
  //         t2 - t1, t4 - t3, t4 - t0, aver_time_icp, aver_time_map_inre, aver_time_consu);
  // printf("\033[1;34m+-------------------------------------------------------------+\033[0m\n");
  // printf("\033[1;34m|                         LIO Mapping Time                    |\033[0m\n");
  // printf("\033[1;34m+-------------------------------------------------------------+\033[0m\n");
  // printf("\033[1;34m| %-29s | %-27s |\033[0m\n", "Algorithm Stage", "Time (secs)");
  // printf("\033[1;34m+-------------------------------------------------------------+\033[0m\n");
  // printf("\033[1;36m| %-29s | %-27f |\033[0m\n", "DownSample", t_down - t0);
  // printf("\033[1;36m| %-29s | %-27f |\033[0m\n", "ICP", t2 - t1);
  // printf("\033[1;36m| %-29s | %-27f |\033[0m\n", "updateVoxelMap", t4 - t3);
  // printf("\033[1;34m+-------------------------------------------------------------+\033[0m\n");
  // printf("\033[1;36m| %-29s | %-27f |\033[0m\n", "Current Total Time", t4 - t0);
  // printf("\033[1;36m| %-29s | %-27f |\033[0m\n", "Average Total Time", aver_time_consu);
  // printf("\033[1;34m+-------------------------------------------------------------+\033[0m\n");

  euler_cur = RotMtoEuler(_state.rot_end);
  fout_out << std::setw(20) << LidarMeasures.last_lio_update_time - _first_lidar_time << " " << euler_cur.transpose() * 57.3 << " "
            << _state.pos_end.transpose() << " " << _state.vel_end.transpose() << " " << _state.bias_g.transpose() << " "
            << _state.bias_a.transpose() << " " << V3D(_state.inv_expo_time, 0, 0).transpose() << " " << feats_undistort->points.size() << std::endl;
}

void LIVMapper::savePCD() 
{
  if (pcd_save_en && (pcl_wait_save->points.size() > 0 || pcl_wait_save_intensity->points.size() > 0) && pcd_save_interval < 0) 
  {
    std::string raw_points_dir = root_dir + "Log/PCD/all_raw_points.pcd";
    std::string downsampled_points_dir = root_dir + "Log/PCD/all_downsampled_points.pcd";
    pcl::PCDWriter pcd_writer;

    if (img_en)
    {
      pcl::PointCloud<pcl::PointXYZRGB>::Ptr downsampled_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
      pcl::VoxelGrid<pcl::PointXYZRGB> voxel_filter;
      voxel_filter.setInputCloud(pcl_wait_save);
      voxel_filter.setLeafSize(filter_size_pcd, filter_size_pcd, filter_size_pcd);
      voxel_filter.filter(*downsampled_cloud);
  
      pcd_writer.writeBinary(raw_points_dir, *pcl_wait_save); // Save the raw point cloud data
      std::cout << GREEN << "Raw point cloud data saved to: " << raw_points_dir 
                << " with point count: " << pcl_wait_save->points.size() << RESET << std::endl;
      
      pcd_writer.writeBinary(downsampled_points_dir, *downsampled_cloud); // Save the downsampled point cloud data
      std::cout << GREEN << "Downsampled point cloud data saved to: " << downsampled_points_dir 
                << " with point count after filtering: " << downsampled_cloud->points.size() << RESET << std::endl;

      if(colmap_output_en)
      {
        fout_points << "# 3D point list with one line of data per point\n";
        fout_points << "#  POINT_ID, X, Y, Z, R, G, B, ERROR\n";
        for (size_t i = 0; i < downsampled_cloud->size(); ++i) 
        {
            const auto& point = downsampled_cloud->points[i];
            fout_points << i << " "
                        << std::fixed << std::setprecision(6)
                        << point.x << " " << point.y << " " << point.z << " "
                        << static_cast<int>(point.r) << " "
                        << static_cast<int>(point.g) << " "
                        << static_cast<int>(point.b) << " "
                        << 0 << std::endl;
        }
      }
    }
    else
    {      
      pcd_writer.writeBinary(raw_points_dir, *pcl_wait_save_intensity);
      std::cout << GREEN << "Raw point cloud data saved to: " << raw_points_dir 
                << " with point count: " << pcl_wait_save_intensity->points.size() << RESET << std::endl;
    }
  }
}

bool LIVMapper::process_one()
{
  if (!sync_packages(LidarMeasures)) return false;
  trace_state("sync");
  handleFirstFrame();
  processImu();
  trace_state("imu");
  stateEstimationAndMapping();
  trace_state(LidarMeasures.lio_vio_flg == VIO ? "vio" : "lio");
  return true;
}

void LIVMapper::finish()
{
  savePCD();
  if (vio_iteration_stream_.is_open())
  {
    vio_iteration_stream_.flush();
    if (!vio_iteration_stream_) throw std::runtime_error("VIO 迭代追踪刷新失败");
  }
}

void LIVMapper::prop_imu_once(StatesGroup &imu_prop_state, const double dt, V3D acc_avr, V3D angvel_avr)
{
  double mean_acc_norm = p_imu->IMU_mean_acc_norm;
  acc_avr = acc_avr * G_m_s2 / mean_acc_norm - imu_prop_state.bias_a;
  angvel_avr -= imu_prop_state.bias_g;

  M3D Exp_f = Exp(angvel_avr, dt);
  /* propogation of IMU attitude */
  imu_prop_state.rot_end = imu_prop_state.rot_end * Exp_f;

  /* Specific acceleration (global frame) of IMU */
  V3D acc_imu = imu_prop_state.rot_end * acc_avr + V3D(imu_prop_state.gravity[0], imu_prop_state.gravity[1], imu_prop_state.gravity[2]);

  /* propogation of IMU */
  imu_prop_state.pos_end = imu_prop_state.pos_end + imu_prop_state.vel_end * dt + 0.5 * acc_imu * dt * dt;

  /* velocity of IMU */
  imu_prop_state.vel_end = imu_prop_state.vel_end + acc_imu * dt;
}

void LIVMapper::imu_prop_callback()
{
  if (p_imu->imu_need_init || !new_imu || !ekf_finish_once) { return; }
  mtx_buffer_imu_prop.lock();
  new_imu = false; // 控制propagate频率和IMU频率一致
  if (imu_prop_enable && !prop_imu_buffer.empty())
  {
    if (state_update_flg)
    {
      imu_propagate = latest_ekf_state;
      // drop all useless imu pkg
      while ((!prop_imu_buffer.empty() && prop_imu_buffer.front().timestamp.seconds() < latest_ekf_time))
      {
        prop_imu_buffer.pop_front();
      }
      last_t_from_lidar_end_time = 0;
      for (int i = 0; i < prop_imu_buffer.size(); i++)
      {
        double t_from_lidar_end_time = prop_imu_buffer[i].timestamp.seconds() - latest_ekf_time;
        double dt = t_from_lidar_end_time - last_t_from_lidar_end_time;
        // cout << "prop dt" << dt << ", " << t_from_lidar_end_time << ", " << last_t_from_lidar_end_time << endl;
        V3D acc_imu(prop_imu_buffer[i].linear_acceleration.x, prop_imu_buffer[i].linear_acceleration.y, prop_imu_buffer[i].linear_acceleration.z);
        V3D omg_imu(prop_imu_buffer[i].angular_velocity.x, prop_imu_buffer[i].angular_velocity.y, prop_imu_buffer[i].angular_velocity.z);
        prop_imu_once(imu_propagate, dt, acc_imu, omg_imu);
        last_t_from_lidar_end_time = t_from_lidar_end_time;
      }
      state_update_flg = false;
    }
    else
    {
      V3D acc_imu(newest_imu.linear_acceleration.x, newest_imu.linear_acceleration.y, newest_imu.linear_acceleration.z);
      V3D omg_imu(newest_imu.angular_velocity.x, newest_imu.angular_velocity.y, newest_imu.angular_velocity.z);
      double t_from_lidar_end_time = newest_imu.timestamp.seconds() - latest_ekf_time;
      double dt = t_from_lidar_end_time - last_t_from_lidar_end_time;
      prop_imu_once(imu_propagate, dt, acc_imu, omg_imu);
      last_t_from_lidar_end_time = t_from_lidar_end_time;
    }

    V3D posi, vel_i;
    Eigen::Quaterniond q;
    posi = imu_propagate.pos_end;
    vel_i = imu_propagate.vel_end;
    q = Eigen::Quaterniond(imu_propagate.rot_end);
    imu_prop_odom.frame_id = "world";
    imu_prop_odom.timestamp = newest_imu.timestamp;
    imu_prop_odom.position.x = posi.x();
    imu_prop_odom.position.y = posi.y();
    imu_prop_odom.position.z = posi.z();
    imu_prop_odom.orientation.w = q.w();
    imu_prop_odom.orientation.x = q.x();
    imu_prop_odom.orientation.y = q.y();
    imu_prop_odom.orientation.z = q.z();
    imu_prop_odom.velocity.x = vel_i.x();
    imu_prop_odom.velocity.y = vel_i.y();
    imu_prop_odom.velocity.z = vel_i.z();
    if(outputs_.pose) outputs_.pose(pubImuPropOdom, imu_prop_odom);
  }
  mtx_buffer_imu_prop.unlock();
}

void LIVMapper::transformLidar(const Eigen::Matrix3d rot, const Eigen::Vector3d t, const PointCloudXYZI::Ptr &input_cloud, PointCloudXYZI::Ptr &trans_cloud)
{
  PointCloudXYZI().swap(*trans_cloud);
  trans_cloud->reserve(input_cloud->size());
  for (size_t i = 0; i < input_cloud->size(); i++)
  {
    pcl::PointXYZINormal p_c = input_cloud->points[i];
    Eigen::Vector3d p(p_c.x, p_c.y, p_c.z);
    p = (rot * (extR * p + extT) + t);
    PointType pi;
    pi.x = p(0);
    pi.y = p(1);
    pi.z = p(2);
    pi.intensity = p_c.intensity;
    trans_cloud->points.push_back(pi);
  }
}

void LIVMapper::pointBodyToWorld(const PointType &pi, PointType &po)
{
  V3D p_body(pi.x, pi.y, pi.z);
  V3D p_global(_state.rot_end * (extR * p_body + extT) + _state.pos_end);
  po.x = p_global(0);
  po.y = p_global(1);
  po.z = p_global(2);
  po.intensity = pi.intensity;
}

template <typename T> void LIVMapper::pointBodyToWorld(const Matrix<T, 3, 1> &pi, Matrix<T, 3, 1> &po)
{
  V3D p_body(pi[0], pi[1], pi[2]);
  V3D p_global(_state.rot_end * (extR * p_body + extT) + _state.pos_end);
  po[0] = p_global(0);
  po[1] = p_global(1);
  po[2] = p_global(2);
}

template <typename T> Matrix<T, 3, 1> LIVMapper::pointBodyToWorld(const Matrix<T, 3, 1> &pi)
{
  V3D p(pi[0], pi[1], pi[2]);
  p = (_state.rot_end * (extR * p + extT) + _state.pos_end);
  Matrix<T, 3, 1> po(p[0], p[1], p[2]);
  return po;
}

void LIVMapper::RGBpointBodyToWorld(PointType const *const pi, PointType *const po)
{
  V3D p_body(pi->x, pi->y, pi->z);
  V3D p_global(_state.rot_end * (extR * p_body + extT) + _state.pos_end);
  po->x = p_global(0);
  po->y = p_global(1);
  po->z = p_global(2);
  po->intensity = pi->intensity;
}

void LIVMapper::standard_pcl_cbk(const fast_livo::CloudSample::ConstPtr &msg)
{
  if (!lidar_en) return;
  mtx_buffer.lock();

  double cur_head_time = msg->timestamp.seconds() + lidar_time_offset;
  // cout<<"got feature"<<endl;
  if (cur_head_time < last_timestamp_lidar)
  {
    fast_livo::log_info("lidar loop back, clear buffer");
    lid_raw_data_buffer.clear();
  }
  // fast_livo::log_info("get point cloud at time: %.6f", msg->timestamp.seconds());
  PointCloudXYZI::Ptr ptr(new PointCloudXYZI());
  p_pre->process(msg, ptr);
  lid_raw_data_buffer.push_back(ptr);
  lid_header_time_buffer.push_back(cur_head_time);
  last_timestamp_lidar = cur_head_time;

  mtx_buffer.unlock();
  sig_buffer.notify_all();
}

void LIVMapper::livox_pcl_cbk(const fast_livo::LivoxSample::ConstPtr &msg_in)
{
  if (!lidar_en) return;
  mtx_buffer.lock();
  fast_livo::LivoxSample::Ptr msg(new fast_livo::LivoxSample(*msg_in));
  // if ((abs(msg->timestamp.seconds() - last_timestamp_lidar) > 0.2 && last_timestamp_lidar > 0) || sync_jump_flag)
  // {
  //   fast_livo::log_info("lidar jumps %.3f\n", msg->timestamp.seconds() - last_timestamp_lidar);
  //   sync_jump_flag = true;
  //   msg->timestamp = fast_livo::Timestamp::from_seconds(last_timestamp_lidar + 0.1);
  // }
  if (abs(last_timestamp_imu - msg->timestamp.seconds()) > 1.0 && !imu_buffer.empty())
  {
    double timediff_imu_wrt_lidar = last_timestamp_imu - msg->timestamp.seconds();
    printf("\033[95mSelf sync IMU and LiDAR, HARD time lag is %.10lf \n\033[0m", timediff_imu_wrt_lidar - 0.100);
    // imu_time_offset = timediff_imu_wrt_lidar;
  }

  double cur_head_time = msg->timestamp.seconds();
  //fast_livo::log_info("Get LiDAR, its header time: %.6f", cur_head_time);
  if (cur_head_time < last_timestamp_lidar)
  {
    fast_livo::log_info("lidar loop back, clear buffer");
    lid_raw_data_buffer.clear();
  }
  // fast_livo::log_info("get point cloud at time: %.6f", msg->timestamp.seconds());
  PointCloudXYZI::Ptr ptr(new PointCloudXYZI());
  p_pre->process(msg, ptr);

  if (!ptr || ptr->empty()) {
    fast_livo::log_info("Received an empty point cloud");
    mtx_buffer.unlock();
    return;
  }

  lid_raw_data_buffer.push_back(ptr);
  lid_header_time_buffer.push_back(cur_head_time);
  last_timestamp_lidar = cur_head_time;

  mtx_buffer.unlock();
  sig_buffer.notify_all();
}


void LIVMapper::rtk_cbk(const fast_livo::GnssSample::ConstPtr& gpsMsg)
{
    fast_livo::log_info("RTK received");
    Eigen::Vector3d trans_local_;
    if (!first_gps) {
        first_gps = true;
        gps_trans_.Reset(gpsMsg->latitude, gpsMsg->longitude, gpsMsg->altitude);
        std::ofstream gps_file_header(rtk_save_file, std::ios::out); 
        if (gps_file_header.is_open()) {
            gps_file_header << "# timestamp x y z vx vy vz h_acc v_acc" << std::endl;
            gps_file_header.close();
        }
    }

    const double GPS_EPOCH_UNIX_TIME = 315964800.0;
    const double SECONDS_PER_WEEK = 604800.0; 
    const double LEAP_SECONDS = 18.0;
    int week = gpsMsg->time.week;
    double tow = gpsMsg->time.tow; 
    double total_gps_seconds = (double)week * SECONDS_PER_WEEK + tow;
    double timestamp_sec = total_gps_seconds + GPS_EPOCH_UNIX_TIME - LEAP_SECONDS;
    auto stamp = fast_livo::Timestamp::from_seconds(timestamp_sec);

    gps_trans_.Forward(gpsMsg->latitude, gpsMsg->longitude, gpsMsg->altitude, trans_local_[0], trans_local_[1], trans_local_[2]);

    RTK rtk_data;
    rtk_data.timestamp = stamp.seconds() - 0.001;
    rtk_data.p[0] = trans_local_[0];
    rtk_data.p[1] = trans_local_[1];
    rtk_data.p[2] = trans_local_[2];
    rtk_data.v[0] = gpsMsg->vel_e;
    rtk_data.v[1] = gpsMsg->vel_n;
    rtk_data.v[2] = -gpsMsg->vel_d;
    
    rtk_buffer.push_back(rtk_data);
    fast_livo::log_info("rtk_data_pushed: %.6f, %.3f, %.3f, %.3f", rtk_data.timestamp, rtk_data.p[0], rtk_data.p[1], rtk_data.p[2]);


    if(trans_local_.norm() > 50.0 && rtk_ini == false && rtk_en == true)
    {
      fast_livo::log_info("Start INIT RTK to LIVO TRANSFORMATION");
      InitializeRTK();
      fast_livo::log_info("RTK to LIVO TRANSFORMATION INITIALIZED");
      rtk_ini = true;
    }

    // if(rtk_ini)
    // {
    //trans_local_ = T_G_to_W * Eigen::Vector3d(trans_local_[0], trans_local_[1], trans_local_[2]);
    fast_livo::PoseSample gps_odom;
    gps_odom.timestamp = stamp; 
    gps_odom.frame_id = "camera_init";
    gps_odom.position.x = trans_local_[0];
    gps_odom.position.y = trans_local_[1];
    gps_odom.position.z = trans_local_[2];
    gps_odom.orientation = fast_livo::quaternion_from_rpy(0.0, 0.0, 0.0);
    gps_odom.velocity.x = gpsMsg->vel_e;
    gps_odom.velocity.y = gpsMsg->vel_n;
    gps_odom.velocity.z = -gpsMsg->vel_d;
    gps_odom.covariance[0] = gpsMsg->h_acc; 
    gps_odom.covariance[7] = gpsMsg->h_acc; 
    gps_odom.covariance[14] = gpsMsg->v_acc; 

    if(outputs_.pose) outputs_.pose(pub_rtk, gps_odom);
    
    if(debug_mode)
    {
      std::ofstream gps_file(rtk_save_file, std::ios::app); 
      if (gps_file.is_open()) {
          gps_file << std::fixed << std::setprecision(6) 
                  << gps_odom.timestamp.seconds() << " "  // 1. Timestamp
                  << trans_local_[0] << " "                // 2. x
                  << trans_local_[1] << " "                // 3. y
                  << trans_local_[2] << " "                // 4. z
                  << gpsMsg->vel_e << " "                  // 5. vx (East)
                  << gpsMsg->vel_n << " "                  // 6. vy (North)
                  << -gpsMsg->vel_d << " "                 // 7. vz (Up, 注意取反)
                  << gpsMsg->h_acc << " "                  // 8. h_acc
                  << gpsMsg->v_acc                         // 9. v_acc
                  << std::endl;
          gps_file.close();
      }
    // }
  }
}

Sophus::SE3 computeSVD(const std::vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d>>& target, 
    const std::vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d>>& source)
{
    if (target.empty() || target.size() != source.size()) {
        return Sophus::SE3(); 
    }

    Eigen::Vector3d target_center = Eigen::Vector3d::Zero();
    Eigen::Vector3d source_center = Eigen::Vector3d::Zero();
    for (const auto& p : target) target_center += p;
    for (const auto& p : source) source_center += p;
    target_center /= target.size();
    source_center /= source.size();

    Eigen::Matrix3d W = Eigen::Matrix3d::Zero();
    for (size_t i = 0; i < source.size(); ++i) {
        W += (target[i] - target_center) * (source[i] - source_center).transpose();
    }

    Eigen::JacobiSVD<Eigen::Matrix3d> svd(W, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::Matrix3d R = svd.matrixU() * svd.matrixV().transpose();
    if (R.determinant() < 0) { 
        R = svd.matrixU() * Eigen::DiagonalMatrix<double, 3>(1, 1, -1) * svd.matrixV().transpose();
    }

    Eigen::Vector3d t = target_center - R * source_center;
    
    return Sophus::SE3(Sophus::SO3(R), t);
}

void LIVMapper::InitializeRTK()
{
    const double time_sync_threshold = 0.05;   

    std::vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d>> pts_source_livo; // Source
    std::vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d>> pts_target_rtk;  // Target
    auto livo_it = livo_state_buffer.begin();

    for (const auto& rtk_data : rtk_buffer)
    {
        double t_rtk = rtk_data.timestamp;
        while (livo_it != livo_state_buffer.end())
        {
            double t_livo = (*livo_it)[0]; 
            if (t_livo < t_rtk - time_sync_threshold) {
                livo_it++;
            } else {
                break;
            }
        }

        if (livo_it == livo_state_buffer.end()) break;

        auto best_it = livo_it;
        double min_dt = std::abs((*best_it)[0] - t_rtk);

        auto next_it = std::next(livo_it);
        if (next_it != livo_state_buffer.end())
        {
            double dt_next = std::abs((*next_it)[0] - t_rtk);
            if (dt_next < min_dt)
            {
                min_dt = dt_next;
                best_it = next_it;
            }
        }

        if (min_dt < time_sync_threshold)
        {
          if (best_it->size() < 4) {
                fast_livo::log_info("LIVO buffer data size is too small (%lu < 4)! Check data collection.", best_it->size());
            }
            pts_target_rtk.push_back(rtk_data.p);
            Eigen::Vector3d p_livo((*best_it)[1], (*best_it)[2], (*best_it)[3]);
            pts_source_livo.push_back(p_livo);
        }
    }

    if (pts_target_rtk.size() < 3) {
        fast_livo::log_info("Not enough matched points for RTK initialization (found %lu). Waiting for more data...", pts_target_rtk.size());
    }
    fast_livo::log_info("pts_target_rtk size: %lu, pts_source_livo size: %lu", pts_target_rtk.size(), pts_source_livo.size());
    T_W_to_G = computeSVD(pts_target_rtk, pts_source_livo);
    T_G_to_W = T_W_to_G.inverse();
}

void LIVMapper::imu_cbk(const fast_livo::ImuSample::ConstPtr &msg_in)
{
  if (!imu_en) return;

  if (last_timestamp_lidar < 0.0) return;
  // fast_livo::log_info("get imu at time: %.6f", msg_in->timestamp.seconds());
  fast_livo::ImuSample::Ptr msg(new fast_livo::ImuSample(*msg_in));
  msg->timestamp = fast_livo::Timestamp::from_seconds(msg->timestamp.seconds() - imu_time_offset);
  double timestamp = msg->timestamp.seconds();

  if (fabs(last_timestamp_lidar - timestamp) > 0.5 && (!ros_driver_fix_en))
  {
    fast_livo::log_info("IMU and LiDAR not synced! delta time: %lf .\n", last_timestamp_lidar - timestamp);
  }

  if (ros_driver_fix_en) timestamp += std::round(last_timestamp_lidar - timestamp);
  msg->timestamp = fast_livo::Timestamp::from_seconds(timestamp);


  if (first_imu_save) {
      std::ofstream imu_file(imu_save_file, std::ios::out);
      if (imu_file.is_open()) {
          imu_file << "# timestamp ax ay az gx gy gz" << std::endl;
          imu_file.close();
      }
      first_imu_save = false;
  }

  if(debug_mode)
  {
    std::ofstream imu_file(imu_save_file, std::ios::app);
    if (imu_file.is_open()) {
        imu_file << std::fixed << std::setprecision(6)
                << timestamp << " "                     // 时间戳
                << msg->linear_acceleration.x << " "    // ax
                << msg->linear_acceleration.y << " "    // ay
                << msg->linear_acceleration.z << " "    // az
                << msg->angular_velocity.x << " "       // gx
                << msg->angular_velocity.y << " "       // gy
                << msg->angular_velocity.z << std::endl;// gz
        imu_file.close();
    }
  }

  mtx_buffer.lock();

  if (last_timestamp_imu > 0.0 && timestamp < last_timestamp_imu)
  {
    mtx_buffer.unlock();
    sig_buffer.notify_all();
    fast_livo::log_info("imu loop back, offset: %lf \n", last_timestamp_imu - timestamp);
    return;
  }

  // if (last_timestamp_imu > 0.0 && timestamp > last_timestamp_imu + 0.2)
  // {

  //   fast_livo::log_info("imu time stamp Jumps %0.4lf seconds \n", timestamp - last_timestamp_imu);
  //   mtx_buffer.unlock();
  //   sig_buffer.notify_all();
  //   return;
  // }

  last_timestamp_imu = timestamp;

  imu_buffer.push_back(msg);
  // cout<<"got imu: "<<timestamp<<" imu size "<<imu_buffer.size()<<endl;
  mtx_buffer.unlock();
  if (imu_prop_enable)
  {
    mtx_buffer_imu_prop.lock();
    if (imu_prop_enable && !p_imu->imu_need_init) { prop_imu_buffer.push_back(*msg); }
    newest_imu = *msg;
    new_imu = true;
    mtx_buffer_imu_prop.unlock();
  }
  sig_buffer.notify_all();
}

cv::Mat LIVMapper::getImageFromMsg(const fast_livo::ImageSample::ConstPtr &message)
{ return message->image.clone(); }

void LIVMapper::img_cbk(const fast_livo::ImageSample::ConstPtr &msg_in)
{
  if (!img_en) return;
  fast_livo::ImageSample::Ptr msg(new fast_livo::ImageSample(*msg_in));
  // if ((abs(msg->timestamp.seconds() - last_timestamp_img) > 0.2 && last_timestamp_img > 0) || sync_jump_flag)
  // {
  //   fast_livo::log_info("img jumps %.3f\n", msg->timestamp.seconds() - last_timestamp_img);
  //   sync_jump_flag = true;
  //   msg->timestamp = fast_livo::Timestamp::from_seconds(last_timestamp_img + 0.1);
  // }

  // Hiliti2022 40Hz
  if (hilti_en)
  {
    if (++frame_counter % 4 != 0) return;
  }
  // double msg_header_time =  msg->timestamp.seconds();
  double msg_header_time = msg->timestamp.seconds() + img_time_offset;
  if (abs(msg_header_time - last_timestamp_img) < 0.001) return;
  //fast_livo::log_info("Get image, its header time: %.6f", msg_header_time);
  if (last_timestamp_lidar < 0) return;

  if (msg_header_time < last_timestamp_img)
  {
    fast_livo::log_info("image loop back. \n");
    return;
  }

  mtx_buffer.lock();

  double img_time_correct = msg_header_time; // last_timestamp_lidar + 0.105;

  if (img_time_correct - last_timestamp_img < 0.02)
  {
    fast_livo::log_info("Image need Jumps: %.6f", img_time_correct);
    mtx_buffer.unlock();
    sig_buffer.notify_all();
    return;
  }

  cv::Mat img_cur = getImageFromMsg(msg);
  img_buffer.push_back(img_cur);
  img_time_buffer.push_back(img_time_correct);

  // fast_livo::log_info("Correct Image time: %.6f", img_time_correct);

  last_timestamp_img = img_time_correct;
  // cv::imshow("img", img);
  // cv::waitKey(1);
  // cout<<"last_timestamp_img:::"<<last_timestamp_img<<endl;
  mtx_buffer.unlock();
  sig_buffer.notify_all();
}

bool LIVMapper::sync_packages(LidarMeasureGroup &meas)
{
  if (lid_raw_data_buffer.empty() && lidar_en) return false;
  if (img_buffer.empty() && img_en) return false;
  if (imu_buffer.empty() && imu_en) return false;

  switch (slam_mode_)
  {
  case ONLY_LIO:
  {
    if (meas.last_lio_update_time < 0.0) meas.last_lio_update_time = lid_header_time_buffer.front();
    if (!lidar_pushed)
    {
      // If not push the lidar into measurement data buffer
      meas.lidar = lid_raw_data_buffer.front(); // push the first lidar topic
      if (meas.lidar->points.size() <= 1) return false;

      meas.lidar_frame_beg_time = lid_header_time_buffer.front();                                                // generate lidar_frame_beg_time
      meas.lidar_frame_end_time = meas.lidar_frame_beg_time + meas.lidar->points.back().curvature / double(1000); // calc lidar scan end time
      meas.pcl_proc_cur = meas.lidar;
      lidar_pushed = true;                                                                                       // flag
    }

    if (imu_en && last_timestamp_imu < meas.lidar_frame_end_time)
    { // waiting imu message needs to be
      // larger than _lidar_frame_end_time,
      // make sure complete propagate.
      // fast_livo::log_info("out sync");
      return false;
    }

    struct MeasureGroup m; // standard method to keep imu message.

    m.imu.clear();
    m.lio_time = meas.lidar_frame_end_time;
    mtx_buffer.lock();
    while (!imu_buffer.empty())
    {
      if (imu_buffer.front()->timestamp.seconds() > meas.lidar_frame_end_time) break;
      m.imu.push_back(imu_buffer.front());
      imu_buffer.pop_front();
    }
    lid_raw_data_buffer.pop_front();
    lid_header_time_buffer.pop_front();
    mtx_buffer.unlock();
    sig_buffer.notify_all();

    meas.lio_vio_flg = LIO; // process lidar topic, so timestamp should be lidar scan end.
    meas.measures.push_back(m);
    // fast_livo::log_info("ONlY HAS LiDAR and IMU, NO IMAGE!");
    lidar_pushed = false; // sync one whole lidar scan.
    if(!img_en)keyframe_time = meas.lidar_frame_end_time;
    return true;

    break;
  }

  case LIVO:
  {
    /*** For LIVO mode, the time of LIO update is set to be the same as VIO, LIO
     * first than VIO imediatly ***/
    EKF_STATE last_lio_vio_flg = meas.lio_vio_flg;
    // double t0 = omp_get_wtime();
    switch (last_lio_vio_flg)
    {
    // double img_capture_time = meas.lidar_frame_beg_time + exposure_time_init;
    case WAIT:
    case VIO:
    {
      // printf("!!! meas.lio_vio_flg: %d \n", meas.lio_vio_flg);
      double img_capture_time = img_time_buffer.front() + exposure_time_init;
      if(img_en)keyframe_time = img_capture_time;
      /*** has img topic, but img topic timestamp larger than lidar end time,
       * process lidar topic. After LIO update, the meas.lidar_frame_end_time
       * will be refresh. ***/
      if (meas.last_lio_update_time < 0.0) meas.last_lio_update_time = lid_header_time_buffer.front();
      // printf("[ Data Cut ] wait \n");
      // printf("[ Data Cut ] last_lio_update_time: %lf \n",
      // meas.last_lio_update_time);

      double lid_newest_time = lid_header_time_buffer.back() + lid_raw_data_buffer.back()->points.back().curvature / double(1000);
      double imu_newest_time = imu_buffer.back()->timestamp.seconds();

      if (img_capture_time < meas.last_lio_update_time + 0.00001)
      {
        img_buffer.pop_front();
        img_time_buffer.pop_front();
        fast_livo::log_info("[ Data Cut ] Throw one image frame! \n");
        return false;
      }

      if (img_capture_time > lid_newest_time || img_capture_time > imu_newest_time)
      {
        // fast_livo::log_info("lost first camera frame");
        // printf("img_capture_time, lid_newest_time, imu_newest_time: %lf , %lf
        // , %lf \n", img_capture_time, lid_newest_time, imu_newest_time);
        return false;
      }

      struct MeasureGroup m;

      // printf("[ Data Cut ] LIO \n");
      // printf("[ Data Cut ] img_capture_time: %lf \n", img_capture_time);
      m.imu.clear();
      m.rtk.timestamp = 0.0;
      m.rtk.p = Eigen::Vector3d::Zero();
      m.lio_time = img_capture_time;
      mtx_buffer.lock();

      if (rtk_en && rtk_ini)
      {
          while (!rtk_buffer.empty())
          {
            fast_livo::log_info("[SyncRTK] RTK_buffer front time: %.6f, last_lio_update_time: %.6f, lio_time: %.6f", rtk_buffer.front().timestamp, meas.last_lio_update_time, m.lio_time);
            if (rtk_buffer.front().timestamp > m.lio_time) break;
            if (rtk_buffer.front().timestamp > meas.last_lio_update_time) 
            {
                m.rtk = rtk_buffer.front();
                break;
            }
            rtk_buffer.pop_front();
          }
          fast_livo::log_info("[SyncRTK] RTK.pushed to MeasureGroup: %.6f, %.3f, %.3f, %.3f", m.rtk.timestamp, m.rtk.p[0], m.rtk.p[1], m.rtk.p[2]);
      }
      
      while (!imu_buffer.empty())
      {
        if (imu_buffer.front()->timestamp.seconds() > m.lio_time) break;

        if (imu_buffer.front()->timestamp.seconds() > meas.last_lio_update_time)
          m.imu.push_back(imu_buffer.front());

        imu_buffer.pop_front();
        // printf("[ Data Cut ] imu time: %lf \n",
        // imu_buffer.front()->timestamp.seconds());
      }
      mtx_buffer.unlock();
      sig_buffer.notify_all();

      *(meas.pcl_proc_cur) = *(meas.pcl_proc_next);
      PointCloudXYZI().swap(*meas.pcl_proc_next);

      int lid_frame_num = lid_raw_data_buffer.size();
      int max_size = meas.pcl_proc_cur->size() + 24000 * lid_frame_num;
      meas.pcl_proc_cur->reserve(max_size);
      meas.pcl_proc_next->reserve(max_size);
      // deque<PointCloudXYZI::Ptr> lidar_buffer_tmp;

      while (!lid_raw_data_buffer.empty())
      {
        if (lid_header_time_buffer.front() > img_capture_time) break;
        auto pcl(lid_raw_data_buffer.front()->points);
        double frame_header_time(lid_header_time_buffer.front());
        float max_offs_time_ms = (m.lio_time - frame_header_time) * 1000.0f;

        for (int i = 0; i < pcl.size(); i++)
        {
          auto pt = pcl[i];
          if (pcl[i].curvature < max_offs_time_ms)
          {
            pt.curvature += (frame_header_time - meas.last_lio_update_time) * 1000.0f;
            meas.pcl_proc_cur->points.push_back(pt);
          }
          else
          {
            pt.curvature += (frame_header_time - m.lio_time) * 1000.0f;
            meas.pcl_proc_next->points.push_back(pt);
          }
        }
        lid_raw_data_buffer.pop_front();
        lid_header_time_buffer.pop_front();
      }

      meas.measures.push_back(m);
      meas.lio_vio_flg = LIO;
      // meas.last_lio_update_time = m.lio_time;
      // printf("!!! meas.lio_vio_flg: %d \n", meas.lio_vio_flg);
      // printf("[ Data Cut ] pcl_proc_cur number: %d \n", meas.pcl_proc_cur
      // ->points.size()); printf("[ Data Cut ] LIO process time: %lf \n",
      // omp_get_wtime() - t0);
      return true;
    }

    case LIO:
    {
      double img_capture_time = img_time_buffer.front() + exposure_time_init;
      meas.lio_vio_flg = VIO;
      // printf("[ Data Cut ] VIO \n");
      meas.measures.clear();
      double imu_time = imu_buffer.front()->timestamp.seconds();

      struct MeasureGroup m;
      m.vio_time = img_capture_time;
      m.lio_time = meas.last_lio_update_time;
      m.img = img_buffer.front();
      mtx_buffer.lock();
      
      // while ((!imu_buffer.empty() && (imu_time < img_capture_time)))
      // {
      //   imu_time = imu_buffer.front()->timestamp.seconds();
      //   if (imu_time > img_capture_time) break;
      //   m.imu.push_back(imu_buffer.front());
      //   imu_buffer.pop_front();
      //   printf("[ Data Cut ] imu time: %lf \n",
      //   imu_buffer.front()->timestamp.seconds());
      // }

      img_buffer.pop_front();
      img_time_buffer.pop_front();
      mtx_buffer.unlock();
      sig_buffer.notify_all();
      meas.measures.push_back(m);
      lidar_pushed = false; // after VIO update, the _lidar_frame_end_time will be refresh.
      // printf("[ Data Cut ] VIO process time: %lf \n", omp_get_wtime() - t0);
      return true;
    }

    default:
    {
      // printf("!! WRONG EKF STATE !!");
      return false;
    }
      // return false;
    }
    break;
  }

  case ONLY_LO:
  {
    if (!lidar_pushed) 
    { 
      // If not in lidar scan, need to generate new meas
      if (lid_raw_data_buffer.empty())  return false;
      meas.lidar = lid_raw_data_buffer.front(); // push the first lidar topic
      meas.lidar_frame_beg_time = lid_header_time_buffer.front(); // generate lidar_beg_time
      meas.lidar_frame_end_time  = meas.lidar_frame_beg_time + meas.lidar->points.back().curvature / double(1000); // calc lidar scan end time
      lidar_pushed = true;             
    }
    struct MeasureGroup m; // standard method to keep imu message.
    m.lio_time = meas.lidar_frame_end_time;
    mtx_buffer.lock();
    lid_raw_data_buffer.pop_front();
    lid_header_time_buffer.pop_front();
    mtx_buffer.unlock();
    sig_buffer.notify_all();
    lidar_pushed = false; // sync one whole lidar scan.
    meas.lio_vio_flg = LO; // process lidar topic, so timestamp should be lidar scan end.
    meas.measures.push_back(m);
    return true;
    break;
  }

  default:
  {
    printf("!! WRONG SLAM TYPE !!");
    return false;
  }
  }
  fast_livo::log_info("out sync");
}

void LIVMapper::publish_img_rgb(const std::string &topic, VIOManagerPtr vio_manager)
{
  if(outputs_.image) outputs_.image(topic, {outputs_.now(), "", vio_manager->img_cp});
}

void LIVMapper::publish_frame_world(const std::string &pubLaserCloudFullRes, VIOManagerPtr vio_manager)
{
  fast_livo::log_info("Publish frame world pointcloud.");
  if (pcl_w_wait_pub->empty()) return;
  PointCloudXYZRGB::Ptr laserCloudWorldRGB(new PointCloudXYZRGB());
  if (img_en)
  {
    *pcl_wait_pub += *pcl_w_wait_pub;
    if(pub_num == pub_scan_num)
    {
      pub_num = 1;
      size_t size = pcl_wait_pub->points.size();
      laserCloudWorldRGB->reserve(size);
      // double inv_expo = _state.inv_expo_time;
      cv::Mat img_rgb = vio_manager->img_rgb;
      for (size_t i = 0; i < size; i++)
      {
        PointTypeRGB pointRGB;
        pointRGB.x = pcl_wait_pub->points[i].x;
        pointRGB.y = pcl_wait_pub->points[i].y;
        pointRGB.z = pcl_wait_pub->points[i].z;

        V3D p_w(pcl_wait_pub->points[i].x, pcl_wait_pub->points[i].y, pcl_wait_pub->points[i].z);
        V3D pf(vio_manager->new_frame_->w2f(p_w)); if (pf[2] < 0) continue;
        V2D pc(vio_manager->new_frame_->w2c(p_w));

        if (vio_manager->new_frame_->cam_->isInFrame(pc.cast<int>(), 3)) // 100
        {
          V3F pixel = vio_manager->getInterpolatedPixel(img_rgb, pc);
          pointRGB.r = pixel[2];
          pointRGB.g = pixel[1];
          pointRGB.b = pixel[0];
          // pointRGB.r = pixel[2] * inv_expo; pointRGB.g = pixel[1] * inv_expo; pointRGB.b = pixel[0] * inv_expo;
          // if (pointRGB.r > 255) pointRGB.r = 255;
          // else if (pointRGB.r < 0) pointRGB.r = 0;
          // if (pointRGB.g > 255) pointRGB.g = 255;
          // else if (pointRGB.g < 0) pointRGB.g = 0;
          // if (pointRGB.b > 255) pointRGB.b = 255;
          // else if (pointRGB.b < 0) pointRGB.b = 0;
          if (pf.norm() > blind_rgb_points) laserCloudWorldRGB->push_back(pointRGB);
        }
      }
    }
    else
    {
      pub_num++;
    }
  }

  /*** Publish Frame ***/
  fast_livo::CloudSample laserCloudmsg;
  if(img_en) pcl::toPCLPointCloud2(*laserCloudWorldRGB,laserCloudmsg.cloud);
  else pcl::toPCLPointCloud2(*pcl_w_wait_pub,laserCloudmsg.cloud);
  laserCloudmsg.timestamp=outputs_.now();
  laserCloudmsg.frame_id="camera_init";
  if(outputs_.cloud) outputs_.cloud(pubLaserCloudFullRes,laserCloudmsg);

  if(img_en)
  {
    PointCloudXYZRGB::Ptr laserCloudBodyRGB(new PointCloudXYZRGB());
    laserCloudBodyRGB->reserve(laserCloudWorldRGB->size());
    for (const auto& pt_world_rgb : laserCloudWorldRGB->points)
    {
          V3D p_global(pt_world_rgb.x, pt_world_rgb.y, pt_world_rgb.z);
          V3D p_body = _state.rot_end.transpose() * (p_global - _state.pos_end);
          PointTypeRGB pointBodyRGB;
          pointBodyRGB.x = p_body(0);
          pointBodyRGB.y = p_body(1);
          pointBodyRGB.z = p_body(2);
          pointBodyRGB.r = pt_world_rgb.r;
          pointBodyRGB.g = pt_world_rgb.g;
          pointBodyRGB.b = pt_world_rgb.b;
          laserCloudBodyRGB->push_back(pointBodyRGB);
    }
    fast_livo::CloudSample laserCloudbodymsg;
    pcl::toPCLPointCloud2(*laserCloudBodyRGB,laserCloudbodymsg.cloud);
    laserCloudbodymsg.timestamp=fast_livo::Timestamp::from_seconds(keyframe_time);
    laserCloudbodymsg.frame_id="camera_init";
    if(outputs_.cloud) outputs_.cloud(pub_lidarRGB,laserCloudbodymsg);

    if(debug_mode)
    {
      if (laserCloudBodyRGB->size() > 0) 
      {
          std::stringstream ss;
          ss << std::setfill('0') << std::setw(6) << pcd_file_index;
          std::string pcd_filename = pcd_save_file + ss.str() + ".pcd";    
          pcl::io::savePCDFileBinary(pcd_filename, *laserCloudBodyRGB);
          pcd_file_index++;
      }
    }
  }
  
  if(!img_en)
  {
    pcl::PointCloud<pcl::PointXYZI>::Ptr laserCloudBody(new pcl::PointCloud<pcl::PointXYZI>());
    laserCloudBody->reserve(pcl_w_wait_pub->size());
    for (const auto& pt_world : pcl_w_wait_pub->points)
    {
          V3D p_global(pt_world.x, pt_world.y, pt_world.z);
          V3D p_body = _state.rot_end.transpose() * (p_global - _state.pos_end);
          pcl::PointXYZI pointBody;
          pointBody.x = p_body(0);
          pointBody.y = p_body(1);
          pointBody.z = p_body(2);
          laserCloudBody->push_back(pointBody);
    }
      fast_livo::CloudSample laserCloudbodymsg;
    pcl::toPCLPointCloud2(*laserCloudBody,laserCloudbodymsg.cloud);
    laserCloudbodymsg.timestamp=fast_livo::Timestamp::from_seconds(keyframe_time);
    laserCloudbodymsg.frame_id="camera_init";
    if(outputs_.cloud) outputs_.cloud(pub_lidarRGB,laserCloudbodymsg);

      if(debug_mode)
      {
        if (laserCloudBody->size() > 0) 
        {
          std::stringstream ss;
          ss << std::setfill('0') << std::setw(6) << pcd_file_index;
          std::string pcd_filename = pcd_save_file + ss.str() + ".pcd";
          pcl::io::savePCDFileBinary(pcd_filename, *laserCloudBody);
          pcd_file_index++;
        }
      }
  }

  
  std::vector<double> livo_xyz(4);
  livo_xyz[0] = keyframe_time;
  livo_xyz[1] = _state.pos_end(0);
  livo_xyz[2] = _state.pos_end(1);
  livo_xyz[3] = _state.pos_end(2);
  livo_state_buffer.push_back(livo_xyz);


  fast_livo::PoseSample odom_msg_ekf;
  odom_msg_ekf.timestamp = fast_livo::Timestamp::from_seconds(keyframe_time);
  odom_msg_ekf.frame_id = "camera_init";
  odom_msg_ekf.position.x = _state.pos_end(0);
  odom_msg_ekf.position.y = _state.pos_end(1);
  odom_msg_ekf.position.z = _state.pos_end(2);
  Eigen::Quaterniond q(_state.rot_end);
  odom_msg_ekf.orientation.w = q.w();
  odom_msg_ekf.orientation.x = q.x();
  odom_msg_ekf.orientation.y = q.y();
  odom_msg_ekf.orientation.z = q.z();
  odom_msg_ekf.velocity.x = _state.vel_end(0);
  odom_msg_ekf.velocity.y = _state.vel_end(1);
  odom_msg_ekf.velocity.z = _state.vel_end(2);
  Eigen::Map<Eigen::Matrix<double, 6, 6, Eigen::RowMajor>> pose_cov_map(odom_msg_ekf.covariance.data());

  pose_cov_map.block<3, 3>(0, 0) = _state.cov.block<3, 3>(3, 3);
  pose_cov_map.block<3, 3>(3, 3) = _state.cov.block<3, 3>(0, 0);
  pose_cov_map.block<3, 3>(0, 3) = _state.cov.block<3, 3>(3, 0);
  pose_cov_map.block<3, 3>(3, 0) = _state.cov.block<3, 3>(0, 3);
  if(outputs_.pose) outputs_.pose(pub_odom,odom_msg_ekf); 

  if(debug_mode)
  {
    std::ofstream odom_file(odom_save_file, std::ios::app); 
    if (odom_file.is_open()) {
        odom_file << std::fixed << std::setprecision(6) 
                  << keyframe_time << " "
                  << _state.pos_end(0) << " "
                  << _state.pos_end(1) << " "
                  << _state.pos_end(2) << " "
	                  << q.x() << " "
	                  << q.y() << " "
	                  << q.z() << " "
	                  << q.w() << " "
	                  << _state.vel_end(0) << " "
	                  << _state.vel_end(1) << " "
	                  << _state.vel_end(2) << std::endl;
        odom_file.close();
    }

    std::ofstream cov_file(cov_save_file, std::ios::app);
    if (cov_file.is_open()) {
        cov_file << std::fixed << std::setprecision(6) << keyframe_time;
        
        for (int i = 0; i < 36; ++i) {
            cov_file << " " << odom_msg_ekf.covariance[i];
        }
        cov_file << std::endl;
        cov_file.close();
    }
  }
  
  /**************** save map ****************/
  /* 1. make sure you have enough memories
  /* 2. noted that pcd save will influence the real-time performences **/
  if (pcd_save_en)
  {
    int size = feats_undistort->points.size();
    PointCloudXYZI::Ptr laserCloudWorld(new PointCloudXYZI(size, 1));

    if (img_en)
    {
      *pcl_wait_save += *laserCloudWorldRGB;
    }
    else
    {
      *pcl_wait_save_intensity += *pcl_w_wait_pub;
    }
    scan_wait_num++;

    if ((pcl_wait_save->size() > 0 || pcl_wait_save_intensity->size() > 0) && pcd_save_interval > 0 && scan_wait_num >= pcd_save_interval)
    {
      pcd_index++;
      string all_points_dir(string(root_dir + "Log/PCD/") + to_string(pcd_index) + string(".pcd"));
      pcl::PCDWriter pcd_writer;
      if (pcd_save_en)
      {
        cout << "current scan saved to /PCD/" << all_points_dir << endl;
        if (img_en)
        {
          pcd_writer.writeBinary(all_points_dir, *pcl_wait_save); // pcl::io::savePCDFileASCII(all_points_dir, *pcl_wait_save);
          PointCloudXYZRGB().swap(*pcl_wait_save);
        }
        else
        {
          pcd_writer.writeBinary(all_points_dir, *pcl_wait_save_intensity);
          PointCloudXYZI().swap(*pcl_wait_save_intensity);
        }        
        Eigen::Quaterniond q(_state.rot_end);
        fout_pcd_pos << _state.pos_end[0] << " " << _state.pos_end[1] << " " << _state.pos_end[2] << " " << q.w() << " " << q.x() << " " << q.y()
                     << " " << q.z() << " " << endl;
        scan_wait_num = 0;
      }
    }
  }
  if(laserCloudWorldRGB->size() > 0)  PointCloudXYZI().swap(*pcl_wait_pub); 
  PointCloudXYZI().swap(*pcl_w_wait_pub);
}

void LIVMapper::publish_visual_sub_map(const std::string &topic)
{
  if(visual_sub_map->empty()) return;
  fast_livo::CloudSample sample;
  pcl::toPCLPointCloud2(*visual_sub_map,sample.cloud);
  sample.timestamp=outputs_.now(); sample.frame_id="camera_init";
  if(outputs_.cloud) outputs_.cloud(topic,sample);
}

void LIVMapper::publish_effect_world(const std::string &pubLaserCloudEffect, const std::vector<PointToPlane> &ptpl_list)
{
  int effect_feat_num = ptpl_list.size();
  PointCloudXYZI::Ptr laserCloudWorld(new PointCloudXYZI(effect_feat_num, 1));
  for (int i = 0; i < effect_feat_num; i++)
  {
    laserCloudWorld->points[i].x = ptpl_list[i].point_w_[0];
    laserCloudWorld->points[i].y = ptpl_list[i].point_w_[1];
    laserCloudWorld->points[i].z = ptpl_list[i].point_w_[2];
  }
  fast_livo::CloudSample sample;
  pcl::toPCLPointCloud2(*laserCloudWorld,sample.cloud);
  sample.timestamp=outputs_.now(); sample.frame_id="camera_init";
  if(outputs_.cloud) outputs_.cloud(pubLaserCloudEffect,sample);
}

template <typename T> void LIVMapper::set_posestamp(T &out)
{
  out.position={_state.pos_end(0),_state.pos_end(1),_state.pos_end(2)};
  out.orientation=geoQuat;
}

void LIVMapper::publish_odometry(const std::string &topic)
{
  odomAftMapped.frame_id="camera_init"; odomAftMapped.child_frame_id="aft_mapped";
  odomAftMapped.timestamp=outputs_.now();
  set_posestamp(odomAftMapped);
  if(outputs_.pose) outputs_.pose(topic,odomAftMapped);
}

void LIVMapper::publish_mavros(const std::string &topic)
{
  msg_body_pose.timestamp=outputs_.now(); msg_body_pose.frame_id="camera_init";
  set_posestamp(msg_body_pose);
  if(outputs_.pose) outputs_.pose(topic,msg_body_pose);
}

void LIVMapper::publish_path(const std::string)
{
  msg_body_pose.timestamp=outputs_.now(); msg_body_pose.frame_id="camera_init";
  set_posestamp(msg_body_pose); path.push_back(msg_body_pose);
  if(outputs_.path) outputs_.path(path);
}
