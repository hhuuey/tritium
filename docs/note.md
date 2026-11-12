/mnt/data/huey/conda/bin/conda init
conda init --reverse bash


modelscope download --dataset modelscope/gsm8k --local_dir /mnt/cfs-baidu/public/haihui.huang/model_pt/modelscope/gsm8k
modelscope download --model Qwen/Qwen3.5-9B --local_dir /mnt/data/huey/model_path/Qwen/Qwen3.5-9B
modelscope download --model Qwen/Qwen3-VL-8B-Instruct --local_dir /mnt/data/huey/model_path/Qwen/Qwen3-VL-8B-Instruct


rsync -Pa "$CASE_SOURCE/$CASE_ID" "$HOST_DEBUG_DIR/jk-mini-dataset-raw/