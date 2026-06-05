# md-replay 录制回放模块

`md-replay` 负责行情录制和回放。

建议同时支持两类数据：

- raw feed：外部原始消息或 SDK 原始字段，用于排查供应商问题。
- normalized events：标准化后的内部事件，用于回测、调试和策略开发。

回放应该尽量和 live 消费使用相同的接口，减少策略侧差异。
