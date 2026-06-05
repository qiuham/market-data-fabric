# Raw 行情采集与录制

第一阶段先实现 raw 行情采集，目标是把交易所或供应商推来的原始报文稳定保存下来。这样即使 refdata、字段转换和标准事件还没有完成，也能先积累真实样本，后续再用这些样本开发 mapper、回放和测试。

## 为什么先做 raw

raw 模式不需要先知道 `instrument_id`、价格精度、数量精度，也不需要把每条消息解析成 `Trade` 或 `BookDelta`。它只做三件事：

```text
收到外部消息
  -> 记录一层很薄的本地 envelope
  -> 原样保存 payload bytes
```

这里的 payload bytes 可以是 JSON 文本、SBE 二进制、供应商 SDK 回调里的原始字段快照，或者后续的专线二进制报文。

## envelope 放什么

raw envelope 只能放“不读 payload 也能知道”的信息：

- `source_id`：数据源，例如 Binance Spot。
- `connection_id`：本进程内的连接编号。
- `stream_id`：如果连接或订阅层已经知道 stream，就写入；否则为 0。
- `capture_seq`：本地采集递增序号。
- `recv_ts_ns`：本机收到消息时的时间戳。
- `protocol`：WebSocket、TCP、UDP、SDK 等。
- `payload_codec`：JSON 文本、SBE、二进制等。
- `compression`：是否压缩。
- `message_kind`：data、heartbeat、subscribe ack、error 等。
- `flags`：一些轻量标记，例如 stream 是否不用解析 payload 就已知。

raw envelope 不放 `instrument_id`、交易所发送时间、交易所序号、买卖方向、价格、数量。这些字段通常必须解析 payload 才能知道，属于 normalized 阶段。

## 文件格式

当前实现是固定二进制文件格式：

```text
RawFileHeader
RawFrameHeader + payload bytes
RawFrameHeader + payload bytes
RawFrameHeader + payload bytes
...
```

相关代码：

- `libs/md-core/include/md/core/raw.hpp`
- `libs/md-replay/include/md/replay/raw_file.hpp`
- `libs/md-replay/include/md/replay/raw_writer.hpp`
- `libs/md-replay/include/md/replay/raw_reader.hpp`

`RawFileHeader` 和 `RawFrameHeader` 只使用固定宽度整数，不放 `std::string`、`std::vector`、指针或 `bool`。文件头包含 endian check，reader 可以识别字节序是否匹配。这样文件更稳定，也方便未来用其他语言写 reader。

## 100 MiB 限制

开发配置默认限制 raw 录制总量为 100 MiB：

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

raw 文件第一版不用 Protobuf，因为这里追求最少依赖和最少 CPU 开销。Protobuf 更适合后续跨语言传输，例如 NATS/Kafka payload，或者需要兼容多语言消费者时。

也就是说：

```text
raw 本地落盘
  优先固定二进制 header + 原始 payload

NATS / Kafka / 跨语言消费
  后续可以用 Protobuf 或 FlatBuffers 做稳定 schema
```

## 热路径注意点

即使是 raw_only，也不要在 WebSocket 回调或 SDK 回调里直接写文件。正确结构应该是：

```text
网络回调 / SDK 回调
  -> 复制到进程内 bounded ring buffer
  -> raw recorder thread 写文件
```

本次先实现 raw 文件格式和 writer/reader。下一步应该补 `md-runtime` 里的 SPSC ring buffer，把文件 IO 从行情接收线程中隔离出去。
