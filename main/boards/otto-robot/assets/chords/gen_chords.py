#!/usr/bin/env python3
"""
生成占位和弦 PCM 文件（16kHz / 16bit / mono）
后续替换为真实吉他采样。
"""
import struct, math, os

SAMPLE_RATE = 16000
NOTE_DURATION = 0.4   # 分解弦每音时长（秒）
DURATION_STR = 0.6    # 扫弦总时长（秒）
AMPLITUDE = 16000

CHORDS = {
    'C':  [262, 330, 392],
    'Am': [220, 262, 330],
    'F':  [175, 220, 262],
    'G':  [196, 247, 294],
}

def sine_wave(freq, duration, sample_rate, amplitude, decay=0.8):
    n = int(sample_rate * duration)
    samples = []
    for i in range(n):
        t = i / sample_rate
        env = math.exp(-decay * t)
        s = int(amplitude * env * math.sin(2 * math.pi * freq * t))
        s = max(-32768, min(32767, s))
        samples.append(s)
    return samples

def mix(a, b):
    return [max(-32768, min(32767, x + y)) for x, y in zip(a, b)]

script_dir = os.path.dirname(os.path.abspath(__file__))

for name, freqs in CHORDS.items():
    # 分解弦：三个音依次播放
    arp_samples = []
    for freq in freqs:
        arp_samples += sine_wave(freq, NOTE_DURATION, SAMPLE_RATE, AMPLITUDE)
    fn = os.path.join(script_dir, f'chord_{name}_arp.raw')
    with open(fn, 'wb') as f:
        for s in arp_samples:
            f.write(struct.pack('<h', s))
    print(f'Written {fn} ({len(arp_samples)} samples)')

    # 扫弦：三个音叠加
    total = int(SAMPLE_RATE * DURATION_STR)
    strum = [0] * total
    for freq in freqs:
        wave = sine_wave(freq, DURATION_STR, SAMPLE_RATE, AMPLITUDE // len(freqs))
        strum = mix(strum, wave)
    fn = os.path.join(script_dir, f'chord_{name}_strum.raw')
    with open(fn, 'wb') as f:
        for s in strum:
            f.write(struct.pack('<h', s))
    print(f'Written {fn} ({len(strum)} samples)')
