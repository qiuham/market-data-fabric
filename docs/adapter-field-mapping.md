# Adapter 字段映射

供应商字段映射必须放在每个 adapter 自己的目录，不放在 `md-service`。原因是不同供应商的字段含义、时间语义、盘口模型、序号规则、异常值处理和重连行为都不同。

## 目录位置

```text
libs/md-adapters/{asset_class}/{provider}/
  include/md/adapters/{provider}/
    *_field_mapper.hpp
    *_normalizer.hpp
  src/
    *_field_mapper.cpp
    *_normalizer.cpp
  schema/
    *.mapping.yaml
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

## mapping.yaml 映射文件

`schema/*.mapping.yaml` 用来记录外部字段到内部字段的映射关系。它的目的包括：

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

## 不要集中放 mapper

不建议做一个全局 `md-mappers` 目录集中放所有供应商转换逻辑。字段映射应该和 adapter 代码、测试、文档放在一起，便于维护和升级。
