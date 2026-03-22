# PyUtau Continued (C++ 基础原型)

这是一个“从 0 到 1”的 C++ UTAU 底层原型，目标是先搭起可扩展架构：

- `core/Project`：工程数据结构（音符、轨道、速度）。
- `core/UstParser`：读取基础 UST 字段。
- `core/Voicebank`：读取 `oto.ini`。
- `core/Resampler`：当前为占位实现（正弦波），后续可替换为真正的 UTAU/OpenUTAU 兼容渲染链。
- `audio/AudioEngine`：导出 PCM 到 WAV。
- `gui/MainWindow + PianoRollWidget`：Qt Widgets 图形界面，布局参考 Synthesizer V Studio 的轨道区 + 钢琴卷帘 + Inspector 三栏结构。

## 构建

```bash
cmake -S . -B build
cmake --build build
```

> 依赖：Qt6 Widgets。

## 说明

目前重点是“最底层骨架 + 可运行 GUI”，并未实现完整 UTAU 重采样、音素拼接与实时播放。
