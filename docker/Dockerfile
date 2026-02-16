# FeralRF Build Environment
# Ubuntu 22.04 with TI SDK, ARM GCC, Pico SDK

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install base dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    wget \
    curl \
    unzip \
    python3 \
    python3-pip \
    python3-venv \
    libusb-1.0-0-dev \
    udev \
    && rm -rf /var/lib/apt/lists/*

# Install ARM GCC for CC1352 and RP2040
RUN apt-get update && apt-get install -y \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    && rm -rf /var/lib/apt/lists/*

# Set up workspace
WORKDIR /workspace

# TI SDK path (mount or copy SDK here)
ENV TI_SDK_PATH=/workspace/firmware/sdk/simplelink_cc13xx_cc26xx_sdk_7_10_01_24

# Pico SDK
ENV PICO_SDK_PATH=/workspace/firmware/sdk/pico-sdk

# Download Pico SDK
RUN git clone --depth 1 --branch 2.0.0 https://github.com/raspberrypi/pico-sdk.git ${PICO_SDK_PATH} && \
    cd ${PICO_SDK_PATH} && \
    git submodule update --init

# Python environment
RUN python3 -m pip install --upgrade pip && \
    pip install \
    pytest \
    pytest-asyncio \
    pytest-cov \
    pyserial \
    pyserial-asyncio \
    build \
    twine

# Set up environment
ENV CC=arm-none-eabi-gcc
ENV CXX=arm-none-eabi-g++

# Default command
CMD ["/bin/bash"]
