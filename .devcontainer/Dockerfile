# 选用稳定的 Ubuntu 22.04 作为基础镜像
FROM ubuntu:22.04

# 设置非交互模式，防止安装软件时卡在提示按键
ENV DEBIAN_FRONTEND=noninteractive

# 更新包列表并安装 C++ 核心开发工具链
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gdb \
    git \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /workspace