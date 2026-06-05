# md-runtime 低延迟运行时模块

`md-runtime` 负责低延迟运行时能力，和 `md-core` 分开是为了避免核心事件模型被平台细节污染。

规划能力：

- CPU 绑核。
- 线程优先级和调度策略。
- clock source 和时间戳读取。
- 内存预分配、page lock、huge page hint。
- cache line 对齐辅助。
- 网络和线程运行时 profile。

普通中频部署可以不启用这些能力；低延迟部署通过配置显式启用。
