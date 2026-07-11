# CTP 行情接入

CTP 期货行情 provider 骨架。

职责：

- 封装 CTP 行情 SDK 回调。
- 快速复制/转换 SDK 回调字段，避免在 SDK 回调里做重活。
- 处理交易日、ActionDay、UpdateTime、UpdateMillisec 等时间语义。
- 将 `CThostFtdcDepthMarketDataField` 转换成内部标准事件。


CTP SDK 头文件和二进制库默认不提交到仓库，后续通过本地依赖或受控 vendor 目录接入。
