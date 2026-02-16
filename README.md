# PyUtau Continued (C++ 基础原型)

这是一个“从 0 到 1”的 C++ UTAU 底层原型，目标是先搭起可扩展架构：

- `core/Project`：工程数据结构（音符、轨道、速度），支持多音轨参数（轨道增益、静音、独立声库 ID）。
- `core/UstParser`：读取基础 UST 字段。
- `core/Voicebank`：读取 `oto.ini`。
- `core/Resampler`：多线程占位渲染实现（按音符分发到多个 worker，最后混音），后续可替换为真正的 UTAU/OpenUTAU 兼容渲染链。
- `audio/AudioEngine`：导出 PCM 到 WAV。
- `gui/MainWindow + PianoRollWidget`：Qt Widgets 图形界面，布局参考 Synthesizer V Studio 的轨道区 + 钢琴卷帘 + Inspector 三栏结构。

## 构建

```bash
cmake -S . -B build
cmake --build build
```

> 依赖：Qt6 Widgets。

## 性能优化（当前版本）

- 对渲染结果预估总帧数并一次性分配，避免频繁 `push_back` 扩容。
- 使用 `std::thread::hardware_concurrency()` 自动确定 worker 数（可通过 `maxThreads` 参数限制）。
- 每个 worker 写入独立缓冲区，减少锁竞争；最后执行统一混音与限幅。
- 导出后在状态栏显示渲染耗时与线程数，便于快速观察优化效果。

## 界面与工作流完善（当前版本）

- **设置（Settings）**：支持设置渲染线程数、主输出增益和采样率。
- **声库管理（Voicebank Manager）**：支持添加/移除/查看声库，并将声库分配给当前选中音轨。
- **词典（Dictionary）**：支持添加、更新、删除、查看、清空歌词映射；导出时自动将音符歌词按词典转换后再渲染。
- **多音轨混音导出**：导出时对所有未静音音轨进行离线渲染并混音输出，支持每轨增益与全局主增益。

## 说明

目前重点是“最底层骨架 + 可运行 GUI”，并未实现完整 UTAU 重采样、音素拼接与实时播放。
