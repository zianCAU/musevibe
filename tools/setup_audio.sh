#!/bin/bash
# MUSE 电脑音源一键安装
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SF2_DIR="$SCRIPT_DIR/soundfonts"
SF2_PATH="$SF2_DIR/GeneralUser_GS.sf2"

echo "=== MUSE 电脑音源安装 ==="

# 1. fluidsynth
if ! command -v fluidsynth &>/dev/null; then
    echo "[1/3] 安装 fluidsynth..."
    brew install fluidsynth
else
    echo "[1/3] fluidsynth 已安装 ✓"
fi

# 2. pyfluidsynth
echo "[2/3] 安装 pyfluidsynth..."
"$SCRIPT_DIR/.venv/bin/pip" install pyfluidsynth --quiet

# 3. soundfont
mkdir -p "$SF2_DIR"
if [ ! -f "$SF2_PATH" ]; then
    echo "[3/3] 下载 GeneralUser GS soundfont (~30MB)..."
    curl -L "https://github.com/mrbumpy409/GeneralUser-GS/releases/download/2.0.1/GeneralUser_GS_2.0.1.zip" \
         -o "$SF2_DIR/gu.zip"
    cd "$SF2_DIR"
    unzip -o gu.zip "*.sf2" -d .
    mv *.sf2 GeneralUser_GS.sf2 2>/dev/null || true
    rm -f gu.zip
    echo "  已保存到 $SF2_PATH"
else
    echo "[3/3] soundfont 已存在 ✓"
fi

echo ""
echo "安装完成！启动命令："
echo "  python3 tools/muse_audio_server.py --serial /dev/cu.usbmodem1101 --soundfont tools/soundfonts/GeneralUser_GS.sf2"
