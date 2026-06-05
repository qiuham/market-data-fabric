# 数据模式：raw、normalized、dual

为了降低早期开发成本，gateway 应支持先做原始数据采集，再逐步接入标准化转换。

核心思路：

```text
raw data 获取和录制
  可以先做，不依赖 refdata 和字段转换完整度

normalized data 标准化
  后续逐步做，用于策略、统一分发、回放和跨市场分析
```

## 三种模式

### raw_only

只采集和发布/录制原始数据，不做内部标准化。

```text
外部 feed / SDK callback
  -> raw recorder / raw publisher
```

适合：

- 先把供应商数据拉下来。
- 保存真实原始报文，方便后面开发 mapper。
- refdata 尚未完善。
- 字段语义还在确认。
- 先做数据归档和回放素材。

限制：

- 策略不能直接统一消费。
- 不输出 `Trade`、`BookDelta`、`OrderEvent` 等标准事件。
- 不做统一 symbol、价格精度、时间戳标准化。

### normalized_only

只输出标准化事件，不额外保留原始数据。

```text
外部 feed / SDK callback
  -> decoder
  -> field mapper
  -> md-core event
  -> publisher
```

适合：

- mapper 已经稳定。
- refdata 已经可用。
- 关注实时策略消费。

风险：

- 如果后续发现 mapper 有 bug，没有 raw 数据会比较难追溯。

### dual

同时保留 raw 数据和标准化事件。

```text
外部 feed / SDK callback
  -> raw recorder / raw publisher
  -> decoder
  -> field mapper
  -> md-core event
  -> normalized publisher / recorder
```

适合生产和调试，是长期推荐模式。

优点：

- 策略消费标准化事件。
- 问题排查时可以回看 raw 数据。
- mapper 变更后可以用 raw 数据重新生成 normalized 数据。

## 配置示例

```yaml
pipeline:
  mode: dual              # raw_only | normalized_only | dual

raw_data:
  enabled: true
  record: true
  publish: false
  format: provider_native
  path: ./data/raw/mock
  storage:
    max_total_bytes: 104857600  # 100 MiB
    limit_policy: stop_recording  # stop_recording | drop_new | rotate_delete_oldest

normalization:
  enabled: true
  require_refdata: true
  unknown_instrument_policy: reject  # reject | raw_only | allow_pending

normalized_data:
  enabled: true
  record: true
  publish: true
  path: ./data/normalized/mock
```


## raw 录制容量限制

第一阶段先把 raw 数据总量限制为 100 MiB，避免开发环境无限写盘。

```yaml
raw_data:
  storage:
    max_total_bytes: 104857600  # 100 MiB
    limit_policy: stop_recording
```

`limit_policy` 含义：

```text
stop_recording
  达到上限后停止继续录制 raw 数据，但 gateway 可以继续运行并上报 recorder_limited 状态。适合开发环境。

drop_new
  达到上限后丢弃新 message frame。适合只想保留最早样本的场景。

rotate_delete_oldest
  达到上限后删除最老文件，为新数据腾空间。适合长期运行，但第一版可以先不实现。
```

当前开发配置使用 `stop_recording`，上限为 100 MiB。`MessageLogWriter` 已把这个限制作为 bounded storage 的硬约束。

## unknown_instrument_policy

当 adapter 收到一个 refdata 中没有的标的时，需要明确策略。

```text
reject
  丢弃标准化事件，记录错误和指标。适合严格生产。

raw_only
  保留 raw 数据，不输出 normalized 事件。适合早期开发和标的自动发现。

allow_pending
  临时生成 pending instrument，占位处理。需要后续 refdata registry 支持，不建议第一版使用。
```

## 性能边界

即使是 `raw_only`，也不要在 SDK 回调里直接写文件或发 NATS。

推荐：

```text
SDK callback
  -> ingress ring
  -> raw recorder thread / publisher thread
```

也就是说，raw 数据路径同样要有 bounded queue / ring buffer 隔离，避免文件 IO、网络 IO 或 broker 抖动卡住行情接收。

## 与 refdata 的关系

`raw_only` 可以绕开 refdata，适合先采集数据。

`normalized_only` 和 `dual` 中的 normalized 分支需要 refdata，因为必须把外部 symbol 转成内部 `instrument_id`，并进行价格、数量、时间戳标准化。

## 与 replay 的关系

raw 数据和 normalized 数据都可以回放，但用途不同：

```text
raw replay
  用来重跑 decoder / mapper / normalizer，验证转换逻辑。

normalized replay
  用来给策略提供统一历史行情输入。
```

因此建议生产尽量保留 raw 数据，至少保留一段时间。第一版统一 message log 格式见 `docs/raw-capture.md`。
