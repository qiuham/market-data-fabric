# A 股逐笔盘口重建与验证

## 仓库边界

- `marketdata` 解析通联 MDL 字段并转换为统一 `BookOrder` / `BookTransaction`。
- `trading-core` 定义统一事件，不包含供应商字段。
- `orderbook` 的 `MboRebuilder` 按交易所序号消费事件，不识别通联快照。
- `book-validate` 复用同一个 `MboRebuilder` 做离线对账，不维护第二套状态机。

生产路径只依赖逐笔流。沪市使用 `Channel + BizIndex`，深市使用
`ChannelNo + ApplSeqNum`；时间戳和行情快照都不能推进或修正生产订单簿。

## 为什么不能按时间戳对快照

快照的 `UpdateTime` 是供应商发布或标记时间，不是逐笔流中的精确截止序号。尤其
沪市快照没有携带生成该快照时对应的 `BizIndex`。同一毫秒内可以有多条委托、
撤单和成交，供应商内部合流与发布还会引入随标的变化的相位。因此：

- 固定延迟、最大延迟、P75 等策略都只能猜测边界；
- 猜早会漏事件，猜晚会多回放事件；
- 高活跃沪市标的更容易表现为快照数量不一致；
- 这不代表逐笔重建状态机有错。

深市过去按时间对账看起来更稳定，是因为样本中的发布相位和事件密度更容易让估计
边界碰巧落对，并不是深市时间戳具备精确 cutoff 语义。统一设计不依赖这种偶然性。

## 成交锚点区间验证

每张快照提供累计成交笔数、累计成交量和成交额。验证器把三者组成成交锚点：

```text
TradeAnchor = (cumulative_trade_count, cumulative_trade_volume,
               cumulative_trade_amount)
```

回放逐笔时，只有成交会改变锚点；委托和撤单只改变订单簿。因此相同锚点天然定义
了两次成交之间的候选事件区间。验证器在该区间的每个事件边界严格比较买卖十档
价格和数量，以及买卖双边总挂量，不设置数量容差。

| 结果 | 含义 | 结论 |
|---|---|---|
| `UniqueWindowMatch` | 区间内唯一状态匹配 | 重建正确，且该样本可唯一定位 |
| `AmbiguousWindowMatch` | 区间内多个状态匹配 | 重建正确，快照 cutoff 不可唯一定位 |
| `NoBookMatch` | 锚点存在但没有匹配状态 | 检查逐笔、mapper 或状态机 |
| `NoTradeAnchor` | 锚点在逐笔流中不存在 | 检查丢数或累计字段口径 |

多解通常是因为若干事件没有改变可见十档和总挂量，或订单簿在区间内再次回到相同
可见状态。验证目标是证明逐笔可产生快照状态，不是猜供应商在哪条事件后采样，
所以多解属于有效结果。

## 通联字段口径

当前输入是 Parquet 工作集：

```text
ROOT/
  sh/market_snapshot.parquet
  sh/tick_by_tick.parquet
  sz/market_snapshot.parquet
  sz/orders.parquet
  sz/transactions.parquet
```

- `SecurityIDSource=101/102` 映射到 `.SH/.SZ`。
- `Side=49/50` 映射到买/卖。
- 深市 `ExecType=70` 是成交，`ExecType=52` 是撤单。
- 沪市 `Type=A/D/T/S` 分别映射新增、删除、成交、状态消息。
- 沪市新增行的 `TradeMoney` 是已成交委托数量，不加入订单簿。
- 价格统一转换为 1e4 定点整数。

连续竞价之外的快照不参与验证，避免把集合竞价的虚拟撮合展示规则混入连续竞价
MBO 重建检查。

## 2026-04-17 样本结果

| 标的 | 连续竞价快照 | Unique | Ambiguous | NoBookMatch | NoTradeAnchor |
|---|---:|---:|---:|---:|---:|
| `600006.SH` | 3970 | 3928 | 42 | 0 | 0 |
| `600410.SH` | 4934 | 4905 | 29 | 0 | 0 |
| `000030.SZ` | 4782 | 4756 | 26 | 0 | 0 |

沪深样本都能由逐笔流产生全部快照状态，mapper 和订单簿 apply 异常均为 0。这证明
同一套“交易所序号驱动生产簿、累计成交锚点验证候选区间”逻辑可以覆盖沪深，不再
需要市场特定的时间延迟调参。

## 完整性检查

单标的 Parquet helper 会在读取时先按 `SecurityID` 过滤。由于一个 Channel 包含
多只证券，过滤后的序号跳跃是正常现象，不能据此报告 gap。生产级完整性检查应在
过滤前维护每个 Channel 的 sequence watermark，先检测 gap、重复和倒退，再按
证券路由事件。当前工具输出的单票首末序号、重复和倒退仅用于顺序诊断。
