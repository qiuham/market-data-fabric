# TORA A 股 native接入

华鑫 TORA 是 A 股 native 接入的第二优先级。这里后续放 TORA SDK wrapper、字段 view、mapper 和测试 fixture。

## 行情范围

- 股票、ETF、债券、北交所快照。
- `subscribeMarketData` / `onRtnMarketData`：通常是 5 档快照，映射为 `MarketSnapshot<5>`。
- 如果实盘授权提供逐笔或更深档位，再补 `OrderEvent`、`Execution` 或更深档快照。

## 交易范围

交易接口先只做模型和 mock，不直接进入实盘主线：

- `reqOrderInsert`：下单。
- `reqOrderAction`：撤单。
- `onRtnOrder`：委托状态回报。
- `onRtnTrade`：成交回报。
- `reqQrySecurity` / `reqQryTradingAccount` / `reqQryPosition`：合约、资金和持仓查询。

## 实现约束

- 不通过 OpenCTP 或其他 CTPAPI 兼容层接 TORA。
- SDK 回调里只做轻量复制和投递，不做复杂解析、日志和内存分配。
- mapper 必须显式处理上交所、深交所、北交所字段差异。
