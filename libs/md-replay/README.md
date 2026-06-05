# md-replay 录制回放模块

`md-replay` 负责行情录制和回放。

建议同时支持两类数据：

- raw feed：外部原始消息或 SDK 原始字段，用于排查供应商问题。
- normalized events：标准化后的内部事件，用于回测、调试和策略开发。

当前已经落地 raw 文件格式和基础读写器：

- `include/md/replay/raw_file.hpp`：固定二进制文件头和 frame 头。
- `include/md/replay/raw_writer.hpp`：raw 写入器，默认 100 MiB 上限。
- `include/md/replay/raw_reader.hpp`：raw 读取器，用于 smoke test、回放和后续 mapper 开发。

回放应该尽量和 live 消费使用相同的接口，减少策略侧差异。
