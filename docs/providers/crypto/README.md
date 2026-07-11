# crypto 行情接入

加密货币市场 provider 放在这里。

## Lv3 支持策略

加密货币交易所常见公开行情是 L2 价位聚合盘口：snapshot + depth delta。并不是所有交易所都提供公开订单级 Lv3 数据。

统一策略：

- 如果交易所或供应商提供订单级 feed，provider 输出 `OrderEvent` 和 `Execution`。
- 如果只提供聚合深度，provider 只能输出 `BookUpdate` / `BookSnapshot`。
- 不允许用聚合价位盘口伪造订单级 MBO。
- 配置中必须声明实际能力：`book_model: L2Mbp` 或 `book_model: L3Mbo`。
