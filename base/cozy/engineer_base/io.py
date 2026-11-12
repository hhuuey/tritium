import os
import json


def list_files(path, recursive=True, follow_symlinks=False, ext=None):
    if not recursive:
        return [
            os.path.join(path, f) for f in os.listdir(path)
            if os.path.isfile(os.path.join(path, f)) and (ext is None or f.endswith(ext))
        ]

    files = []
    for dirpath, dirnames, filenames in os.walk(path, followlinks=follow_symlinks):
        for fn in filenames:
            if ext is None or fn.endswith(ext):
                files.append(os.path.join(dirpath, fn))
    return files


def load_json_line(file, max_num=-1):
    objs = []
    with open(file, 'r') as f:
        while True:
            line = f.readline()
            if not line:
                break
            objs.append(json.loads(line))
            if 0 < max_num == len(objs):
                break
    return objs


def load_json_file(file):
    with open(file, 'r', encoding='utf-8') as f:
        return json.load(f)


def write_json_file(file, payload, indent=2, ensure_ascii=False):
    output_dir = os.path.dirname(file)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    with open(file, 'w', encoding='utf-8') as f:
        json.dump(payload, f, indent=indent, ensure_ascii=ensure_ascii)
