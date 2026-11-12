from phi_data_sdk.phi_data.phi_data import PhiData

import os
import json
from typing import List
from dataclasses import dataclass, field

mount_prefix = "/mnt/goosefs/data-preprocess/"

raw_topic_names = [
    'PCO_GlobalLocalization',  # 全局定位信息
    "PCO_SceneRecovery",  # 场景重建话题
]


@dataclass
class ClipInfo:
    clip_id: str = ''
    start_ts: int = 0
    end_ts: int = 0
    duration: int = 0
    main_lidar: str = ''
    topics: List[str] = field(default_factory=list)

@dataclass
class TopicInfo:
    topic_name: str = ''
    start_ts: int = 0
    end_ts: int = 0
    fps: float = 0.0
    total_frames: int = 0

# 初始化data sdk
def phigent_data_sdk_auth(environment: str = 'prod'):
    try:
        data_client = PhiData(environment)
    except Exception as e:
        print(f"initialize data client error: {e}")
        return None
    return data_client


# 通过data sdk查询clip相关信息
def query_clip_info(data_client, clip_id):
    clip_info = data_client.get_clip_info(clip_id)
    if clip_info is None:
        return None
    clip_info = ClipInfo(
        clip_id=clip_info.Meta.ID,
        start_ts=clip_info.Meta.StartTs,
        end_ts=clip_info.Meta.EndTs,
        duration=clip_info.Meta.Duration,
        topics=clip_info.Topics.Topics,
        main_lidar=clip_info.Topics.MainLidarTopic,
    )
    return clip_info


# 通过data sdk查询clip的某个话题信息
def query_topic_info(data_client, clip_id, topic_name):
    topic_info = data_client.get_topic_info(clip_id, topic_name)
    if topic_info is None:
        return None
    topic_info = TopicInfo(
        topic_name=topic_name,
        start_ts=topic_info.StartTs,
        end_ts=topic_info.EndTs,
        fps=topic_info.Fps,
        total_frames=topic_info.TotalFrame
    )
    return topic_info


# 通过data sdk下载clip的指定话题到指定目录
def download_clip_topic(data_client, clip_id, topics, output_path):
    try:
        data_client.download(clip_id, output_path, topics=topics)
    except Exception as e:
        print(f"download clip {clip_id} error: {e}")
        return False
    print(f"download clip {clip_id} to {output_path}")
    return True


# topic_file_info对应的消息格式
# <str>, <List[TsFile]>: 返回topic数据根路径以及`TsFile` 对象的列表：
# - 每个 `TsFile` 对象包含以下字段
# -- Ts <int>: 文件的时间戳，表示该文件对应的数据的时间点，单位为毫秒。
# -- Path <str>: 文件的存储路径，表示该文件在系统中的存储位置。仅在 `need_path=True` 时返回。
# -- Line <int>: 文件中的行号或数据块的编号，表示文件中的数据位置或片段编号。
def query_topic_content(data_client, clip_id, topic_name):
    result = {}
    topic_info = data_client.get_topic_info(clip_id, topic_name)
    if topic_info is None:
        return None
    topic_file_info = data_client.get_topic_range_list(clip_id,
                                                       topic_name,
                                                       topic_info.StartTs,
                                                       topic_info.EndTs,
                                                       need_path=True)
    topic_root = topic_file_info[0]

    with open(os.path.join(mount_prefix, topic_root), "r", encoding="utf8") as f_globalloc:
        datas = []
        try:
            while True:
                line = f_globalloc.readline()
                if not line:
                    break
                datas.append(json.loads(line))
        except:
            return None

    for tsfile in topic_file_info[1]:
        result[tsfile.Ts] = datas[tsfile.Line - 1]

    return result


if __name__ == "__main__":
    data_client = phigent_data_sdk_auth('prod')
    clip_id = '17A5482S0_20250210T181931'
    clip_info = query_clip_info(data_client, clip_id)
    topic_info = query_topic_info(data_client, clip_id, 'PCO_GlobalLocalization')
    topic_content = query_topic_content(data_client, clip_id, 'PCO_GlobalLocalization')


    print("phigent dataset sdk auth")
