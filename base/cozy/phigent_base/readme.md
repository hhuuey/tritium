loguru
pip3 install phi-dataset-sdk==1.6.8 -i http://pypiserver.phigent.net/simple --trusted-host pypiserver.phigent.net
pip3 install phi-base-sdk -i http://pypiserver.phigent.net/simple --trusted-host pypiserver.phigent.net
pip3 install phi_pipeline_sdk -i http://pypiserver.phigent.net/simple --trusted-host pypiserver.phigent.net


export PYTHONPATH=. && python3 tools/gen_input_file/gen_input_file.py -t lane_reconstruct -cid "17H974230_20250314T170446" -st 1741943086000 -et 1741943146000
    -td "/mnt/cfs-baidu/public/haihui.huang/tmp" -p '{"mode":"lane","is_park":0,"ldmap_topic":"Topic_PCO_MapRouting"}'