# huey personal repo

## 多环境开发入口

公共 Dockerfile、依赖准备和项目操作脚本统一位于 [script/env](script/env/README.md)，
按项目和环境选择 ROS 1、ROS 2、无 ROS C++、Python CPU 或 Linux CUDA 开发环境。
当前支持 `fast-livo2` 和 `fast-calib2`。第三方源码、数据集和结果分别位于不入库的
`.deps/`、`.dataset/`、`.result/`，容器内继续使用 `/deps`、`/data`、`/results`。
