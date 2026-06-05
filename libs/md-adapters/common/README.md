# adapter 通用约定

这里放跨市场、跨供应商共用的 adapter 约定。

## Lv3 能力边界

项目统一支持 Lv3 事件模型，但每个数据源是否能输出真实 Lv3，取决于外部 feed 是否提供订单级数据。

- 能提供逐笔委托和逐笔成交的数据源：映射为 `OrderEvent` 和 `Execution`。
- 只能提供价位聚合盘口的数据源：映射为 `BookDelta` 或 `BookSnapshot`，不能伪造成 Lv3。
- 只能提供逐笔成交的数据源：可以输出 `Trade` 或 `Execution`，但不能重建完整 MBO。
- adapter 必须在配置和状态中声明实际 book model：`L1`、`L2Mbp`、`L3Mbo`。

## 统一 Lv3 字段

`OrderEvent` 用于表达委托生命周期：新增、修改、撤单、删除、替换。

`Execution` 用于表达成交和执行关系：成交编号、买方委托号、卖方委托号、主动方、成交价格、成交数量。

不同市场字段不完全一致，缺失字段保留为 `0` 或 `Unknown`，并通过 `flags` 或 adapter 状态说明数据质量。
