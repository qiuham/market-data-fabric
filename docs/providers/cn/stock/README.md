# A 股行情适配器

A 股行情 adapter 放在这里。当前只接 native SDK，不接 CTPAPI 兼容层。

## 接入顺序

1. `zhongtai/xtppro`：优先接中泰 XTP Pro Quote。它能提供 10 档快照、订单簿、逐笔委托、逐笔成交、回补和证券静态信息。
2. `zhongtai/xtp`：保留中泰 XTP 3.0 Trader 设计位置，行情不作为新主线。
3. `huaxin/tora`：第二优先接华鑫 TORA native。先覆盖股票、ETF、债券和北交所快照，再按真实授权决定是否补逐笔。
4. 其他券商或供应商等 XTP/TORA 的模型和 pipeline 稳定后再评估。

不做 OpenCTP/CTPAPI 兼容接入。兼容层会损失字段保真度，也会把性能和错误语义交给额外封装层，不适合作为生产主线。

## Lv3 支持策略

A 股 Lv3 通常依赖供应商提供逐笔委托、逐笔成交或订单级重建能力。不同交易所、不同供应商、不同授权等级的字段可能不同。

统一策略：

- 逐笔委托映射为 `OrderEvent`。
- 逐笔成交映射为 `Execution`，同时可以派生 `Trade` 给中频消费者。
- 如果成交中包含买卖双方委托号，应填充 `buy_order_id` 和 `sell_order_id`。
- 如果只有逐笔成交、没有逐笔委托，不能宣称完整 `L3Mbo`。
- 上交所、深交所、北交所字段差异由具体 provider adapter 处理。
