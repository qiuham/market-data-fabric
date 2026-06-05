# md-replay 录制回放模块

`md-replay` 负责行情录制和回放。

建议同时支持两类数据：

- provider message：外部原始消息或 SDK 原始字段，用于排查供应商问题。
- normalized events：标准化后的内部事件，用于回测、调试和策略开发。

当前已经落地统一 message log：

- `include/md/replay/message_log.hpp`：固定二进制文件头、frame 头、writer 和 reader。
- 默认 100 MiB 上限，避免开发环境无限写盘。
- 文件外层使用 `MessageEnvelope`，payload 可以是原始报文，也可以是后续序列化后的标准事件。

回放应该尽量和 live 消费使用相同的接口，减少策略侧差异。
