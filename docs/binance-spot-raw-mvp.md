# Binance Spot Raw MVP

本次先选 Binance Spot 做第一个 crypto raw 接入对象。原因是它的公开行情 WebSocket 比较直接，适合先验证原始报文采集、统一 envelope、message log 和 feed 模型。

参考文档来自用户提供的 `crypto-api-docs` 仓库：

- `docs/binance/binance-spot-api-docs/README.md`
- `docs/binance/binance-spot-api-docs/faqs/market_data_only.md`
- `docs/binance/binance-spot-api-docs/sbe-market-data-streams.md`

## 第一阶段范围

第一阶段只做 raw，不做字段标准化：

```text
Binance WebSocket feed
  -> MessageEnvelope(payload_kind = ProviderMessage)
  -> 原始 JSON payload
  -> message log 文件
```

当前已落地的代码：

- `libs/md-core/include/md/core/feed.hpp`
- `libs/md-adapters/crypto/binance/include/md/adapters/crypto/binance/feeds.hpp`
- `configs/dev/binance_spot_raw.yaml`
- `tests/binance_feeds_smoke.cpp`
- `tests/message_log_smoke.cpp`

## 支持的 feed 名称生成

当前先支持这些公开行情 feed 名称。Binance 官方 URL 中仍使用 stream 这个词，代码内部统一使用 feed：

```text
<symbol>@trade
<symbol>@aggTrade
<symbol>@bookTicker
<symbol>@depth
<symbol>@depth@100ms
<symbol>@depth5
<symbol>@depth10
<symbol>@depth20
<symbol>@depth5@100ms
<symbol>@depth10@100ms
<symbol>@depth20@100ms
!bookTicker
```

代码会把 `BTCUSDT` 这类 symbol 转成 Binance feed 需要的小写形式，例如：

```text
BTCUSDT + trade -> btcusdt@trade
ETHUSDT + depth@100ms -> ethusdt@depth@100ms
```

## 为什么 raw MVP 优先用 single feed

Binance 有两种常见 WebSocket URL：

```text
/ws/<streamName>
/stream?streams=<streamName1>/<streamName2>
```

raw-only 阶段优先用 `/ws/<streamName>`，因为 feed 名称来自连接本身，不需要解析 payload 就能生成 `feed_id`。

combined endpoint 的 payload 外面通常还有一层 wrapper。如果要从 payload 中提取具体 feed，就会进入解析逻辑。这个解析以后可以做，但不是 raw MVP 的优先事项。

## Feed 设计

内部不使用 Binance 的 stream、OKX/Coinbase 的 channel 或 Bybit 的 topic 作为统一概念，统一使用 feed：

```text
FeedSpec
  -> ResolvedFeed(provider_feed_key, feed_id)
  -> FeedConnectionSpec(endpoint, startup_messages)
```

Binance 单 feed 当前对应：

```text
FeedSpec(symbols=[BTCUSDT], Trade)
  -> provider_feed_key = btcusdt@trade
  -> endpoint = wss://data-stream.binance.vision:443/ws/btcusdt@trade
```

`symbols=[]`、`symbols=[ALL]` 或 `symbols=[*]` 表示全量市场。adapter 会优先映射到供应商本身的聚合 feed；如果该 feed 类型没有供应商聚合能力，就需要先从 universe 展开成具体 symbols。

`symbols=[BTCUSDT,ETHUSDT]` 会生成 combined endpoint。当前 live client 不解析 combined wrapper，所以 envelope 先只保证 `connection_id` 正确；只有 single feed 或供应商聚合 feed 能在不读 payload 的情况下填 `feed_id`。

OKX/Bybit/Coinbase 这类后续通过固定 endpoint + startup message 的交易所，也会落到同一个 `FeedConnectionSpec`。

## 生产域名选择

当前默认使用 public market data 专用域名：

```text
wss://data-stream.binance.vision:443/ws/<streamName>
```

也预留了：

```text
wss://stream.binance.com:9443/ws/<streamName>
wss://stream.testnet.binance.vision/ws/<streamName>
wss://demo-stream.binance.com:9443/ws/<streamName>
```

## 下一步

后续实现真实网络接入时，需要补三块：

1. 重连/backoff 和更完整的连接生命周期。
2. `md-runtime` 的 queue，让网络回调不要被文件 IO 卡住。
3. gateway pipeline，把 `FeedConnectionSpec`、WebSocket 收包和 `MessageLogWriter` 串起来。

normalized mapper 先不做。等 raw 样本稳定后，再用这些样本实现 `Trade`、`Quote`、`BookDelta` 和后续 book builder。
