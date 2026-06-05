# Adapter 字段转换

供应商字段转换必须放在每个 adapter 自己的目录，不放在 `md-service`。原因是不同供应商的字段含义、时间语义、盘口模型、序号规则、异常值处理和重连行为都不同。Lv3 字段转换同样遵循这个规则。

## 目录位置

字段转换必须落在具体 provider 目录下：

```text
libs/md-adapters/{asset_class}/{provider}/
  include/md/adapters/{provider}/
    *_field_mapper.hpp
    *_normalizer.hpp
  src/
    *_field_mapper.cpp
    *_normalizer.cpp
  docs/
    field_mapping.md
    api_version.md
  tests/
    *_field_mapper_test.cpp
  fixtures/
    raw/
    normalized/
```

## 转换流程

```text
vendor bytes / SDK callback
  -> provider raw type
  -> provider field mapper
  -> md::core event
```

例如：

```text
Binance WebSocket JSON
  -> BinanceDepthUpdateRaw
  -> BinanceFieldMapper
  -> md::core::BookDelta
```

```text
CThostFtdcDepthMarketDataField
  -> CtpFieldMapper
  -> md::core::BookSnapshot
```

## API 文档来源

供应商 API 文档、字段说明、枚举说明和接口版本由外部文档项目维护。本仓库不维护手写 mapping YAML，避免出现两份 source of truth。

本仓库只保留：

- provider adapter 代码。
- C++ field mapper / normalizer。
- mapper 单元测试。
- 必要的 fixture。
- 可由外部文档项目生成或同步的 `docs/field_mapping.md`。

## 运行时边界

不能在行情热路径上解释执行外部映射文件。

生产 adapter 的字段转换应由 C++ mapper 实现，或者由 `tools/md-codegen` 在构建期从外部文档项目生成 C++ 代码。运行时链路应该是：

```text
外部报文 -> C++ decoder -> C++ field mapper -> md-core event
```

不应该是：

```text
外部报文 -> 读取/查询/解释 YAML -> md-core event
```

## 映射上下文

字段转换通常需要上下文，例如：

- `source_id`：行情源 ID。
- `venue_id`：交易场所 ID。
- `symbol_map`：外部 symbol 到内部 instrument id 的映射。
- `precision_map`：价格和数量精度映射。
- `trading_calendar`：交易日历。
- `clock`：本机时间戳来源。

服务层可以创建上下文，但不应该理解供应商字段细节。

## Lv3 转换要求

如果数据源提供订单级事件，应转换为：

- `OrderEvent`：逐笔委托、order add/modify/cancel/delete/replace。
- `Execution`：逐笔成交、order executed、trade match。

如果数据源只提供价位聚合盘口，只能转换为 `BookDelta` 或 `BookSnapshot`，不能伪造 `OrderEvent`。

## 不要集中放 mapper

不建议做一个全局 `md-mappers` 目录集中放所有供应商转换逻辑。字段转换应该和具体 provider 的 adapter 代码、测试、文档放在一起，便于维护和升级。

如果多个 provider 后续确实有可复用逻辑，应提取成普通 C++ helper，并通过测试证明不会把供应商语义混在一起；具体字段转换仍然保留在 provider adapter 内。
