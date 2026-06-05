# md-adapters 行情适配器

`md-adapters` 负责连接外部行情源或供应商 SDK，把外部消息转换成 `md-core` 标准事件。

建议按供应商拆分，而不是只按资产类型拆分。例如：

- `crypto/binance`：Binance 行情适配器。
- `crypto/okx`：OKX 行情适配器。
- `cn_futures/ctp`：CTP 期货行情适配器。
- `cn_equity/xtp`：XTP A 股行情适配器。
- `us_equity/ib`：IB 美股行情适配器。

原因是同一资产类型内部，不同供应商的协议、字段、序号、限流、断线恢复和盘口语义也可能完全不同。

## 字段映射目录约定

不设置通用 `common` 字段映射目录。所有供应商报文字段映射必须放在具体 provider 的 `mapping/` 目录下，避免误导成多个供应商可以共用同一份 mapping。
