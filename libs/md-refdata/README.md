# md-refdata 参考数据模块

`md-refdata` 负责行情标准化需要的参考数据。

包括：

- 内部 `instrument_id`。
- 外部 symbol 到内部 ID 的映射。
- 交易所和 venue 信息。
- 资产类型：crypto、A 股、美股、期货、期权等。
- 价格精度、数量精度、合约乘数。
- 交易日历和交易时段。

热路径中应尽量使用整数 ID，不要反复传递或比较字符串 symbol。
