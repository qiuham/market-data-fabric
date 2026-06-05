# md-core 核心库

`md-core` 是行情系统的核心类型层，也是未来低延迟或高频场景最可能复用的部分。

职责：

- 标准事件结构：`Trade`、`Quote`、`BookDelta`、`BookSnapshot`、`OrderEvent`、`Execution`、`Status`。
- 标准 header：schema version、source id、venue id、instrument id、stream id、序号、时间戳、flags。
- 价格和数量的 fixed-point 表达。
- Lv3 订单级事件模型。
- sink / callback 基础接口。
- 轻量序号和状态元数据。

限制：

- 不依赖 NATS、Kafka、数据库、服务框架。
- 不依赖任何供应商 SDK。
- 不放配置系统、监控导出器、日志重逻辑。
- 不绑定具体操作系统专属能力。
