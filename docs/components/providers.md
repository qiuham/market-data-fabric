# Providers

`providers` 负责连接外部行情源或供应商 SDK，把外部消息转换成 `trading-core` 标准事件。字段转换以 provider 内的 C++ mapper 和测试为准。

目录按 `marketdata-api-doc` 的分类语义对齐，但代码里省略重复的 `domain` 层，结构是：

```text
scope / asset_class / provider / product
```

例如：

- `crypto/binance`：Binance 行情接入。
- `crypto/okx`：OKX 行情接入。
- `cn/stock/zhongtai/xtppro`：中泰 XTP Pro A 股行情接入。
- `cn/stock/zhongtai/xtp`：中泰 XTP 3.0 交易接入预留。
- `cn/stock/huaxin/tora`：华鑫 TORA A 股行情接入。
- `cn/futures/ctp`：CTP 期货行情接入。
- `us/stock/ib`：IB 美股行情接入。

原因是同一资产类型内部，不同供应商的协议、字段、序号、限流、断线恢复和盘口语义也可能完全不同。

`gateways`、`exchanges` 这类 domain 仍然保留在外部文档项目里；代码仓库的 `providers` 本身已经表达接入语义，所以不再额外增加这一层目录。

## 字段转换目录约定

不设置通用 `common` 字段转换目录，也不维护手写 mapping YAML。所有供应商报文字段转换必须放在具体 provider 的 C++ mapper 中，并配套测试和 fixture。供应商 API 字段文档由外部文档项目维护。

## A 股接入约束

A 股 provider 只接 native SDK。不要通过 CTPAPI 兼容层接股票柜台，也不要把兼容层字段作为标准语义来源。

原因：

- 兼容层会把 provider 原生字段压扁成另一套 API，逐笔、10 档、订单簿、状态字段可能丢失或变形。
- 兼容层多一层动态库封装，回调、拷贝、版本兼容和错误语义都不可控。
- native SDK 的字段、序号、限流和断线恢复才是 mapper 测试和生产排障的可靠依据。
