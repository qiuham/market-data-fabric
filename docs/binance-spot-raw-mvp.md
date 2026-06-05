# Binance Spot Raw MVP

本次先选 Binance Spot 做第一个 crypto raw 接入对象。原因是它的公开行情 WebSocket 比较直接，适合先验证原始报文采集、统一 envelope、message log 和订阅模型。

参考文档来自用户提供的 `crypto-api-docs` 仓库：

- `docs/binance/binance-spot-api-docs/README.md`
- `docs/binance/binance-spot-api-docs/faqs/market_data_only.md`
- `docs/binance/binance-spot-api-docs/sbe-market-data-streams.md`

## 第一阶段范围

第一阶段只做 raw，不做字段标准化：

```text
Binance WebSocket stream
  -> MessageEnvelope(payload_kind = ProviderMessage)
  -> 原始 JSON payload
  -> message log 文件
```

当前已落地的代码：

- `libs/md-adapters/crypto/binance/include/md/adapters/crypto/binance/spot_streams.hpp`
- `configs/dev/binance_spot_raw.yaml`
- `tests/binance_spot_streams_smoke.cpp`
- `tests/message_log_smoke.cpp`

## 支持的 stream 名称生成

当前先支持这些公开行情 stream 名称：

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
```

代码会把 `BTCUSDT` 这类 symbol 转成 Binance stream 需要的小写形式，例如：

```text
BTCUSDT + trade -> btcusdt@trade
ETHUSDT + depth@100ms -> ethusdt@depth@100ms
```

## 为什么 raw MVP 优先用 single stream

Binance 有两种常见 WebSocket URL：

```text
/ws/<streamName>
/stream?streams=<streamName1>/<streamName2>
```

raw-only 阶段优先用 `/ws/<streamName>`，因为 stream 名称来自连接本身，不需要解析 payload 就能生成 `stream_id`。

combined stream 的 payload 外面通常还有一层 stream wrapper。如果要从 payload 中提取具体 stream，就会进入解析逻辑。这个解析以后可以做，但不是 raw MVP 的优先事项。

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

1. adapter 内的 WebSocket client，负责 TLS、ping/pong、重连、订阅生命周期。
2. `md-runtime` 的 queue，让网络回调不要被文件 IO 卡住。
3. gateway pipeline，把 `SpotSubscription`、WebSocket 收包和 `MessageLogWriter` 串起来。

normalized mapper 先不做。等 raw 样本稳定后，再用这些样本实现 `Trade`、`Quote`、`BookDelta` 和后续 book builder。
