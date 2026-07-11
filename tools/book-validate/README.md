# book-validate

`book-validate` 用通联逐笔数据重建订单簿，并用行情快照做离线一致性验证。
生产重建和验证边界严格分离：订单簿只按交易所序号推进，快照不参与回放、调参
或修正订单簿。

## 设计

```text
沪市 tick_by_tick             深市 orders + transactions
       |                                  |
       +-- 按 Channel + 交易所序号排序 ---+
                          |
                   Tonglian mapper
                          |
             BookOrder / BookTransaction
                          |
                    MboRebuilder
                          |
       每个事件边界记录 (累计成交笔数, 累计成交量)
                          |
       与相同成交锚点的快照比较十档和双边总挂量
```

时间戳不用于选择快照截止点。沪市快照没有提供对应逐笔流的 `BizIndex` cutoff，
发布时间也不等于采样边界；用毫秒时间或估计延迟强行对齐会把正确的订单簿判错。

累计成交笔数、成交量和成交额只负责界定候选区间。在两次成交之间可能有多条委托和撤单，
验证器逐一检查区间内所有事件后的订单簿状态：

- `UniqueWindowMatch`：区间内只有一个状态与快照完全一致。
- `AmbiguousWindowMatch`：区间内有多个一致状态，能证明订单簿正确，但无法反推唯一 cutoff。
- `NoBookMatch`：成交锚点存在，但区间内没有一致状态。
- `NoTradeAnchor`：逐笔流中不存在快照声明的累计成交锚点。

前两种都表示验证通过。验证器不会从候选中选一个状态写回订单簿；实盘也不需要
知道供应商在哪个候选事件后生成了快照。

## 输入和使用

只支持语义化 Parquet/ZSTD 工作集：

```text
ROOT/
  sh/market_snapshot.parquet
  sh/tick_by_tick.parquet
  sz/market_snapshot.parquet
  sz/orders.parquet
  sz/transactions.parquet
```

```bash
cmake -S . -B build
cmake --build build --target book-validate

CN_BOOK_CHECK_PYTHON=/path/to/python \
  ./build/book-validate /data/20260417 \
  --symbols 600006.SH,000030.SZ --summary
```

Parquet helper 需要 Python 环境可导入 `pyarrow`。参数：

- `--symbols A.SH,B.SZ`：验证指定标的；不提供时从快照文件发现标的。
- `--market ALL|SH|SZ`：限制自动发现的市场，默认 `ALL`。
- `--limit N`：自动发现时最多验证 N 个标的，默认 8；`0` 表示不限。
- `--mismatch-limit N`：每个标的最多打印 N 条未匹配快照，默认 3。
- `--journal-dir DIR`：按标的导出与供应商无关的 trading-core 标准事件日志。
- `--summary`：输出汇总和异常分类。

成交额使用 1e4 定点口径，逐笔侧累计 `price * quantity`，快照侧读取
`Turnover`。订单和成交回溯已经移到独立 `event-trace`，验证器只负责可选导出
公共 journal：

```bash
./build/book-validate ROOT --symbols 600006.SH --journal-dir journals
./build/event-trace journals/600006.SH.mdevt --order 18064
./build/event-trace journals/600006.SH.mdevt --trade 289538
```

只验证连续竞价快照。沪市优先使用 `InstruStatus=TRADE`，字段缺失时使用
09:30-14:57；深市当前使用同一时间范围。

## 序列诊断边界

沪市按合并逐笔的 `BizIndex` 回放，深市按 `ChannelNo + ApplSeqNum` 回放。输出的
重复、倒退和首末序号可诊断单票数据顺序，但不能用单票序号跳跃判断丢包，因为
同一 Channel 混有其他证券。严格 gap 检测必须先消费未按证券过滤的完整 Channel
流，再把事件分发给各订单簿。
