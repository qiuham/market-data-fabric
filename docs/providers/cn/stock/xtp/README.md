# XTP 3.0 A 股适配器

中泰 XTP 3.0 这里主要保留交易侧设计位置。行情主线使用同 provider 下的 `xtppro`，避免继续投入即将迁移的旧行情接口。

## 行情参考范围

- `QueryAllTickers` / `QueryAllTickersFullInfo`：可作为字段语义参考。
- `SubscribeMarketData` / `OnDepthMarketData`：旧行情接口参考，不作为新实现主线。
- `SubscribeOrderBook` / `OnOrderBook`：旧订单簿接口参考。
- `SubscribeTickByTick` / `OnTickByTick`：旧逐笔接口参考。

## 交易范围

交易接口先只做模型和 mock，不直接进入实盘主线：

- `InsertOrder`：普通股票、ETF、债券、期权等下单入口。
- `CancelOrder`：撤单。
- `OnOrderEvent`：委托状态回报。
- `OnTradeEvent`：成交回报。
- `QueryAsset` / `QueryPosition`：资金和持仓。

## 实现约束

- 不通过 OpenCTP 或其他 CTPAPI 兼容层接 XTP。
- SDK 回调里只做轻量复制和投递，不做复杂解析、日志和内存分配。
- mapper 使用固定点整数和字段有效位，避免 `double` 直接进入核心事件。
- provider 特有字段先保留在 XTP mapper 层，只有多 provider 复用后再提升到核心模型。
