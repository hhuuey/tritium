import json

from attr import field
from mcap.writer import Writer as McapfWriter
from typing import List

from dataclasses import dataclass, asdict, field
from typing import List


@dataclass
class Point2D11:
    x: float
    y: float
    inside_pts: List[str] = field(default_factory=list)


point = Point2D11(x=1.0, y=5.0, inside_pts=["a", "b", "c"])
p_dict = asdict(point)
json_str = json.dumps(p_dict).encode("utf-8")

ts_ns = 1741943086041783040


def get_own_datastruct_writer(file_path: str) -> McapfWriter:
    stream = open(file_path, "wb")
    writer = McapfWriter(stream)
    writer.start()
    return writer

def write_mcap_raw(writer, p_dict):
    # writer.start()
    channel_id = writer.register_channel(
        schema_id=0,  # 0: schema-less data
        topic="person_topic",
        message_encoding="json",
    )

    writer.add_message(
        channel_id=channel_id,
        log_time=ts_ns,
        data=json.dumps(p_dict).encode("utf-8"),
        publish_time=ts_ns,
    )
    # writer.finish()

writer = get_own_datastruct_writer("test.mcap")
write_mcap_raw(writer, p_dict)
writer.finish()