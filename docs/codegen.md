# 构建期 Codegen 规划

本项目后续可以接入构建期 codegen，用来把外部 API 文档项目产出的接口定义转换成 C++ 代码。codegen 的目标是提升扩展性，同时保持运行时性能接近手写 C++。

## 核心原则

- codegen 只在构建期或生成步骤运行，不在行情热路径运行。
- 运行时不能读取、查询或解释 YAML/JSON/IR 映射规则。
- 生成结果必须是普通 C++ 代码，编译进具体 provider adapter。
- 热路径 mapper 不使用反射、不做动态字段查找、不做堆分配、不抛异常。
- 复杂业务语义保留手写 C++，不要强行全部生成。

推荐链路：

```text
外部 API 文档项目
  -> 稳定 IR / 生成输入
  -> tools/md-codegen
  -> generated/*.hpp / generated/*.cpp
  -> 编译进 provider adapter
  -> 运行时执行普通 C++ mapper
```

运行时链路保持为：

```text
外部报文 -> C++ decoder -> C++ field mapper -> md-core event
```

## 适合生成的内容

- provider raw type。
- 字段常量和消息类型枚举。
- 简单字段拷贝代码。
- enum 映射表。
- mapper 函数骨架。
- fixed-point 转换调用骨架。
- timestamp 转换调用骨架。
- mapper 单元测试骨架。
- `docs/field_mapping.md` 这类人看的字段说明。

## 不建议生成的内容

这些逻辑通常依赖交易语义和供应商行为，建议手写并测试：

- 交易日和夜盘处理。
- 复杂时间戳组合。
- sequence gap recovery。
- snapshot + delta 对齐。
- Lv3 订单状态机。
- 异常值、空值、哨兵值处理。
- 供应商 bug workaround。
- 性能关键的 order book 更新逻辑。

## 目录约定

codegen 工具预留在：

```text
tools/md-codegen/
  README.md
  templates/
```

具体 provider 的生成物建议放在：

```text
libs/md-adapters/{asset_class}/{provider}/generated/
  *_raw.hpp
  *_mapper.generated.hpp
  *_mapper.generated.cpp
```

手写语义代码仍放在：

```text
libs/md-adapters/{asset_class}/{provider}/src/
  *_decoder.cpp
  *_field_mapper.cpp
  *_normalizer.cpp
```

测试和 fixture 放在：

```text
libs/md-adapters/{asset_class}/{provider}/tests/
libs/md-adapters/{asset_class}/{provider}/fixtures/
```

## 生成物是否提交

早期建议提交生成物，并在 CI 里校验生成物没有漂移：

```text
1. 运行 codegen
2. 检查 git diff
3. 如果生成物和仓库不一致，CI 失败
```

这样使用者不需要先安装完整 codegen 环境也能编译项目，同时仍能发现生成物和输入定义不一致。

## 与外部 API 文档项目的关系

供应商 API 文档、字段说明、枚举说明和接口版本由外部文档项目维护。本仓库不手写 mapping 文件。

本仓库后续只消费外部文档项目产物，例如：

```text
provider_api.ir.json
provider_api.ir.bin
provider_api.generated.json
```

具体格式等第一个真实 adapter 接入后再确定，避免过早设计一个不贴合真实接口的 IR。

## 性能要求

生成的 mapper 应满足：

- `noexcept` 优先。
- 不做堆分配。
- 不使用运行时反射。
- 不按字符串查字段。
- 不在每条行情中访问配置文件。
- 简单转换可 `inline`。
- 热路径依赖通过 `MappingContext` 传入。

示意：

```cpp
inline bool map_depth_update(
    const ProviderDepthUpdateRaw& src,
    const MappingContext& ctx,
    md::core::BookDelta& out) noexcept {
    out.header.instrument_id = ctx.symbols.lookup(src.symbol);
    out.header.seq = src.sequence;
    out.price = ctx.price.to_fixed(src.price);
    out.quantity = ctx.quantity.to_fixed(src.quantity);
    return true;
}
```

## 当前阶段

当前只预留目录和设计边界，不实现 codegen。建议先完成：

1. `MappingContext`。
2. `AdapterCapabilities`。
3. 一个真实 provider adapter。
4. mapper fixture/golden tests。

等真实 mapper 形态稳定后，再实现 `tools/md-codegen`。
