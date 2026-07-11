# md-client 消费端库

`client` 是消费端库，给策略和下游服务使用。

它不包含行情源 adapter，也不包含 broker 服务本身。它只封装：

- NATS/Kafka/shared memory 等消费方式。
- schema 解码。
- topic/subject 命名规则。
- 事件回调分发，包括 `OrderEvent` 和 `Execution`。
- symbol/instrument 映射辅助。
- sequence/gap 检查。
- live 消费接口。

消费者也可以不使用 `client`，直接用 NATS/Kafka 原生客户端加 schema 解码。
