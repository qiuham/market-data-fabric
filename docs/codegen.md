# 构建期 Codegen

`tools/codegen` 只作为后续扩展预留。当前阶段先手写 Binance mapper，把真实 decoder、mapper、fixture 形态跑稳，再决定 IR 和生成规则。

## 原则

- codegen 只在构建期或生成步骤运行。
- 运行时不解释 YAML / JSON / IR 映射文件。
- 生成物必须是普通 C++，编译进具体 provider mapper。
- 热路径 mapper 不反射、不动态查字段、不堆分配、不抛异常。
- 复杂交易语义保留手写，并用 fixture / golden test 覆盖。

## 适合生成

- provider raw message struct。
- 消息类型和字段常量。
- 简单 enum / 字段拷贝骨架。
- mapper 函数骨架。
- mapper 测试骨架。
- `field_mapping.md` 这类人读文档。

## 不适合生成

- sequence gap recovery。
- snapshot + delta 对齐。
- Lv3 订单状态机。
- 交易日 / 夜盘 / 时区特殊逻辑。
- 供应商异常值和 bug workaround。
- 性能关键 order book 更新逻辑。

## 推荐链路

```text
external API docs / stable IR
  -> tools/codegen
  -> generated C++
  -> provider mapper
  -> runtime C++ mapper
```

运行时仍然是：

```text
external message -> decoder -> field mapper -> trading-core event
```

## 当前不做

当前不实现 IR、模板引擎或生成器。等第一个真实 mapper 稳定后，再补：

1. `MappingContext`。
2. `ProviderCapabilities`。
3. provider fixture / golden tests。
4. codegen 输入格式。
