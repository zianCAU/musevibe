#!/usr/bin/env python3
"""
MUSE 远程音频服务端
===================
支持两种输入模式：
  UDP 模式（默认）: python3 tools/muse_audio_server.py
  串口模式:         python3 tools/muse_audio_server.py --serial /dev/cu.usbmodem1101

音频后端：
  --music-dir  指定包含 C.wav / Am.wav / F.wav / G.wav / 鼓.wav 的目录
               默认目录见 DEFAULT_MUSIC_DIR
"""

import argparse
import json
import socket
import threading
import time
from pathlib import Path

import numpy as np
import sounddevice as sd
import soundfile as sf

# ── 默认音乐目录（修改为你的 WAV 文件所在目录）──────────────────────────────────
# 目录内需包含：C.wav / Am.wav / F.wav / G.wav / 鼓.wav
DEFAULT_MUSIC_DIR = "tools/music"

SR = 44100   # 统一输出采样率

# ── 和弦索引映射 ───────────────────────────────────────────────────────────────
CHORD_NAMES = {0: "C", 1: "Am", 2: "F", 3: "G"}

# ── 加载后的 WAV 数据（mono float32, 已重采样到 SR）────────────────────────────
_chord_wav: dict[int, np.ndarray] = {}   # chord_index → audio
_drum_wav:  np.ndarray | None = None


def _load_mono(path: Path) -> np.ndarray:
    """加载 WAV，转单声道，重采样到 SR，归一化到 85%。"""
    data, sr = sf.read(str(path), dtype="float32", always_2d=False)
    if data.ndim == 2:
        data = data.mean(axis=1)
    if sr != SR:
        ratio = SR / sr
        n_out = int(len(data) * ratio)
        x_old = np.linspace(0, 1, len(data))
        x_new = np.linspace(0, 1, n_out)
        data = np.interp(x_new, x_old, data).astype(np.float32)
    peak = np.max(np.abs(data))
    if peak > 1e-6:
        data *= 0.85 / peak
    return data


def load_music_dir(music_dir: str) -> bool:
    global _drum_wav
    d = Path(music_dir)
    ok = True
    for idx, name in CHORD_NAMES.items():
        p = d / f"{name}.wav"
        if p.exists():
            _chord_wav[idx] = _load_mono(p)
            print(f"  [chord] {p.name}  {len(_chord_wav[idx])/SR:.2f}s")
        else:
            print(f"  [warn]  找不到 {p}，将跳过此和弦")
            ok = False

    drum_p = d / "鼓.wav"
    if drum_p.exists():
        _drum_wav = _load_mono(drum_p)
        print(f"  [drum]  {drum_p.name}  {len(_drum_wav)/SR:.2f}s  (循环)")
    else:
        print(f"  [warn]  找不到 {drum_p}，无鼓声")
    return ok


# ── 混音引擎 ──────────────────────────────────────────────────────────────────
_mix_lock = threading.Lock()

# 一次性音符（和弦）
_active_notes: list[dict] = []   # [{data, pos}]

# 鼓循环轨道
_drum_pos: int = 0
_drum_enabled: bool = True


def _audio_callback(outdata, frames, time_info, status):
    mix = np.zeros(frames, dtype=np.float32)

    with _mix_lock:
        # 1. 鼓循环
        global _drum_pos
        if _drum_wav is not None and _drum_enabled:
            drum_len = len(_drum_wav)
            written = 0
            while written < frames:
                take = min(frames - written, drum_len - _drum_pos)
                mix[written:written + take] += _drum_wav[_drum_pos:_drum_pos + take] * 2.6
                _drum_pos = (_drum_pos + take) % drum_len
                written += take

        # 2. 和弦一次性音符
        alive = []
        for note in _active_notes:
            rem = len(note["data"]) - note["pos"]
            take = min(frames, rem)
            mix[:take] += note["data"][note["pos"]:note["pos"] + take]
            note["pos"] += take
            if note["pos"] < len(note["data"]):
                alive.append(note)
        _active_notes[:] = alive

    # 软限幅
    peak = np.max(np.abs(mix))
    if peak > 1.0:
        mix /= peak

    outdata[:, 0] = mix


def play_chord_wav(chord_idx: int):
    """立即叠加播放对应和弦 WAV（不打断鼓声）。"""
    data = _chord_wav.get(chord_idx)
    if data is None:
        print(f"  [warn] 无和弦 WAV: {CHORD_NAMES.get(chord_idx)}")
        return
    with _mix_lock:
        # 移除上一个同类和弦（避免堆叠过多），保留鼓
        _active_notes.clear()
        _active_notes.append({"data": data.copy(), "pos": 0})


# ── 数据包处理 ────────────────────────────────────────────────────────────────
def handle_packet(raw: bytes):
    try:
        msg = json.loads(raw)
        c = int(msg.get("c", 0))
        s = int(msg.get("s", 0))
        b = int(msg.get("b", 80))
    except Exception as e:
        print(f"[warn] 解包失败: {e}")
        return

    name = CHORD_NAMES.get(c, f"#{c}")
    style = "strum" if s == 1 else "arp"
    print(f"  ♪  {name} / {style}  BPM={b}")

    threading.Thread(target=play_chord_wav, args=(c,), daemon=True).start()


# ── 主程序 ────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="MUSE 远程音频服务端（WAV模式）")
    parser.add_argument("--port",       type=int, default=7777,           help="UDP 监听端口")
    parser.add_argument("--serial",     type=str, default=None,           help="串口，如 /dev/cu.usbmodem1101")
    parser.add_argument("--baud",       type=int, default=115200,         help="串口波特率")
    parser.add_argument("--music-dir",  type=str, default=DEFAULT_MUSIC_DIR,
                        help="包含 C/Am/F/G/鼓.wav 的目录")
    parser.add_argument("--device",     type=int, default=None,           help="音频输出设备 index")
    parser.add_argument("--list-devices", action="store_true",            help="列出音频设备后退出")
    args = parser.parse_args()

    if args.list_devices:
        print(sd.query_devices())
        return

    print("=" * 55)
    print("MUSE 远程音频服务端  (WAV + 鼓循环模式)")
    print(f"  模式:      {'串口 ' + args.serial if args.serial else 'UDP :' + str(args.port)}")
    print(f"  音乐目录:  {args.music_dir}")
    print(f"  采样率:    {SR} Hz")
    print("=" * 55)

    print("\n加载音频文件...")
    load_music_dir(args.music_dir)
    print()

    stream = sd.OutputStream(
        samplerate=SR, channels=1, dtype="float32",
        latency="low", device=args.device,
        callback=_audio_callback, blocksize=512,
    )
    stream.start()
    print("音频流已开启，鼓声循环中...\n")

    try:
        if args.serial:
            _run_serial(args.serial, args.baud)
        else:
            _run_udp(args.port)
    except KeyboardInterrupt:
        print("\n退出")
    finally:
        stream.stop()


def _run_udp(port: int):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", port))
    print(f"监听 UDP :{port} ... （Ctrl+C 退出）\n")
    try:
        while True:
            data, addr = sock.recvfrom(256)
            print(f"← {addr[0]}:{addr[1]}  {data.decode(errors='replace')}")
            handle_packet(data)
    finally:
        sock.close()


def _run_serial(port: str, baud: int):
    try:
        import serial
    except ImportError:
        print("串口模式需要 pyserial：pip install pyserial")
        return
    print(f"监听串口 {port} @{baud} ... （Ctrl+C 退出）\n")
    while True:
        try:
            with serial.Serial(port, baud, timeout=1) as ser:
                while True:
                    line = ser.readline().decode(errors="replace").strip()
                    if not line.startswith("MUSE_CHORD:"):
                        continue
                    parts = line.split(":")
                    if len(parts) != 5:
                        continue
                    try:
                        c, s, b, p = int(parts[1]), int(parts[2]), int(parts[3]), int(parts[4])
                    except ValueError:
                        continue
                    raw = json.dumps({"c": c, "s": s, "b": b, "p": p}).encode()
                    print(f"← serial  {raw.decode()}")
                    handle_packet(raw)
        except Exception as e:
            print(f"[串口断开] {e}，2秒后重连...")
            time.sleep(2)


if __name__ == "__main__":
    main()
