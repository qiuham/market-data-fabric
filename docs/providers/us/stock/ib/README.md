# IB 行情适配器

Interactive Brokers 行情 adapter 骨架。

IB API 通常通过 TWS 或 IB Gateway 接入，适合中低频和多市场覆盖，不应被当作低延迟直连行情源。

职责：

- 封装 IB market data API。
- 处理 contract 到内部 instrument 的映射。
- 处理 pacing limit、订阅权限、重连和重订阅。
- 将 tick price、tick size 等事件转换成内部标准事件。
