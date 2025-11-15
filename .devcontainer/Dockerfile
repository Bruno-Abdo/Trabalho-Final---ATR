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
    # Pygame dependencies
    libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-mixer-2.0-0 libsdl2-ttf-2.0-0 \
    libportmidi0 \
    # General graphics/font libs
    libgl1 libfontconfig1 libfreetype6 libglib2.0-0 fonts-dejavu-core \
    # C/C++ toolchain + build tools
    build-essential cmake ninja-build gdb lldb cppcheck ccache \
    libssl-dev liblz4-dev libre2-dev zlib1g-dev libcurl4-openssl-dev \
    # Boost.Asio Library dependencies (Corrected name)
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*
