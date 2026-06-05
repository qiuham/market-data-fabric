# crypto 示例 Lv3 供应商

这个目录只是示例 provider，用来说明 crypto 订单级 feed 如何映射到统一 Lv3 模型。

真实项目中应为具体交易所或供应商创建独立目录，例如：

```text
libs/md-adapters/crypto/{provider}/mapping/order_event.yaml
libs/md-adapters/crypto/{provider}/mapping/execution.yaml
```

如果某个交易所只提供 L2 聚合深度，不应复用这里的 Lv3 mapping。
