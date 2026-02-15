# C++ 版本 UTAU 最底层实现计划

## 目标范围（仅底层，不含完整 GUI）
- 构建一个可复用的核心引擎：
  - 音频 I/O（读取/写出 WAV、基础流式播放接口）
  - 采样数据库访问（音源文件、元数据、索引）
  - UST 工程解析（音符序列、歌词、时值、音高参数）
  - 重采样与拼接的调度框架（先提供最小可运行实现）
  - 渲染管线 API（离线渲染到 WAV）
- 先实现“可用的最小链路”：`UST -> 音源检索 -> 重采样调用 -> 拼接输出`。

## 技术与工程基线
- 语言标准：C++20
- 构建系统：CMake + Ninja
- 核心依赖建议：
  - `libsndfile`（WAV/PCM 读写）
  - `fmt`（日志与格式化）
  - `catch2`（测试）
  - 可选：`Eigen`（后续 DSP）
- 目录结构：
  - `src/core/audio`：音频缓冲、采样率转换接口、WAV I/O
  - `src/core/ust`：UST 解析器、模型
  - `src/core/voicebank`：音源索引、别名查询、缓存
  - `src/core/render`：渲染图构建、任务调度、输出合成
  - `src/core/plugin`：外部重采样器/工具适配层
  - `tests/`：单元测试与最小端到端测试

## 分阶段路线图

### Phase 0：需求固化与验收基准（1 周）
- 明确首个兼容目标：
  - 支持 UST 基本字段（Tempo、Length、Lyric、NoteNum）
  - 支持单音源目录中最基本别名映射
- 定义“最小可用验收”：
  - 输入一份简单 UST + 音源目录，输出长度正确、无崩溃的 WAV。

### Phase 1：音频底座（1~2 周）
- 实现 `AudioBuffer`：
  - 统一采样格式（float32, interleaved/non-interleaved）
  - 采样率、声道、帧数管理
- 实现 WAV 读写：
  - `WavReader`, `WavWriter`
  - 边界条件：空文件、采样率不一致、位深兼容
- 测试：
  - 读写回环一致性（误差阈值）

### Phase 2：UST 解析与数据模型（1~2 周）
- 实现 `UstParser`：
  - 解析 section、key-value
  - 转换到 `Project`, `NoteEvent` 模型
- 处理最小必要参数：
  - 节奏、音符时长、歌词、音高编号
- 测试：
  - 正常文件、缺省字段、非法值回退

### Phase 3：音源库与别名检索（1~2 周）
- 实现 `VoicebankIndex`：
  - 扫描 `oto.ini`（最小字段）
  - 建立 alias -> 样本路径 + offset/overlap/preutterance 映射
- 实现缓存层：
  - 小型 LRU（WAV 解码缓存）
- 测试：
  - 别名命中、回退策略（直接歌词查找）

### Phase 4：重采样与拼接最小管线（2~3 周）
- 抽象 `IResampler` 接口：
  - 输入：目标音高、时长、参数
  - 输出：临时 WAV 或内存 PCM
- 先实现一个“内置简化重采样器”（占位）：
  - 变速/变调简化版本（可接受音质一般）
- 拼接器 `WaveConcatenator`：
  - 按 note 时间线拼接
  - 简单淡入淡出/交叉淡化避免爆音
- 测试：
  - 单音符、多音符、极端短音符

### Phase 5：渲染调度与 CLI（1 周）
- 实现 `RenderEngine`：
  - 从 `Project` 生成任务列表
  - 顺序执行（后续可并行）
- 提供 CLI：
  - `utau_core_render --ust xxx.ust --voicebank xxx --out out.wav`
- 端到端测试：
  - 样例 UST 渲染成功且可播放

## 核心接口草案
- `Project UstParser::parse(const std::filesystem::path&)`
- `VoicebankIndex VoicebankIndex::load(const std::filesystem::path&)`
- `RenderResult RenderEngine::render(const Project&, const VoicebankIndex&, const RenderOptions&)`
- `AudioBuffer IResampler::resample(const ResampleRequest&)`

## 质量与性能基线
- 稳定性：
  - 所有解析器必须返回结构化错误（error code + message + location）
- 性能：
  - 目标：100 音符以内工程在开发机离线渲染 < 5s（占位重采样器）
- 可维护性：
  - 模块边界清晰，禁止跨层直接访问实现细节

## 风险与对策
- 风险 1：UTAU 生态格式边角很多（oto/ust 变体）
  - 对策：先做最小兼容子集 + 样本集驱动迭代
- 风险 2：重采样器音质与兼容性
  - 对策：先保证接口稳定，再替换具体实现
- 风险 3：跨平台音频库行为差异
  - 对策：CI 覆盖 Windows/Linux，统一浮点内部格式

## 里程碑交付物
- M1：可解析 UST 并打印 note 列表
- M2：可加载音源并命中 alias
- M3：可离线渲染最小 demo UST 为 WAV
- M4：提供可复用 C++ core 库 + CLI 工具

## 下一步（可直接开工）
1. 初始化 CMake 工程与模块目录。
2. 先写 `AudioBuffer` + `WavReader/Writer` + 测试。
3. 并行编写 `UstParser` 和 `VoicebankIndex` 的最小实现。
4. 接上 `RenderEngine` 顺序管线，打通首条端到端流程。
