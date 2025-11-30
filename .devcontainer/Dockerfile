# syntax=docker/dockerfile:1

FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive
ENV PYTHONUNBUFFERED=1
ENV PYTHONDONTWRITEBYTECODE=1
ENV PIP_DISABLE_PIP_VERSION_CHECK=1
ENV PIP_NO_CACHE_DIR=1
# Ensure the path for local pip packages is included
ENV PATH="/home/ubuntu/.local/bin:${PATH}" 

# --- Stage 1: Install Core Utilities and Bash ---
RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    sudo ca-certificates tzdata curl lsb-release wget git openssh-client pkg-config unzip zip locales

# --- Stage 2: Create the 'ubuntu' user ---
ARG USERNAME=ubuntu
ARG USER_UID=1000
ARG USER_GID=$USER_UID
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd -s /bin/bash --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(ALL\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# --- Stage 3: Install Remaining Dependencies ---
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Python 3.12 + pip/venv
    python3 python3-pip python3-venv python3-dev \
    # C/C++ toolchain + build tools
    build-essential cmake ninja-build gdb lldb cppcheck ccache \
    # Pygame dependencies
    libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-mixer-2.0-0 libsdl2-ttf-2.0-0 \
    libportmidi0 \
    # General graphics/font libs
    libgl1 libfontconfig1 libfreetype6 libglib2.0-0 fonts-dejavu-core \
    libssl-dev liblz4-dev libre2-dev zlib1g-dev libcurl4-openssl-dev \
    # General PyQt6 dependencies
    libegl1 libgl1 libxkbcommon-x11-0 libxcb-cursor0 libdbus-1-3 xcb-cursor0 \
    # C++ Libraries
    libboost-all-dev \
    nlohmann-json3-dev \
    # Clean up apt lists to keep the image small
    && rm -rf /var/lib/apt/lists/*
