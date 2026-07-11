# 行情消息 Schema v1

这里预留线上消息格式定义，包括 L1/L2/L3 行情事件。

`libs/feed` 定义的是 C++ 进程内事件结构；`schemas/md/v1` 定义的是跨进程、跨语言、网络传输、文件落盘时使用的 wire schema。

后续可以选择一种或多种格式：

- FlatBuffers：适合低开销跨语言消息。
- Cap'n Proto：适合低拷贝消息访问。
- SBE：适合低延迟金融消息编码。
- Protobuf：生态成熟，适合服务化场景。

保留 `v1` 是为了明确 schema 版本。以后如果出现破坏性字段变化，可以新增 `schemas/md/v2/`。Lv3 wire schema 应覆盖 `OrderEvent` 和 `Execution`。
