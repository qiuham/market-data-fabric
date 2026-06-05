# Binance 行情适配器

Binance 行情 adapter 骨架。

职责：

- 连接 Binance 行情 endpoint。
- 解码 trade、depth、book snapshot 等原始消息。
- 处理 snapshot + delta 对齐。
- 检查 update id / sequence gap。
- 将外部字段转换为 `md-core` 标准事件。

