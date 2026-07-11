# md-cluster 分布式协调模块

`cluster` 负责分布式部署中的逻辑控制面概念。

职责：

- 节点注册。
- 心跳。
- assignment（任务分配）。
- lease（租约）。
- primary/standby failover（主备故障切换）。
- publisher epoch（发布者世代号）。
- config version（配置版本）。

行情 tick 不应经过 `cluster`。控制面只处理低频元数据。
