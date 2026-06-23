# md-replay

`md-replay` 是历史回放能力预留模块，当前不是开发主线。

等 live 接入、标准化 mapper、publisher / client 稳定后，再补：

- raw 样本回放，用于重跑 decoder / mapper。
- normalized event 回放，用于策略和研究。
- 回放速度控制、过滤和时间范围选择。

当前文档不展开持久化日志格式，避免过早绑定实现。
