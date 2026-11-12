# 项目长期约束

## 回答约定

- 回答使用中文。
- 开头单独一行写“你好，huey!”，结尾单独一行写“世界如此美妙，我们一起继续加油!”。

## 仓库结构与环境

- 仓库根目录：`/Users/huey/work/tritium`（下文简称 `<repo>`）。
- Python 解释器：`/opt/homebrew/Caskroom/miniconda/base/envs/tritium/bin/python`。
- 主要目录：
  - `base/cozy`：Python 基础通用库；`base/ccommon`：C++ 通用头文件。
  - `project/`：各项目，按 `core_calibration`、`core_localization`、`core_mapping`、`core_perception`、
    `kit_dataloader`、`kit_dataprod`、`interview` 分类，新项目放在对应分类下。
  - `script/release/build.py`：项目发布脚本。
  - `.deps/`：第三方源码依赖，不入库。
  - `.dataset/`：按项目分类的数据集，不入库，以只读方式使用。
  - `.result/`：运行结果与调试输出，例如 `.result/fast-livo2`。
  - `docs/`：数学、论文与项目笔记。
- 数据集放在 `<repo>/.dataset/<项目名>`，以只读方式使用。
- 个人学习记录目录：`/Users/huey/tritium/archive/personal`（不在本仓库内）。

## Python 代码规范

- import 顺序：项目内文件 → cozy → 第三方库。
- cozy 按子模块缩写导入后调用，例如 `from cozy import vb`，再写 `vb.some_function()`。
  缩写：`mb` map_base、`lb` loc_base、`mlb` mllm_base、`nb` net_base、`eb` engineer_base、
  `vb` visualization_base、`pb` phigent_base。
- 每行不超过 120 字符；`if` 使用卫语句；禁止 `try/except`。
- 函数每个参数类型唯一，参数不超过 5 个。
- 手写源码单文件尽量不超过 300 行；每个功能模块独立成文件，模块间解耦。
- 注释和文档使用中文；生成物和第三方代码保留工具或上游原文。

## cozy 基础库

- 优先复用 `base/cozy` 已有代码，减少第三方依赖；新增的通用代码放入 cozy。
- 定期重构 cozy，减少代码量。
- 维护 cozy 索引（`base/cozy/README.md`）；改动涉及索引时同步更新，缺失时新建。

## Python 项目实现

- 以 `pipeline.py` 为核心：算法拆成 step1、step2… 各子步骤，`pipeline.py` 内定义 `step1_func`、
  `step2_func`… 作为入口，每个 step 的实现各自放一个 py 文件；较小的步骤可合并到 `mix_step.py`。
- 每个子步骤：
  - 输出一个用于检验的可视化结果文件；
  - 耗时较长时结果可复用：有缓存直接加载，否则运行算法。
- 日志使用 `eb.logger`，在关键位置输出进度，数据量大时尤其要有；关键中间结果用 `eb.summary` 保存。
- 发布：项目包含 `pyproject.toml`，便于通过 `script/release/build.py` 发布为 pip 包；
  所有入口脚本做成命令行，安装后可直接运行。

## C++ / ROS 项目

- 上游开源项目（如 FAST-LIVO2、MINS、cartographer）保持上游代码风格，只做必要改动，并保留 LICENSE。
- ROS 1 项目在 Docker 中构建运行（宿主机为 Apple Silicon Mac），参考
  `project/core_localization`：
  - 镜像同时兼容 amd64 / arm64，去掉 `-march=native` / `-mcpu=native` 等本机指令集选项；
  - 可视化使用容器内 `foxglove_bridge`（`ws://localhost:8765`）+ Mac 端 Foxglove 原生应用，不用 VNC/浏览器桌面；
  - 公共 Dockerfile 和环境、依赖准备、容器启动/停止、编译、算法运行与播包入口统一放在
    `<repo>/script/env/`，用项目名和环境参数选择；算法内部辅助脚本保留在原项目。
  - ROS 1、ROS 2、无 ROS C++ 和 Python 使用独立工具链与缓存；环境可用不代表算法已经迁移。
  - 算法核心不依赖 ROS 消息或节点；消息、参数、TF、可视化由适配层处理，迁移不同时改变算法公式。
  - Git 源码依赖下载到 `<repo>/.deps/`，不入库，可读写挂载；编译产物用独立缓存卷保存。
  - 数据只读挂载，结果写入 `<repo>/.result/<项目名>`，不写回源码目录。
- `build/`、`devel/`、`Log/`、`output/`、`*.bag` 等运行产物不入库。

## 算法与解释

- 优先选择时间复杂度较低的方案，避免暴力求解，并注明用到的数学知识和概念。
- 解释模型：输入、最终输出、中间数据流如何变化。
- 解释代码工程：输入有哪些及格式、输出是什么、如何运行、结果如何保存。
- 解释具体算法：总体思想、涉及知识、总体步骤、代码实现位置、每一步的数学推导与直观理解。
- 数学公式单独成行，保证能正确渲染。
- 每次回答算法或开源代码问题后，把问题和不清晰的知识点（数学概念、公式、引理等）分类整理到个人学习记录目录，
  记下提问时间；后续回答前先查阅这些记录。

## 修改与验证

- 小功能模块的修改不写冒烟测试。

## Git 流程

- 仅在用户明确要求时执行 `git commit`；`git push` 由用户执行。
- 推送到新的 GitHub 仓库：先在 GitHub 建空仓库（不勾选 README/.gitignore/License），
  本地检查并提交，再 `git remote add origin ...`、`git push -u origin main`。
  需要保留子目录历史时，用 `git subtree split --prefix=<子目录>` 拆分后推送。
