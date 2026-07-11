# 国内期货行情接入

国内期货行情 provider 放在这里。

## Lv3 支持策略

普通 CTP 行情通常是有限档快照，不是订单级 Lv3。部分交易所直连、专线 feed 或供应商可能提供更细粒度数据。

统一策略：

- CTP 这类有限档行情映射为 `BookSnapshot` 或 `Quote`。
- 如果供应商提供订单级委托或执行事件，映射为 `OrderEvent` 和 `Execution`。
- provider 必须明确声明实际 `book_model`，不能把有限档快照伪造成 Lv3。
