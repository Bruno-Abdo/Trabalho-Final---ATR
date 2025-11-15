# syntax=docker/dockerfile:1

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PYTHONUNBUFFERED=1
ENV PYTHONDONTWRITEBYTECODE=1
ENV PIP_DISABLE_PIP_VERSION_CHECK=1
ENV PIP_NO_CACHE_DIR=1
ENV PATH="/home/ubuntu/.local/bin:${PATH}"

RUN apt-get update && apt-get install -y --no-install-recommends \
    sudo ca-certificates tzdata curl lsb-release wget git openssh-client pkg-config unzip zip locales

# --- Base OS + common tools ---
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Python 3.12 + pip/venv
    python3 python3-pip python3-venv python3-dev \
    # PyQt6 dependencies
    libx11-6 libxext6 libxcb1 libxkbcommon0 libxkbcommon-x11-0 libx11-xcb1 \
    libgl1 libegl1 libfontconfig1 libfreetype6 libglib2.0-0 \
    libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-shm0 \
    libxcb-randr0 libxcb-xfixes0 libxcb-xinerama0 \
    fonts-dejavu-core xauth \
    # C/C++ toolchain + build tools
    build-essential cmake ninja-build gdb lldb cppcheck ccache \
    libssl-dev liblz4-dev libre2-dev zlib1g-dev libcurl4-openssl-dev \
    # Boost.Asio Library dependencies
    && rm -rf /var/lib/apt/lists/*
