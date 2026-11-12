map builder
- collated_trajectery_builder
    - sensor_collector
    - global_trajectory_builder
        - pose_graph
        - local_trajectory_builder


sensor_brideg->
	trajectory_builder->addsensordata (collect trajectory builder)
				       ->makaedaspachtchable(可以访问到trajectory_builder)
	sensor_collector ->addsensordata(做数据同步) collator类
					同步完成后 队列调用callback(collatortrajectorybuilder)
									 实际为handlecollectsensordata

	handlecollectsensordata最终传入了global trajectory builder里面
	global trajectory builder就开始真正 addsensordata



Cartographer激光帧的时间戳是最后一个点对应的时间戳
pointcloud.points[i].time 对应的时间戳是是相对于帧的时间戳