# md-runtime 低延迟运行时模块

`md-runtime` 负责低延迟运行时能力，和 `md-core` 分开是为了避免核心事件模型被平台细节污染。

规划能力：

- CPU 绑核。
- 线程优先级和调度策略。
- clock source 和时间戳读取。
- 内存预分配、page lock、huge page hint。
- cache line 对齐辅助。
- 通用队列，例如 `SpscRing<T>`。
- 网络和线程运行时 profile。

`SpscRing<T>` 是通用基础设施，不绑定 raw 或 normalized。它适合一个生产者线程和一个消费者线程，例如单个 WebSocket 连接线程到 recorder worker。多连接共用一个队列时不能使用 SPSC，需要后续补 MPSC 或分片队列。

普通中频部署可以不启用这些能力；低延迟部署通过配置显式启用。
