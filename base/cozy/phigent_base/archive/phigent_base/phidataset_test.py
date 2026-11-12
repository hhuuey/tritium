from phi_dataset_sdk.dataset import client
from phi_dataset_sdk.model.slice import SliceKey

import os
from dataclasses import dataclass

phi_sdk_username = 'xiaolu.jiang'
phi_sdk_password = 'JXLwo@198981'

# 某些工作流的输出话题
workflow_topic_names = [
    "LANE_RECON",  # 行车车道线重建
    "LANE_RECON_PARK",  # 泊车车道线重建
    "LD_LANE_RECON",  # ldmap车道线方案
    "HDMAP_LANE_RECONSTRUCT_BAIDU",  # hdmap车道线方案
    "LIDAR_RECON_MCLIP_MAPPING"  # 4d点云重建
]


@dataclass
class SliceInfo:
    clip_id: str = ''
    slice_id: str = ''
    start_ts: int = 0
    end_ts: int = 0
    name: str = ''
    actual_start_ts: int = 0
    actual_end_ts: int = 0


# 初始化dataset sdk
def phigent_dataset_sdk_auth(environment: str = 'prod'):
    try:
        dataset_client = client.DatasetClient(environment)
        dataset_client.auth(username=phi_sdk_username, password=phi_sdk_password)
        return dataset_client
    except Exception as e:
        print(f"initialize dataset client error: {e}")
        return None


# 通过dataset sdk查询某个数据集下的所有clip_id
def list_dataset_info(dataset_client, dataset_name):
    results = []
    clip_result = dataset_client.list_data(dataset_name)
    if clip_result[0] != 0:
        return []
    slice_infos = clip_result[1]
    for slice_info in slice_infos:
        results.append(
            SliceInfo(clip_id=slice_info['clip_id'],
                      slice_id=slice_info['slice_id'],
                      start_ts=slice_info['start_ts'],
                      end_ts=slice_info['end_ts'],
                      name=slice_info['name'],
                      actual_start_ts=slice_info['act_start_ts'],
                      actual_end_ts=slice_info['act_end_ts']))
    return results


# query某个clip_id的slice信息
def query_slice_info(dataset_client, clip_id, start_ts, end_ts):
    slice_info = dataset_client.get_slice_info(SliceKey(clip_id=clip_id, start_ts=start_ts, end_ts=end_ts))
    if slice_info is None:
        return None
    return SliceInfo(clip_id=slice_info['clip_id'],
                     slice_id=slice_info['slice_id'],
                     start_ts=slice_info['start_ts'],
                     end_ts=slice_info['end_ts'],
                     name=slice_info['name'],
                     actual_start_ts=slice_info['act_start_ts'],
                     actual_end_ts=slice_info['act_end_ts'])


# 通过dataset sdk查询某个工作流对应的输出路径
def query_wkflow_topic_path_from_clip_id(dataset_client, topic_name, slice_key):
    # 如果是基础的topic，就不用添加version字段
    topic_item = {"topic": topic_name, "version": "latest"}
    status, slice_topic_oss_info_result = dataset_client.get_slice_topic_oss_info(slice_key, [topic_item])
    if status != 0:
        return None
    bucket_id_mount_prefix_map = {b["id"]: b["mount_prefix"] for b in slice_topic_oss_info_result["buckets"]}
    for i in range(len(slice_topic_oss_info_result["topics"])):
        if slice_topic_oss_info_result["topics"][i]["topic"] == topic_name:
            return os.path.join(bucket_id_mount_prefix_map[slice_topic_oss_info_result["topics"][i]["bucket_id"]],\
                slice_topic_oss_info_result["topics"][i]["oss_path"])
    return None


def form_slice_key(clip_id, start_ts, end_ts):
    return SliceKey(clip_id=clip_id, start_ts=start_ts, end_ts=end_ts)


if __name__ == "__main__":
    dataset_client = phigent_dataset_sdk_auth('prod')
    dataset_name = 'ramp-0625-jxl'
    slice_infos = list_dataset_info(dataset_client, dataset_name)
    slice_test = slice_infos[0]
    slice_info = dataset_client.get_slice_info(
        form_slice_key(slice_test.clip_id, slice_test.start_ts, slice_test.end_ts))

    path = query_wkflow_topic_path_from_clip_id(dataset_client, 'LANE_RECON', \
        form_slice_key(slice_test.clip_id, slice_test.start_ts, slice_test.end_ts))
    print("phigent dataset sdk auth")