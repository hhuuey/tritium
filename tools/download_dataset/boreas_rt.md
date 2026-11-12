# 安装

参考: https://www.boreas.utias.utoronto.ca/#/download

brew install awscli

sudo apt install awscli

cd /Users/huey/work/tritium/.dataset/Boreas-RT
root=.
aws s3 sync s3://boreas/boreas-2024-12-10-12-07 $root/boreas-2024-12-10-12-07 --exclude "*"  --include "lidar/*" --include "camera/*" --include "imu/*" --include "applanix/*" --include "calib/*" --no-sign-request