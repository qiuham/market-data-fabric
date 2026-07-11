# Lv3 行情能力规划

本项目把 Lv3 定义为订单级行情能力，也就是能够表达单笔委托的生命周期和成交执行关系。统一模型适用于 crypto、A 股、美股、期货等市场，但每个外部数据源是否能输出真实 Lv3，取决于它是否提供订单级 feed。

## 行情层级定义

```text
L1：最优买卖价、最新成交、基础 quote。
L2：价位聚合盘口，通常是有限档快照或 MBP 增量。
L3：订单级行情，通常是 MBO / order-by-order / 逐笔委托 + 逐笔成交。
```

## 统一事件

Lv3 统一使用两个核心事件：

- `OrderEvent`：表达委托生命周期，包括新增、修改、撤单、删除、替换。
- `Execution`：表达成交和执行关系，包括成交编号、买方委托号、卖方委托号、主动方、价格和数量。

`Trade` 仍然保留，用于中频消费者或无法提供订单号关系的数据源。对于能提供 Lv3 的数据源，可以从 `Execution` 派生 `Trade`，但不能反过来用普通 `Trade` 伪造 `Execution`。

## 核心字段

`OrderEvent` 主要字段：

- `order_id`：当前委托编号。
- `original_order_id`：替换或修改前的委托编号。
- `partition_id`：交易所分区或业务分片。
- `order_seq`：委托业务序号。
- `priority_id`：时间优先级或队列优先级。
- `price`：fixed-point 价格。
- `quantity`：委托数量。
- `remaining_quantity`：剩余数量。
- `side`：买卖方向。
- `action`：新增、修改、撤单、删除、替换。
- `order_type`：限价、市价、最优价等。
- `time_in_force`：订单有效性。
- `trading_phase`：交易阶段。

`Execution` 主要字段：

- `trade_id`：成交编号。
- `buy_order_id`：买方委托编号。
- `sell_order_id`：卖方委托编号。
- `resting_order_id`：被动方委托编号。
- `aggressor_order_id`：主动方委托编号。
- `price`：成交价格。
- `quantity`：成交数量。
- `execution_type`：成交、撤单、冲正等。
- `aggressor_side`：主动买、主动卖、未知。

## 各市场策略

### 加密货币

很多 crypto 公开行情只提供 L2 聚合深度，不提供完整订单级数据。只有当交易所或供应商明确提供 order-by-order feed 时，adapter 才能输出 `OrderEvent` 和 `Execution`。

### A 股

A 股 Lv3 通常依赖逐笔委托和逐笔成交授权。逐笔委托映射为 `OrderEvent`，逐笔成交映射为 `Execution`。如果成交消息包含买卖双方委托号，应填充 `buy_order_id` 和 `sell_order_id`。

### 美股

美股需要区分 SIP、券商 API、聚合 depth、TotalView、ITCH 类 direct feed。只有订单级 feed 才输出 `OrderEvent` 和 `Execution`，普通 quote/trade 不能伪造成 Lv3。

### 期货

普通 CTP 行情通常是有限档快照，不是 Lv3。只有交易所直连、专线 feed 或供应商提供订单级事件时，才输出 Lv3 事件。

## 不伪造原则

adapter 必须声明实际能力：

```yaml
book_model: L1
book_model: L2Mbp
book_model: L3Mbo
```

如果外部 feed 只提供 L2 聚合盘口，adapter 只能输出 `BookUpdate` 或 `BookSnapshot`。不能拆分聚合数量来伪造订单级委托。

## MBO Book

`orderbook` 中预留 `MboBook`，用于从 `OrderEvent` 和 `Execution` 构建订单级盘口。后续实现时需要处理：

- 订单新增、修改、撤单、删除、替换。
- 成交对剩余数量的影响。
- 同价位时间优先级。
- partition 序号检查。
- snapshot 和增量恢复。
- 交易阶段切换。

## 下游消费

`client` 增加 `Order` 和 `Execution` 事件订阅能力。策略可以选择：

- 中频：只消费 `Trade`、`Quote`、`BookSnapshot`。
- 低延迟：消费 `BookUpdate`。
- Lv3/HFT：消费 `OrderEvent`、`Execution`，自行或通过 `MboBook` 构建订单簿。
