# Adapter 字段映射

供应商字段映射必须放在每个 adapter 自己的目录，不放在 `md-service`。原因是不同供应商的字段含义、时间语义、盘口模型、序号规则、异常值处理和重连行为都不同。Lv3 字段映射同样遵循这个规则。

## 目录位置

字段映射必须落在具体 provider 目录下：

```text
libs/md-adapters/{asset_class}/{provider}/
  include/md/adapters/{provider}/
    *_field_mapper.hpp
    *_normalizer.hpp
  src/
    *_field_mapper.cpp
    *_normalizer.cpp
  mapping/
    *.yaml
  docs/
    field_mapping.md
  tests/
    *_field_mapper_test.cpp
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

## 映射上下文（MappingContext）

字段映射通常需要上下文，例如：

- `source_id`：行情源 ID。
- `venue_id`：交易场所 ID。
- `symbol_map`：外部 symbol 到内部 instrument id 的映射。
- `precision_map`：价格和数量精度映射。
- `trading_calendar`：交易日历。
- `clock`：本机时间戳来源。

服务层可以创建上下文，但不应该理解供应商字段细节。

## mapping/*.yaml 映射文件

`mapping/*.yaml` 用来记录外部字段到内部字段的映射关系。它的目的包括：

- 给人查看。
- 给测试生成器使用。
- 给后续文档生成使用。
- 帮助检查供应商 SDK 升级后的字段变化。

示例：

```yaml
provider: ctp
source_message: CThostFtdcDepthMarketDataField
target_event: BookSnapshot
fields:
  InstrumentID:
    target: header.instrument_id
    transform: symbol_map.lookup
  LastPrice:
    target: last_price
    transform: price_to_fixed
```

## mapping/*.yaml 的运行时边界

`mapping/*.yaml` 不在行情热路径运行时解析。它只用于文档、review、测试用例生成或代码生成输入。

生产 adapter 的字段转换应由 C++ mapper 实现，或者由构建期生成 C++ 代码。不能在每条行情消息上读取 YAML、查 YAML 或解释执行 YAML 规则。

## 不设置 common 字段映射目录

不设置通用 `common` 字段映射目录。原因是即使同一个市场，不同供应商的字段名、二进制布局、序号语义、时间字段和异常值处理也可能完全不同。

如果多个 provider 后续确实有可复用工具，应提取成普通 C++ helper，并通过测试证明不会把供应商语义混在一起；字段映射本身仍然保留在具体 provider 的 `mapping/` 目录。

## Lv3 映射要求

如果数据源提供订单级事件，应映射为：

- `OrderEvent`：逐笔委托、order add/modify/cancel/delete/replace。
- `Execution`：逐笔成交、order executed、trade match。

如果数据源只提供价位聚合盘口，只能映射为 `BookDelta` 或 `BookSnapshot`，不能伪造 `OrderEvent`。

## 不要集中放 mapper

不建议做一个全局 `md-mappers` 目录集中放所有供应商转换逻辑。字段映射应该和具体 provider 的 adapter 代码、测试、文档放在一起，便于维护和升级。
