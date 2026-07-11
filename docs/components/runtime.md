# runtime 低延迟运行时模块

`runtime` 负责低延迟运行时能力，和 `trading-core` 分开是为了避免核心事件模型被平台细节污染。

规划能力：

- CPU 绑核。
- 线程优先级和调度策略。
- clock source 和时间戳读取。
- 内存预分配、page lock、huge page hint。
- cache line 对齐辅助。
- 通用队列，例如 `SpscRing<T>`。
- 统一日志封装，优先使用 quill，缺失时退回轻量 stderr logger。
- 网络和线程运行时 profile。

`SpscRing<T>` 是通用基础设施，不绑定具体消息类型。它适合一个生产者线程和一个消费者线程，例如单个 WebSocket 连接线程到 recorder worker。多连接共用一个队列时不能使用 SPSC，需要后续补 MPSC 或分片队列。

日志只用于 lifecycle、错误、gap、统计摘要和显式 debug 输出；行情 tick 热路径默认不逐条写日志。

普通中频部署可以不启用这些能力；低延迟部署通过配置显式启用。
