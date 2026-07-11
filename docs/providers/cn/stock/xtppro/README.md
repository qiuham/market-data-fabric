# XTP Pro A 股行情接入

中泰 XTP Pro 是当前 A 股行情接入主线。这里后续放 XTP Pro Quote SDK wrapper、字段 view、mapper 和测试 fixture。

## 行情范围

- `SetConfigFile` / `Login`：实盘 UDP 行情必须先读取配置文件，再登录。
- `QueryAllTickers` / `QueryAllTickersFullInfo`：证券静态信息和精度。
- `QueryTickersLatestMarketData`：查询合约最新快照，用于启动补当前状态。
- `SubscribeMarketData` / `OnDepthMarketData`：10 档快照，映射为 `MarketSnapshot<10>`。
- `SubscribeOrderBook` / `OnOrderBook`：订单簿快照，映射为 `BookSnapshot<10>`。
- `SubscribeTickByTick` / `OnTickByTick`：逐笔委托和逐笔成交，分别映射为 `OrderEvent` 和 `Execution`。
- `RequestRebuildQuote` / `OnRebuildTickByTick` / `OnRebuildMarketData`：回补行情，单独标记为 rebuild 数据。

## 暂缓范围

- 指数通、港股通和期权特殊行情先只保留设计位置，不进入第一阶段实现。
- XTP Pro Trader 暂不作为主线，交易侧先保留 XTP 3.0 Trader 设计空间。

## 实现约束

- 不通过 OpenCTP 或其他 CTPAPI 兼容层接 XTP Pro 行情。
- SDK 回调里只做轻量复制和投递，不做复杂解析、日志和内存分配。
- mapper 使用固定点整数和字段有效位，避免 `double` 直接进入核心事件。
- SDK header 和动态库不提交到仓库；本目录只放 wrapper、view、mapper、fixture 和说明。
