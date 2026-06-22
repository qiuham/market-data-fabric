# Binance 行情适配器

Binance 行情 adapter 当前先实现 Spot raw MVP 的基础能力。raw 只表示 payload 是供应商原始报文，外层仍使用统一 `MessageEnvelope`。

已落地：

- `BinanceFeedSpec -> ResolvedFeed -> FeedConnectionSpec`：启动时生成 provider feed key、endpoint 和 `feed_id`。
- Spot feed key 生成：`trade`、`aggTrade`、`bookTicker`、全市场 `!bookTicker`、`depth`、`depth@100ms`、`depth5/10/20`。
- WebSocket URL 生成：Spot 生产、market-data-only、testnet、demo 环境，并预留 Binance 衍生品 endpoint。
- `MessageEnvelope` 模板：不解析 payload，只用连接和订阅层已知的信息填 `source_id`、`connection_id`、`feed_id`、`capture_seq`、`recv_ts_ns`。
- `BinanceFeedClient` 最小 live client：基于 `md-net` 的 Boost.Beast backend 接收原始 payload，并通过 `string_view` callback 暴露，不做 JSON 解析或标准化转换。
- 多 symbol spec 会生成 Binance combined endpoint；当前 raw client 不解析 wrapper，combined 消息先按 connection 级 envelope 输出。

后续职责：

- 处理 WebSocket ping/pong、重连、订阅生命周期。
- 在 raw_only 模式下录制原始 JSON/SBE payload。
- 在 normalized 模式下解码 trade、depth、book ticker 等原始消息。
- 处理 snapshot + delta 对齐。
- 检查 update id / sequence gap。
- 将外部字段转换为 `md-core` 标准事件。

注意：如果 Binance 只提供聚合深度，就只能输出 L2 MBP，不允许伪造成 Lv3 MBO。
