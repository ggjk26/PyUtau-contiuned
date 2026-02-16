# Mini UTAU Core (C++)

这是一个“最低可实现”的 UTAU 基础内核示例，目标是先打通**无图形界面**核心链路，并逐步靠近 OpenUTAU 的后端流程。

当前已实现：

- 读取 UST（Tempo、Lyric、Length、NoteNum、Velocity）
- 读取 oto.ini（alias、offset、consonant、cutoff、preutter、overlap）
- 基础合成引擎（非采样拼接版）：
  - 元音相关谐波音色（a/i/u/e/o）
  - 简易辅音噪声段（基于 preutter）
  - 起音速度（velocity）影响 attack
  - 连音滑音（前音到当前音）
  - 尾部颤音（vibrato）
  - cutoff/overlap 的基础处理
- 输出 16-bit PCM WAV

> 说明：当前仍是“可运行骨架”，尚未实现 OpenUTAU 级别的真正重采样器 + wavtool 拼接、音素词典、完整 pitch curve 等。

## 构建

```bash
cmake -S . -B build
cmake --build build -j
```

## 使用

```bash
./build/mini_utau --ust demo.ust --out demo.wav
./build/mini_utau --ust demo.ust --oto voice/oto.ini --out demo.wav
```

## 最小 UST 示例

```ust
[#VERSION]
UST Version1.2
[#SETTING]
Tempo=120
[#0000]
Length=480
Lyric=a
NoteNum=60
Velocity=120
[#0001]
Length=480
Lyric=R
NoteNum=60
[#0002]
Length=480
Lyric=i
NoteNum=64
Velocity=90
[#TRACKEND]
```

## 后续建议（向 OpenUTAU 靠近）

1. 对接真实 resampler + wavtool，替换当前加法合成。
2. 实现 CV/CVVC/VCCV 音素切分与 alias 自动匹配。
3. 加入 PBS/PBW/PBY/PBM 等 pitch 曲线解析与渲染。
4. 增加包络点、flags、voice color 与多线程批量渲染。
5. 最后接入图形界面（Qt/ImGui/Web 均可）。
