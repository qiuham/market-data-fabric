# Market Data Fabric

`market-data-fabric` 是一个 C++20 行情接入和标准化骨架，目标是把不同交易所、券商或行情供应商的实时数据转换成稳定的内部事件模型，再交给 publisher / client / book builder 等下游模块。

当前重点是 crypto live 接入和 mapper，不是历史回放或持久化日志。

## 当前方向

- 先接入 live raw feed，再逐步增加标准化 mapper。
- 下游主接口使用统一事件模型，不直接依赖交易所 raw schema。
- 核心行情事件不按 crypto / 股票 / 期货拆成多套；资产类别、合约属性和精度放在 refdata。
- L2 聚合盘口使用批量 `BookUpdate`，不把一条 depth 消息拆成丢失序号和 checksum 的单价位事件。
- 只有真实订单级 feed 才输出 `OrderEvent` / `Execution`，不从 L2 聚合盘口伪造 Lv3。

## 已有能力

- `md-core`：`MessageEnvelope`、`RawProviderMessage`、`Trade`、`Quote`、`BookUpdate`、`BookSnapshot`、`OrderEvent`、`Execution` 等核心类型。
- `md-refdata`：`Instrument`、`AssetClass`、`ProductType`、精度、标的、到期日等基础参考数据字段。
- `md-adapters/crypto/binance`：Binance feed key、endpoint、WebSocket live raw client 和 `bookTicker -> Quote` mapper。
- `md-service`：feed session、重连/backoff、停止条件和基础运行统计。
- `md-runtime`：SPSC ring、统一日志封装等低延迟基础设施。
- `md-net`：WebSocket endpoint 解析和 Boost.Beast backend。

## 当前进度

- Binance Spot `bookTicker` 已完成 live raw 接入和 `Quote` 标准化映射。
- `bookTicker -> Quote` 已用 Binance 在线 BTCUSDT 数据验证，能同时查看 raw payload 和 normalized quote。
- mapper 使用固定点整数解析价格和数量，不走 `double`，避免精度误差和额外开销。
- 默认生产路径不输出逐条行情日志；`--log-payload` / `--log-normalized` 只用于调试 mapper 和字段检查。
- 启动、停止、重连、异常保留摘要日志；高频运行状态后续用轻量运行指标统计，不靠逐条日志。
- 本地 smoke tests 覆盖核心事件、Binance feed、mapper、session、WebSocket endpoint 和 SPSC ring。

## 仓库结构

```text
apps/md-node/                         统一进程入口
libs/md-core/                         核心事件、envelope、feed、sink
libs/md-refdata/                      instrument 和参考数据模型
libs/md-book/                         L1/L2/L3 盘口构建预留
libs/md-adapters/crypto/binance/      Binance 行情 adapter
libs/md-service/                      feed session 和 gateway 运行时
libs/md-runtime/                      队列、低延迟运行时基础设施
libs/md-net/                          网络辅助和 WebSocket backend
libs/md-transport/                    NATS/Kafka/shm/TCP/UDP/file 传输预留
libs/md-client/                       策略消费端库预留
docs/                                 架构和字段转换说明
tests/                                smoke tests
```

## 数据面

```text
external feed / SDK
  -> adapter client
  -> decoder
  -> provider mapper
  -> md-core event
  -> book / sequence check
  -> publisher / client
```

第一阶段允许 `raw_only` 跑通连接和样本采集；生产主路径应逐步转向 `normalized` 或 `dual`，让策略消费统一事件。

## 统一模型

通用事件：

- `Trade`：成交，`aggressor_side` 统一表示主动方 / taker 方向。
- `Quote`：最优买卖价。
- `BookUpdate`：L2 聚合盘口批量更新，包含 message 级序号和 checksum。
- `BookSnapshot`：有限档或全量快照。
- `OrderEvent` / `Execution`：真实 Lv3 订单级事件。
- `Status` / `Heartbeat`：状态和心跳。

产品差异放在 `Instrument`：

- `AssetClass`：Crypto、Equity、Futures、Options、Fx、Index。
- `ProductType`：Spot、Perpetual、Future、Option、Index、Fund。
- 精度、合约乘数、标的、到期日、行权价等属性。

特殊事件后续按产品扩展，例如 funding、mark price、open interest、auction imbalance、settlement price 等。

## Binance 当前支持

支持生成和连接这些 Spot feed：

- `trade`
- `aggTrade`
- `bookTicker`
- `!bookTicker`
- `depth`
- `depth@100ms`
- `depth5/10/20`

示例：

```bash
./cmake-build-debug/md-node --binance-feed-spec-preview
./cmake-build-debug/md-node --binance-live --symbol=BTCUSDT --feed=bookTicker --messages=3 --log-normalized=3
```

生产或长时间运行时不要打开逐条 payload / normalized 日志，建议只保留 warn 以上日志：

```bash
./md-node --binance-live --symbol=BTCUSDT --feed=bookTicker --log-level=warn
```

## 构建和测试

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug --output-on-failure
```

如果本机没有 Boost headers，CMake 可以按锁定版本下载；离线环境可以提前安装 Boost，或配置 `-DMDF_FETCH_BOOST=OFF`。

日志默认走 `md-runtime` 的统一封装；如果系统有 `quill` 或配置 `-DMDF_FETCH_QUILL=ON`，会使用 quill 异步日志，否则退回轻量 stderr logger。

## 当前路线图

1. 把 mapper 成功/失败等运行指标从调试日志 handler 中拆出来，形成独立 lightweight stats。
2. 继续实现 Binance mapper：`trade/aggTrade -> Trade`。
3. 实现 Binance `depth -> BookUpdate`，保留 message 级序号、prev seq 和 checksum。
4. 建立 adapter -> mapper -> normalized event 的 gateway pipeline，feed 线程只做轻量接收和投递。
5. 加入 publisher/client，先服务实时消费。
6. 扩展 OKX、Bybit、Coinbase、Kraken、Gate.io 等 crypto adapter。
7. 接入股票、期货等市场时复用核心事件，只扩展 refdata 和特殊事件。
8. 等 live、mapper 和传输层稳定后，再补历史回放和持久化日志。

## 更多文档

- `docs/architecture.md`
- `docs/adapter-field-mapping.md`
- `docs/data-modes.md`
- `docs/lv3-market-data.md`
- `docs/hft-readiness.md`
- `docs/deployment.md`
