# 标准事件日志与回放

`include/marketdata/replay/event_journal.hpp` 按交易所序号顺序保存 trading-core 标准
事件，与通联、TORA、XTP 或其他供应商无关。文件头保存数据源和交易日，记录头保存
事件类型、schema、partition、交易所序号和事件序号，payload 是可平凡复制的标准
事件结构。

生产行情线程不直接写文件。推荐链路：

```text
mapper/market mapper
  -> orderbook
  -> 预分配 SPSC
  -> 独立 journal writer
  -> 异步生命周期索引
```

顺序 journal 是 checkpoint 补放、问题复现、订单生命周期和成交反查的共同事实源。
`trading-evaluation` 仓库中的 `book-validate` 只负责验证和可选导出 journal；订单
与成交查询由同仓库的 `event-trace` 读取 journal 完成，不再反复扫描供应商
Parquet。

`message_log.hpp` 保存 provider 原始 payload，`event_journal.hpp` 保存 normalized
事件；前者用于重新解释原始数据，后者用于确定性重建和业务回溯，两者职责不同。

`AsyncEventJournalQueue` 是预分配的有界 SPSC。入队返回 `Full` 时属于数据完整性
故障，service 必须停止宣称盘口可信，不能静默丢弃。`CheckpointCoordinator` 只在
journal 已持久化且所有必需下游均应用成功后提交序号，恢复严格从下一序号重放。
