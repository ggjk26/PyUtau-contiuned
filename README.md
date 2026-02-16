# Mini UTAU Core (C++)

这是一个“最低可实现”的 UTAU 基础内核示例，目标是先打通**无图形界面**的核心链路：

- 读取 UST（音符、节奏、音高、歌词）
- 读取 oto.ini（音源映射参数）
- 做最简渲染（当前为正弦波占位合成）
- 输出 16-bit PCM WAV

> 说明：当前版本属于核心架构雏形，用于验证流程，尚未实现 OpenUTAU 级别的真实拼接、重采样器调用、时域/频域处理等功能。

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
[#0001]
Length=480
Lyric=R
NoteNum=60
[#0002]
Length=480
Lyric=i
NoteNum=64
[#TRACKEND]
```

## 后续建议（向 OpenUTAU 靠近）

1. 引入 resampler + wavtool 管线，替换正弦波占位。
2. 实现音素切分、alias 匹配（CV/CVVC/VCCV）与发音词典。
3. 支持包络、旗标（flags）、pitch curve、vibrato。
4. 加入插件系统和批处理渲染。
5. 再接图形界面（Qt/ImGui/Web 前端均可）。
