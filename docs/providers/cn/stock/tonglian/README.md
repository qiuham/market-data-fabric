# 通联 A 股行情适配

通联层负责 Parquet/SDK 字段、字符枚举和供应商时间表达，随后生成统一中国交易所
view；上交所和深交所业务规则分别复用 `venues/cn/sse` 与 `venues/cn/szse`，最终
输出 trading-core 标准事件。

```text
Tonglian row
  -> adapters/cn/stock/tonglian
  -> CN venue view
  -> SSE/SZSE normalizer
  -> BookOrder / BookTrade
```

当前离线工作集：

```text
ROOT/
  sh/market_snapshot.parquet
  sh/tick_by_tick.parquet
  sz/market_snapshot.parquet
  sz/orders.parquet
  sz/transactions.parquet
```

验证：

```bash
cd ../trading-evaluation
CN_BOOK_CHECK_PYTHON=/path/to/python \
  ./build/book-validate ROOT \
  --symbols 600006.SH,000030.SZ \
  --summary
```

导出标准事件并回溯：

```bash
./build/book-validate ROOT --symbols 600006.SH --journal-dir journals
./build/event-trace journals/600006.SH.mdevt --order 18064
./build/event-trace journals/600006.SH.mdevt --trade 289538
```

沪市 `Type=S` 会参与 Channel/BizIndex 连续性，并映射为 trading-core `TradingPhaseUpdate`
供 phase tracker 和 journal 使用，但不修改订单簿。单证券过滤后的
BizIndex 跳号不能判断丢包，完整 gap 检查必须位于证券过滤之前。
