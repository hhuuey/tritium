from __future__ import annotations

import hashlib
import re


def mapping_key_for_scene(scene_id: str) -> str:
    normalized = re.sub(r"[^A-Za-z0-9_]", "_", scene_id).strip("_") or "scene"
    digest = hashlib.sha1(scene_id.encode("utf-8")).hexdigest()[:10]
    return f"{normalized[:40]}_{digest}"
