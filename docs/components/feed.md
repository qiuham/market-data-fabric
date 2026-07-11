# md-core 核心库

`trading-core` 是行情系统的核心类型层，也是未来低延迟或高频场景最可能复用的部分。

职责：

- 统一 message envelope：raw provider message 和 normalized event 共用外层字段。
- 标准事件结构：`Trade`、`Quote`、`BookUpdate`、`BookSnapshot`、
  `BookOrder`、`BookTrade`、`TradingPhaseUpdate`。
- `BookUpdate` 是 L2 聚合盘口的批量更新模型；旧的 `BookDelta` 仅保留为单价位兼容结构。
- 标准 header：schema version、source id、venue id、instrument id、feed id、序号、时间戳、flags。
- 统一 feed 描述：`FeedSpec`、`ResolvedFeed`、`FeedConnectionSpec` 和 raw provider message callback view。
- 价格和数量的 fixed-point 表达。
- Lv3 订单级事件模型。
- sink / callback 基础接口。
- 轻量序号和状态元数据。

限制：

- 不依赖 NATS、Kafka、数据库、服务框架。
- 不依赖任何供应商 SDK。
- 不放配置系统、监控导出器、日志重逻辑。
- 不绑定具体操作系统专属能力。
