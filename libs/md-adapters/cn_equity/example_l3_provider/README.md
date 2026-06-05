# A 股示例 Lv3 供应商

这个目录只是示例 provider，用来说明 A 股逐笔委托和逐笔成交如何映射到统一 Lv3 模型。

真实项目中应为具体供应商创建独立目录，例如：

```text
libs/md-adapters/cn_equity/{provider}/mapping/order_event.yaml
libs/md-adapters/cn_equity/{provider}/mapping/execution.yaml
```

不同 A 股供应商字段差异很大，不能把这里的示例 mapping 当成通用实现。
