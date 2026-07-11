# 架构说明

## 唯一核心

标准事件、ID、序号和时间戳统一来自 `trading-core`。本仓库只保存行情接入所需的
原始 feed envelope、协议解析、供应商适配、市场语义、回放和运行时组件。
`orderbook` 直接消费 trading-core 事件，不依赖任何供应商。

## 两级标准化

```text
原始消息
  -> Provider Decoder：字段、编码、SDK、供应商 session
  -> Provider Mapper：供应商字段映射
  -> Market Mapper：交易所订单、成交、状态、序号语义
  -> trading-core Event
```

例如通联、TORA 和 XTP 的沪市字段不同，但都应转换为同一种 SSE native view，再
复用 SSE mapper。通联的字符 `S` 只是供应商表达；“状态消息推进交易所序号、
但不修改订单簿”才是可复用的市场语义。

概念上分层，运行时不分线程。组合 mapper 使用模板或直接函数调用，编译器可以
内联 decoder、mapper 和 sink；不得为了目录分层增加队列、虚调用或事件堆分配。

## 单一公共库

所有公共头文件位于 `include/marketdata`，CMake 只导出 `marketdata` 目标。内部
目录表达职责，不伪装成多个独立子仓库。

```text
feed -> providers -> markets -> trading-core event
                               |-> orderbook
                               |-> replay journal
                               |-> publisher
```

`runtime` 和 `net` 提供实现能力，不能引入交易所字段语义。`service` 只组装连接、
恢复、journal 和 publisher，不解析 provider payload。

目录与逻辑边界一致：

```text
providers/cn/stock/tonglian   通联字段、文件与序号包装
providers/crypto/binance      Binance协议与字段
markets/cn/sse                上交所公共行情语义
markets/cn/szse               深交所公共行情语义
```

不保留只有声明、没有实现或调用方的“未来接口”。参考数据客户端、CPU/内存调优等
能力在真正接入runtime时再以可测试实现加入，避免目录结构虚报能力。

## 连续性与恢复

连续性可能同时存在两层：供应商 packet/session 序号和交易所业务序号。decoder
负责暴露原始 cursor，market mapper 负责其理解的交易所连续性。两者对外统一为
`ContinuityObservation`。

发现 gap 或倒退后必须进入粘滞 `Stale`，停止向订单簿和 publisher 发送后续事件。
只有 checkpoint/replay 已将所有下游状态恢复到明确序号，service 才能完成恢复。
盘中收到的任意第一条消息不能自动成为可信基线。

## 回放与回溯

生产订单簿和离线验证消费同一种 trading-core 标准事件。长期回溯采用旁路的顺序
事件 journal，行情线程只向预分配 SPSC 写入固定大小事件；文件写入、压缩和索引在
异步线程完成。

```text
normalized event
  |-> orderbook
  `-> SPSC -> journal writer -> lifecycle index
```

同级仓库 `trading-evaluation` 中的 `book-validate` 是验证前端，不是长期存储
实现。订单和成交查询读取公共 journal/index，避免再次扫描供应商 Parquet 或维护
另一套生命周期逻辑。
