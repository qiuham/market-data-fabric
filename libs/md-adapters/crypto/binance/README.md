# Binance 行情适配器

Binance 行情 adapter 当前先实现 Spot raw-provider-message MVP 的基础能力：payload 保留供应商原始报文，外层使用统一 `MessageEnvelope`。

已落地：

- `BinanceFeedSpec -> ResolvedFeed -> FeedConnectionSpec`：启动时生成 provider feed key、endpoint 和 `feed_id`。
- Spot feed key 生成：`trade`、`aggTrade`、`bookTicker`、全市场 `!bookTicker`、`depth`、`depth@100ms`、`depth5/10/20`。
- WebSocket URL 生成：Spot 生产、market-data-only、testnet、demo 环境，并预留 Binance 衍生品 endpoint。
- `MessageEnvelope` 模板：不解析 payload，只用连接和订阅层已知的信息填 `source_id`、`connection_id`、`feed_id`、`capture_seq`、`recv_ts_ns`。
- `BinanceWebSocketFeedClient` 最小 live client：基于 `md-net` 的 Boost.Beast backend 接收原始 payload，并通过 `string_view` callback 暴露，不做 JSON 解析或标准化转换。
- `bookTicker -> Quote` mapper：把 Binance 最优买卖价字段转换为内部 BBO 模型。
- `trade -> Trade` mapper：把 Binance 原始逐笔成交转换为内部成交模型，并反转 maker-side 字段得到 taker/aggressor side。
- mapper 共用轻量 JSON value reader，放在 `detail/` 下，后续替换 decoder 时不影响 mapper 对外接口。
- 多 symbol spec 会生成 Binance combined endpoint；当前 live client 不解析 wrapper，combined 消息先按 connection 级 envelope 输出。

后续职责：

- 在 normalized 模式下继续解码 depth 等原始消息，下一步优先做 `depth -> BookUpdate`。
- 处理 snapshot + delta 对齐。
- 检查 update id / sequence gap。
- 将外部字段转换为 `md-core` 标准事件。
- 等多个 crypto adapter 出现相同需求后，再提公共 mapper context/helper；当前不先做 Binance 专用抽象的全局化。

注意：如果 Binance 只提供聚合深度，就只能输出 L2 MBP，不允许伪造成 Lv3 MBO。
