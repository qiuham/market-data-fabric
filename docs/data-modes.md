# 数据模式

gateway 支持三种数据模式，用来区分接入阶段和生产阶段的职责。

## raw_only

只接收供应商原始消息，不输出标准化事件。

```text
external feed -> provider -> RawProviderMessage
```

适合：

- 新 provider 初期验证连接、订阅和限流。
- 收集真实样本，辅助开发 decoder / mapper。
- refdata 和字段语义还没有稳定时降低接入成本。

限制：

- 下游策略不能统一消费。
- 不做 symbol、价格精度、数量精度和时间戳标准化。
- 不输出 `Trade`、`Quote`、`BookUpdate` 等标准事件。

## normalized_only

只输出标准化事件，不额外保留 raw payload。

```text
external feed -> decoder -> mapper -> trading-core event
```

适合 mapper 和 refdata 已经稳定、只关注实时消费的链路。

风险是排查 mapper bug 时缺少原始消息上下文，因此生产初期不建议只跑这一种模式。

## dual

同时保留 raw 分支和 normalized 分支。

```text
external feed
  -> raw branch
  -> decoder -> mapper -> trading-core event
```

长期生产更推荐 `dual`：策略消费标准化事件，排障和回归测试可以回看 raw 样本。

## 当前优先级

当前阶段先实现：

1. live raw 接入。
2. provider mapper。
3. normalized event pipeline。
4. publisher / client。

历史回放和持久化日志后续再补，不作为当前主线。

## unknown instrument 策略

normalized 分支必须把外部 symbol 映射到内部 `instrument_id`。找不到标的时需要显式策略：

```text
reject       丢弃 normalized event，记录错误和指标。
raw_only     只保留 raw 分支，不输出 normalized event。
allow_pending 临时占位，依赖后续 refdata registry，不建议第一版使用。
```

## 热路径边界

无论哪种模式，都不要在网络回调或 SDK 回调里做阻塞 IO、复杂日志或动态配置查询。推荐结构：

```text
ingress callback -> bounded queue -> parser / mapper / publisher worker
```
