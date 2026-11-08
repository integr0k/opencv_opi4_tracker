#!/usr/bin/env bash
# Bootstrap dependencies for opencv_opi4_tracker on a COMPLETELY FRESH
# Ubuntu / Debian system (e.g. Orange Pi 4 Pro board image).
#
# What it does:
#   1. installs system packages (build tools, FFmpeg, OpenCV),
#   2. clones the motrack tracker library (https://github.com/clip5/motrack),
#   3. builds it into motrack/build/install — the path the Makefile expects,
#   4. optionally builds this project with `make` (pass --build).
#
# Usage:
#   ./setup.sh             # deps + motrack
#   ./setup.sh --build     # deps + motrack + build the tracker itself
#
# Safe to run repeatedly: every step is skipped if already done.
#
# NOTE on models: *.onnx files are NOT stored in the repo (too big).
# Put yolo11n.onnx next to main.cpp manually after running this script.

set -euo pipefail
cd "$(dirname "$0")"

# --- sudo: board images sometimes log in directly as root --------------------
SUDO=""
if [ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null; then
    SUDO="sudo"
fi

# --- 1. system packages -------------------------------------------------------
# build-essential  g++, make
# pkg-config       Makefile uses `pkg-config --cflags opencv5`-style lookups
# cmake            builds motrack (and CMakeLists.txt of this project)
# git              clones motrack
# ca-certificates  https for git clone
# ffmpeg           OpenCV CAP_FFMPEG backend: decodes the RTSP stream
# libopencv-dev    OpenCV (core/imgproc/videoio/imgcodecs/dnn), built with
#                  FFmpeg support so RTSP input works out of the box.
#                  GUI (highgui) is NOT used: the board is headless —
#                  the annotated picture is streamed to a PC as MJPEG over
#                  HTTP, just open http://<board-ip>:8000 in any browser.
echo "==> apt update"
$SUDO apt update

echo "==> installing system packages (this can take a while on the board)"
$SUDO apt install -y --no-install-recommends \
    build-essential \
    pkg-config \
    cmake \
    git \
    ca-certificates \
    ffmpeg \
    libopencv-dev

# --- 2. motrack (tracking library, linked via -lmotrack) ----------------------
MOTRACK_URL="https://github.com/clip5/motrack.git"

if [ ! -d motrack/.git ]; then
    echo "==> cloning motrack"
    git clone "$MOTRACK_URL" motrack
else
    echo "==> motrack already cloned, skipping"
fi

if [ ! -f motrack/build/install/lib/libmotrack.so ]; then
    echo "==> building motrack"
    cmake -S motrack -B motrack/build -DCMAKE_BUILD_TYPE=Release
    cmake --build motrack/build -j "$(nproc)"
    cmake --install motrack/build
else
    echo "==> motrack already built, skipping"
fi

# --- 3. the project itself (optional) -----------------------------------------
if [ "${1:-}" = "--build" ]; then
    echo "==> building opencv_opi4_tracker"
    make
    echo "==> done. Put yolo11n.onnx here, then run:"
    echo "    ./main 'rtsp://<ip>:8080/h264_opus.sdp'"
fi

echo "==> setup complete"
