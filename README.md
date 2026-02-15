# PyUtau-contiuned (C++ minimal core bootstrap)

这是一个**可运行的最基础 C++ 版本 UTAU 底层原型**，用于把最小 UST 文件离线渲染为 WAV。

> 当前版本目标：先打通底层链路 `UST -> 音符解析 -> 简化重采样(正弦合成占位) -> WAV 输出`。

## Desktop Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Desktop Run

```bash
./build/utau_core_render --ust examples/minimal.ust --out out.wav --gain 0.2
```

查看帮助：

```bash
./build/utau_core_render --help
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Branch Notes

- `main` / `work`: desktop minimal core only (no Android-specific build files).
- `android`: keeps the Android NDK/JNI adaptation version.

## 当前支持

- UST 基础字段：
  - `[#SETTING]` 下 `Tempo`
  - note section 下 `Length` / `NoteNum` / `Lyric`
- 渲染：
  - 基于音高的简化单声道合成（占位实现）
  - 支持 `Lyric=R/rest/pau/sil` 作为静音段
  - 输出 16-bit PCM WAV
- 命令行：
  - 必填：`--ust`、`--out`
  - 可选：`--sr`（自动限制在 8000~192000）、`--gain`（范围 `(0.0, 4.0]`）、`--no-normalize`（关闭自动峰值归一化）

## 后续建议

- 接入真实 voicebank 与 `oto.ini` 索引
- 替换占位重采样器为兼容 UTAU 的实现
- 引入并行任务调度、缓存与更完整的自动化测试
