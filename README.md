# PyUtau-continued C++ 底层原型

这是一个面向 UTAU 工作流的 **最底层 C++ 实现骨架**，设计思路参考 OpenUTAU 的分层方式：

1. **数据解析层**：读取 `ust` 与 `oto.ini`。
2. **音库索引层**：根据 alias/歌词映射发音配置。
3. **合成引擎层**：将音符序列渲染为 PCM 缓冲区（当前为占位实现，后续可接入真正 resampler/wavtool）。
4. **导出层**：写出标准 WAV 文件。

> 当前版本聚焦“跑通底层链路”，用于后续替换为真实 UTAU 重采样器。

## 构建

```bash
cmake -S . -B build
cmake --build build
```

## 运行

```bash
./build/utau_cli <song.ust> <oto.ini> <output.wav>
```

## 后续建议（贴近 OpenUTAU）

- 增加 `IResampler` 接口，支持外部引擎（如 worldline-r、moresampler）。
- 增加 `IWavtool` 接口，做 note 拼接、交叉淡化。
- 支持 pitch curve（PBS/PBY/PIT）与动态（VEL、DYN）。
- 处理 Shift-JIS 编码与多音节 CVVC/VCCV 音素映射。
