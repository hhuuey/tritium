phi_sdk_username = 'xiaolu.jiang'
phi_sdk_password = 'JXLwo@198981'
hdmap_topic_name = "HDMAP_LANE_RECONSTRUCT_BAIDU"
hdmap_fix_topic_name = "HDMAP_LANE_RECONSTRUCT_MAPPING_FIX"
calib_topic_name = "LIDAR_RECON_INPUT_MCLIP_MAPPING"

# from config.config_base import *

from phi_dataset_sdk.dataset import client
from phi_dataset_sdk.model.slice import SliceKey

from cozy import eb

import os
import json
from pathlib import Path


def query_calib_path(clip_id, start_ts, end_ts):
    lidar_construct_dir = query_workflow(clip_id, start_ts, end_ts, calib_topic_name)
    eb.huey_log(eb.LoggerLevel.INFO,
                f"Queried lidar construct directory: {lidar_construct_dir}",
                module_name="parser.parse_workflow")

    if not lidar_construct_dir:
        eb.huey_log(
            eb.LoggerLevel.ERROR,
            f"Failed to query calibration topic path for clip_id: {clip_id}, start_ts: {start_ts}, end_ts: {end_ts}",
            module_name="parser.parse_workflow",
        )
        return ''

    json_files = list(Path(lidar_construct_dir).glob("*.json"))
    if len(json_files) != 1:
        eb.huey_log(
            eb.LoggerLevel.ERROR,
            f"Expected exactly one JSON file in lidar construct directory: {lidar_construct_dir}, but found {len(json_files)}",
            module_name="parser.parse_workflow")
        return ''

    calib_base_dir = str(json_files[0])
    json_data = json.loads(open(calib_base_dir).read())
    calib_base_dir = os.path.join(json_data["calibration"]["v1"]["mount_prefix"],
                                  json_data["calibration"]["v1"]["path"])

    return calib_base_dir


def query_workflow(clip_id, start_ts, end_ts, topic_name):
    eb.huey_log(
        eb.LoggerLevel.ERROR,
        f"Querying workflow for clip_id: {clip_id}, start_ts: {start_ts}, end_ts: {end_ts}, topic_name:{topic_name}",
        module_name="parser.parse_workflow",
    )

    dataset_client = phigent_dataset_sdk_auth('prod')
    if not dataset_client:
        eb.huey_log(eb.LoggerLevel.INFO,
                    "Failed to authenticate with dataset SDK.",
                    module_name="parser.parse_workflow")
        return ''

    topic_item = {"topic": topic_name, "version": "latest"}
    status, slice_topic_oss_info_result = dataset_client.get_slice_topic_oss_info(
        SliceKey(clip_id=clip_id, start_ts=start_ts, end_ts=end_ts), [topic_item])
    if status != 0:
        eb.huey_log(
            eb.LoggerLevel.INFO,
            f"Failed to query workflow topic path for clip_id: {clip_id}, start_ts: {start_ts}, end_ts: {end_ts}",
            module_name="parser.parse_workflow")
        return ''

    bucket_id_mount_prefix_map = {b["id"]: b["mount_prefix"] for b in slice_topic_oss_info_result["buckets"]}
    for i in range(len(slice_topic_oss_info_result["topics"])):
        if slice_topic_oss_info_result["topics"][i]["topic"] == topic_name:
            base_path = os.path.join(bucket_id_mount_prefix_map[slice_topic_oss_info_result["topics"][i]["bucket_id"]],\
                slice_topic_oss_info_result["topics"][i]["oss_path"])
            return base_path

    return ''


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
def list_dataset_info(dataset_name):
    dataset_client = phigent_dataset_sdk_auth('prod')
    results = []
    clip_result = dataset_client.list_data(dataset_name)
    if clip_result[0] != 0:
        return []
    slice_infos = clip_result[1]
    for slice_info in slice_infos:
        results.append([slice_info['clip_id'], slice_info['start_ts'], slice_info['end_ts']])
    return results


if __name__ == '__main__':

    clip_id = "LGB818931_20260108T100512_U1JKQT"
    start_ts = "1767837981000"
    end_ts = "1767838041000"

    workflow_query = query_workflow(
        clip_id,
        start_ts,
        end_ts,
        hdmap_topic_name,
        # "LIDAR_RECON_MCLIP_MAPPING",
    )
    eb.huey_log(eb.LoggerLevel.INFO, workflow_query, module_name="parser.parse_workflow")

    # calib_dir = query_calib_path(clip_id, start_ts, end_ts)
    # eb.huey_log(eb.LoggerLevel.INFO, f"Calibration directory: {calib_dir}", module_name="parser.parse_workflow")
