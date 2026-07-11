# 美股行情接入

美股行情 provider 放在这里。

## Lv3 支持策略

美股市场中，不同数据源差异很大：券商 API、SIP、交易所 depth、TotalView、ITCH 类 direct feed 的能力完全不同。

统一策略：

- ITCH / order-by-order feed 映射为 `OrderEvent` 和 `Execution`。
- 聚合 depth feed 映射为 `BookUpdate` 或 `BookSnapshot`。
- 券商 API 若只提供 quote/trade，则只能输出 `Quote` 和 `Trade`。
- provider 必须声明实际 `book_model` 和数据授权等级。
