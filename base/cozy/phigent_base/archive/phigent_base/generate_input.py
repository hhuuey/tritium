import os

def lidar_reconstruct_cmd(clip_id, start_ts, end_ts, output_dir):
    cmd = f'/root/miniconda3/envs/vma/bin/python3 tools/gen_input_file/gen_input_file.py -t lidar_reconstruct -cid "{clip_id}" -st {start_ts} -et {end_ts} -td "{output_dir}" -p \'{{"mode":"MCLIP_MAPPING","is_park":0,"bev_ver":"v11","seg_ver":"v6","det2d_ver":"v7"}}\''
    return cmd


# mode:
# -- ldmap LDMAP车道线重建
# -- lane VMA行车车道线重建
def lane_reconstruct_cmd(workflow_tools_base_path, clip_id, start_ts, end_ts, mode, output_dir):
    cmd = f'cd {workflow_tools_base_path} &&  export PYTHONPATH=. && /root/miniconda3/envs/vma/bin/python tools/gen_input_file/gen_input_file.py -t lane_reconstruct -cid "{clip_id}" -st {start_ts} -et {end_ts} -td "{output_dir}" -p \'{{"mode":"{mode}","is_park":0,"ldmap_topic":"PCO_MapRouting"}}\''
    return cmd


if __name__ == "__main__":
    workflow_tools_base_path = '/root/workflow_tools_3.0'
    output_dir = '/root/0926_test'
    # slice_key = [clip_id, start_ts, end_ts]
    slice_key = ['17H564030_20250731T162255', 1753950233000, 1753950263000]

    os.makedirs(output_dir, exist_ok=True)
    os.chdir(workflow_tools_base_path)
    os.environ['PYTHONPATH'] = workflow_tools_base_path
    print("pythonpath:", os.environ['PYTHONPATH'])
    cmd = lane_reconstruct_cmd(workflow_tools_base_path, slice_key[0], slice_key[1], slice_key[2], 'lane', output_dir)
    print("Running command:", cmd)
    # os.system(cmd)