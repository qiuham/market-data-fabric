# 统一消息 Envelope 与原始报文采集

第一阶段先采集交易所或供应商推来的原始报文，但外层结构不再单独做一套 raw envelope。项目统一使用：

```text
MessageEnvelope + payload
```

区别只在 payload：

```text
原始报文：payload_kind = ProviderMessage，payload_encoding = ProviderJson / ProviderBinary / Sbe
标准事件：payload_kind = Trade / Quote / BookDelta / OrderEvent，payload_encoding = MdStruct / Protobuf
```

这样 raw、mapper 输出、录制、发布可以复用同一套 envelope、队列和落盘格式，避免后续出现多套相似结构。

## 为什么 raw 阶段仍然可以先做

raw 模式不需要先知道 `instrument_id`、价格精度、数量精度，也不需要把每条消息解析成 `Trade` 或 `BookDelta`。它只做三件事：

```text
收到外部消息
  -> 填写不依赖 payload 解析的 MessageEnvelope 字段
  -> 原样保存 payload bytes
```

这里的 payload bytes 可以是 JSON 文本、SBE 二进制、供应商 SDK 回调里的原始字段快照，或者后续的专线二进制报文。

## envelope 怎么填

`MessageEnvelope` 可以同时服务 raw 和 normalized。raw 阶段只填写“不读 payload 也能知道”的信息：

- `payload_kind`：原始供应商报文填 `ProviderMessage`。
- `payload_encoding`：JSON、二进制、SBE 等。
- `source_id`：数据源，例如 Binance Spot。
- `connection_id`：本进程内的连接编号。
- `stream_id`：如果连接或订阅层已经知道 stream，就写入；否则为 0。
- `capture_seq`：本地采集递增序号。
- `recv_ts_ns`：本机收到消息时的时间戳。
- `protocol`：WebSocket、TCP、UDP、SDK 等。
- `compression`：是否压缩。
- `payload_size`：payload 字节数。
- `flags`：一些轻量标记，例如 stream 是否不用解析 payload 就已知。

raw 阶段不知道的字段保持 0，例如 `instrument_id`、`source_ts_ns`、`exchange_seq`。不要为了填这些字段去解析 payload。

## 文件格式

当前实现是固定二进制 message log：

```text
MessageLogFileHeader
MessageLogFrameHeader + payload bytes
MessageLogFrameHeader + payload bytes
MessageLogFrameHeader + payload bytes
...
```

相关代码：

- `libs/md-core/include/md/core/message.hpp`
- `libs/md-replay/include/md/replay/message_log.hpp`

`MessageLogFileHeader` 和 `MessageLogFrameHeader` 只使用固定宽度整数，不放 `std::string`、`std::vector`、指针或 `bool`。文件头包含 endian check，reader 可以识别字节序是否匹配。

## 100 MiB 限制

开发配置默认限制录制总量为 100 MiB：

```yaml
raw_data:
  storage:
    max_total_bytes: 104857600
    limit_policy: stop_recording
```

当前代码已经实现：

- `stop_recording`：达到上限后停止继续写入。
- `drop_new`：达到上限后丢弃新 frame。

`rotate_delete_oldest` 先保留枚举，后续需要多文件分片和索引时再实现。

## 和 Protobuf 的关系

本地 message log 第一版不用 Protobuf，因为这里追求最少依赖和最少 CPU 开销。Protobuf 更适合后续跨语言传输，例如 NATS/Kafka payload，或者需要兼容多语言消费者时。

```text
本地落盘
  优先固定二进制 header + payload bytes

NATS / Kafka / 跨语言消费
  后续可以用 Protobuf 或 FlatBuffers 做稳定 schema
```

## 热路径注意点

即使是 raw_only，也不要在 WebSocket 回调或 SDK 回调里直接写文件。正确结构应该是：

```text
网络回调 / SDK 回调
  -> 复制到进程内 bounded queue
  -> recorder thread 写文件
```

队列本身是通用的，例如 `SpscRing<T>`，可以承载 `ProviderMessageFrame`，也可以承载后续标准化后的 `Trade`、`BookDelta` 等类型。
