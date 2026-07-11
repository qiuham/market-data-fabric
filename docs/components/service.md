# service 服务运行时模块

`service` 是 gateway 的单进程运行时。

职责：

- 根据配置选择 `raw_only`、`normalized_only` 或 `dual` 数据模式。

- 加载配置。
- 构建 provider -> mapper -> publisher pipeline。
- 管理 provider 生命周期。
- 组装 recorder、publisher、metrics、health。
- 维护 feed / mapper 的轻量运行统计。
- 处理启动、停止、重连等服务级逻辑。
- 根据 provider 的序号检查结果控制发布、恢复和健康状态。

## Channel 序号闸门

序号连续性的协议规则属于 provider mapper 模块。例如通联 mapper 内使用
`TonglianSequenceGate`，Crypto mapper 可以使用 update-id 区间或 previous-id 规则。
`service` 不假设所有市场都是严格 `+1`；它只根据 provider 返回的结果控制
publisher，并编排 checkpoint/replay。

默认状态不接受第一条消息。调用方必须先通过以下任一方式建立明确边界：

- 从日初开始时为具体 Channel 建立 sequence 0 基线；
- 从 checkpoint/replay 恢复完成后为该 Channel 建立 `last_sequence` 基线。

发现 gap 或倒退后，service 必须停止该序列域的发布、标记下游订单簿失效并启动
恢复。仅记录日志不能恢复可信状态。完整重放并重建所有下游订单簿后，才能通知
provider gate 完成恢复。重复消息不会破坏可信状态，但也不会再次应用。

沪市状态消息也占用 `BizIndex`，因此序号闸门必须位于 `Type` 和 `SecurityID`
过滤之前。深市同理，必须在完整 `ChannelNo + ApplSeqNum` 流上检查。

它不应该包含具体供应商字段转换逻辑。
