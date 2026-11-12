FROM ros:noetic-ros-base-focal AS ros1-noetic

SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates cmake git jq patch util-linux \
    libboost-all-dev libeigen3-dev libgeographic-dev libgoogle-glog-dev \
    libopencv-dev libpcl-dev python3-matplotlib \
    ros-noetic-cmake-modules ros-noetic-cv-bridge ros-noetic-eigen-conversions \
    ros-noetic-foxglove-bridge ros-noetic-image-geometry ros-noetic-image-transport-plugins \
    ros-noetic-pcl-ros ros-noetic-rosbag && \
    rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends libyaml-cpp-dev && rm -rf /var/lib/apt/lists/*
COPY entrypoint.sh /usr/local/bin/tritium-ros-entrypoint
RUN chmod +x /usr/local/bin/tritium-ros-entrypoint
WORKDIR /catkin_ws
ENTRYPOINT ["/usr/local/bin/tritium-ros-entrypoint"]
CMD ["sleep", "infinity"]

FROM ros:jazzy-ros-base-noble AS ros2-jazzy
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git jq patch util-linux python3-colcon-common-extensions \
    libeigen3-dev libopencv-dev libpcl-dev libgeographiclib-dev libgoogle-glog-dev \
    ros-jazzy-cv-bridge ros-jazzy-image-transport-plugins ros-jazzy-pcl-conversions \
    ros-jazzy-foxglove-bridge ros-jazzy-rosbag2-storage-mcap && \
    rm -rf /var/lib/apt/lists/*
RUN apt-get update && apt-get install -y --no-install-recommends libyaml-cpp-dev && rm -rf /var/lib/apt/lists/*
COPY entrypoint.sh /usr/local/bin/tritium-ros-entrypoint
RUN chmod +x /usr/local/bin/tritium-ros-entrypoint
WORKDIR /catkin_ws
ENTRYPOINT ["/usr/local/bin/tritium-ros-entrypoint"]
CMD ["sleep", "infinity"]

FROM ubuntu:24.04 AS cpp-cpu
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates cmake ninja-build git jq patch util-linux \
    libeigen3-dev libopencv-dev libpcl-dev libyaml-cpp-dev libgoogle-glog-dev && \
    rm -rf /var/lib/apt/lists/*
RUN apt-get update && apt-get install -y --no-install-recommends libgeographiclib-dev && rm -rf /var/lib/apt/lists/*
COPY entrypoint.sh /usr/local/bin/tritium-ros-entrypoint
RUN chmod +x /usr/local/bin/tritium-ros-entrypoint
WORKDIR /catkin_ws
ENTRYPOINT ["/usr/local/bin/tritium-ros-entrypoint"]
CMD ["sleep", "infinity"]

FROM python:3.12-slim-bookworm AS python-cpu
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates git jq util-linux && rm -rf /var/lib/apt/lists/*
ENV PYTHONNOUSERSITE=1 PIP_REQUIRE_VIRTUALENV=true
WORKDIR /catkin_ws
CMD ["sleep", "infinity"]

FROM nvidia/cuda:12.8.1-cudnn-devel-ubuntu24.04 AS python-cuda
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates git jq util-linux python3 python3-venv python3-pip && \
    rm -rf /var/lib/apt/lists/*
ENV PYTHONNOUSERSITE=1 PIP_REQUIRE_VIRTUALENV=true
WORKDIR /catkin_ws
CMD ["sleep", "infinity"]
