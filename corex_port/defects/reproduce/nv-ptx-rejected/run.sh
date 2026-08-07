#!/usr/bin/env bash
# 若上一进程残留导致 cuCtxCreate 挂起,先 'ixsmi -r' 复位再跑。
set -e
bash build.sh
./reproduce
