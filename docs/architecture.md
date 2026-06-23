# 架构说明

## 职责边界

`md-core` 是可复用的核心层，负责标准事件类型、时间戳、ID、序号元数据和 sink 接口。它不应该依赖消息队列、供应商 SDK、数据库、服务治理或操作系统专属实现。

`md-refdata` 负责品种和市场上下文，包括内部 instrument id、外部 symbol 映射、交易所、资产类型、价格精度、数量精度、交易日历和交易时段。

`md-book` 负责盘口构建和校验。不同市场的盘口语义差异很大，因此需要分别支持 L1、有限档快照、价位聚合增量、Lv3 订单级盘口等模型。

`md-codecs` 放通用协议解析辅助能力，例如 JSON、二进制 frame、FIX/SBE 风格消息、编码转换等。具体供应商字段语义仍然属于 adapter。

`md-adapters` 负责外部行情源接入。每个供应商可能有不同的连接方式、断线恢复、订阅限制、时间字段、序号规则和盘口语义，因此 adapter 应按 provider 拆分，而不是只按资产类型粗分。

`md-transport` 负责标准化事件的分发和订阅，例如 NATS、Kafka、共享内存、TCP、UDP 组播、文件等。

`md-service` 负责单进程内的 gateway 运行时，包括配置加载、pipeline 构建、adapter 生命周期、publisher/recorder 组装、健康检查和指标。

`md-cluster` 负责分布式控制面概念，包括 assignment、lease、failover、node registry 和 publisher epoch。行情 tick 不应经过这一层。

`md-runtime` 负责低延迟运行时钩子，例如 CPU 绑核、调度策略、clock reader、内存锁定、预分配和平台相关调优。它是可选层，不应让 `md-core` 变成平台专属库。

`md-client` 是消费端库。策略通过它消费 live 标准化事件，而不需要关心底层传输是 NATS、Kafka、共享内存还是其他方式。对于 Lv3 数据，消费端可以订阅 `OrderEvent` 和 `Execution`。

## Kafka 风格角色模型

项目可以使用一个二进制程序承载多个角色：

```text
md-node --roles=gateway
md-node --roles=controller
md-node --roles=gateway,controller
```

开发和小规模部署可以合并角色；生产规模变大后，可以把 controller 和 gateway 分开部署。代码上保持职责分层，部署上保持灵活。

## 数据面

数据面链路是高频路径：

```text
source -> adapter -> decoder -> field mapper -> normalizer -> book/sequence -> publisher
```

这条链路要避免阻塞、避免不必要分配、避免控制面依赖、避免把日志和指标导出器放进热回调。

## 控制面

控制面只处理低频元数据：

- 哪个 gateway 负责哪个 source/shard。
- 哪个节点是 primary，哪个节点是 standby。
- lease 是否有效。
- 配置版本是多少。
- 切主后新的 publisher epoch 是多少。

控制面可以基于静态 YAML、Kubernetes、etcd、Consul 或自有 controller 实现。无论如何，它不应该参与每条行情消息的发布。

## 高频复用边界

低延迟或高频场景可以复用：

- `md-core`：核心事件模型。
- 轻量 `md-refdata`
- `md-book`：L1/L2/L3 盘口构建模块。
- 部分 `md-codecs`
- 部分 feed decoder
- `md-runtime` 低延迟运行时钩子

不应该依赖：

- NATS/Kafka 等中频分发层。
- 服务 supervisor。
- 控制面 watch/lease。
- 阻塞日志、阻塞指标、数据库客户端。
- 动态配置系统。

## 依赖方向

推荐依赖方向：

```text
md-core
  <- md-refdata
  <- md-book
  <- md-codecs
  <- md-adapters
  <- md-transport
  <- md-service
  <- md-cluster
  <- md-client
  <- apps
```

禁止让 `md-core` 反向依赖外层模块。
