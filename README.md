# Market Data Fabric 行情数据平台

`market-data-fabric` 是一个面向量化系统的 C++ 行情数据平台骨架，目标是把多市场、多供应商的行情接入统一成稳定的内部事件模型，并通过可插拔传输层分发给策略、记录器、回放服务和下游分析系统。

这个项目希望同时覆盖：

- 加密货币行情：现货、合约、期权等。
- A 股行情：券商柜台、行情供应商、Level-1/Level-2 等。
- 美股行情：券商 API、第三方行情、交易所/授权数据源等。
- 期货行情：CTP、易盛、柜台 SDK、交易所或供应商专线等。

核心设计原则是：行情接入、字段转换、分发方式、服务治理、消费端接口互相解耦。中频系统可以通过统一服务发布行情；未来低延迟或高频场景可以复用干净的核心库和部分解码/book 组件。

## 设计目标

- 将不同外部行情源标准化为统一的内部事件模型。
- 统一支持 Lv3 订单级事件模型：`OrderEvent` 和 `Execution`。
- 保持 `md-core` 不依赖任何供应商 SDK、消息队列、数据库或服务框架。
- 将供应商字段转换放在各自 adapter 目录，避免污染服务层。
- 支持 NATS、Kafka、共享内存、TCP、UDP 组播、文件等多种分发和落盘方式。
- 通过 `md-client` 把策略消费行情的方式透明化，降低策略侧重复代码。
- 通过 `md-cluster` 预留分布式部署能力，包括节点注册、租约、分片、主备切换和发布者 epoch。
- 通过 `md-runtime` 提前规划 CPU 绑核、线程优先级、计时器、内存预分配等低延迟运行时能力。
- 让普通中频部署和未来高频复用共用基础模型，但不让高频热路径依赖控制面或消息中间件。

## 仓库结构

```text
apps/
  md-node/              统一进程入口，支持 gateway/controller 等角色。

libs/
  md-core/              核心事件类型、ID、时间戳、序号、sink 接口。
  md-refdata/           品种、交易所、资产类型、交易时段、精度、symbol 映射。
  md-book/              L1/L2/L3 MBO 盘口构建和校验。
  md-codecs/            通用协议解析辅助工具。
  md-adapters/          外部行情源 adapter 和供应商字段转换。
  md-transport/         NATS、Kafka、共享内存、TCP、组播、文件等传输层。
  md-service/           gateway 运行时、pipeline 构建、生命周期、健康检查、指标。
  md-cluster/           分布式 assignment、lease、failover、publisher epoch、节点注册。
  md-runtime/           CPU 绑核、调度策略、计时器、内存 profile、低延迟运行时钩子。
  md-replay/            原始数据和标准化事件的录制与回放。
  md-client/            策略和下游服务使用的消费端库。

schemas/                网络传输、落盘、跨语言消费使用的消息 schema。
configs/                开发、生产和参考数据配置示例。
docs/                   架构、部署、字段转换、数据模式、高频准备度等文档。
tests/                  单元、集成、回放、fuzz、benchmark 测试。
scripts/                开发、CI、运维脚本。
tools/                  构建期工具预留目录，例如 md-codegen。
third_party/            明确需要随仓库管理的第三方依赖预留目录。
```

## 运行角色

项目采用类似 Kafka 的角色模型：代码职责分层，但部署形态可以合并或拆分。

```text
md-node --roles=gateway
md-node --roles=controller
md-node --roles=gateway,controller
```

早期可以只用静态 YAML 运行 gateway；后续可以接入 Kubernetes Lease、etcd、Consul，或者启用独立 controller 角色。无论控制面如何部署，实时行情 tick 都不应经过控制面。

## 数据面链路

```text
外部行情源 / 供应商 SDK
  -> adapter client
  -> decoder
  -> provider field mapper
  -> md-core event
  -> sequence checker / book builder
  -> transport publisher
  -> NATS / Kafka / shm / TCP / multicast / file
```

数据面只负责接收、解析、标准化、校验、构建盘口、发布和录制。它应该尽量简单、低抖动、少阻塞。

## 数据模式

gateway 支持三种数据模式，方便先做 raw data 获取，再逐步接入标准化转换：

```text
raw_only         只采集/录制供应商原始数据，不输出标准化事件。
normalized_only  只输出标准化事件，不额外保留原始数据。
dual             同时保留 raw 数据和标准化事件，长期生产更推荐。
```

详见 `docs/data-modes.md` 和 `docs/raw-capture.md`。

## 控制面职责

逻辑控制面负责低频元数据，不负责每条行情：

- 节点注册和心跳。
- source、symbol、shard 的 assignment。
- 主备 lease 和故障切换。
- publisher epoch，用于下游识别切主。
- 配置版本和滚动发布状态。

第一版可以使用静态配置；规模上来后再接入外部协调系统或启用 controller 角色。

## Adapter 字段转换

供应商字段转换放在 adapter 自己的目录，不放在 `md-service`：

```text
libs/md-adapters/{asset_class}/{provider}/
  src/*_field_mapper.cpp
  src/*_normalizer.cpp
  docs/field_mapping.md
  tests/*_field_mapper_test.cpp
  fixtures/
```

供应商 API 文档由外部文档项目维护。本仓库不维护手写 mapping YAML，字段转换以 C++ mapper 和 tests 为准。标准事件字段只在 `md-core` 定义。symbol、价格精度、数量精度、交易日、交易时段等上下文由 `md-refdata` 提供。Lv3 事件使用 `OrderEvent` 和 `Execution`，不允许 adapter 用 L2 聚合盘口伪造订单级数据。

## 消费端库

`md-client` 是给策略和下游服务使用的消费端库。它不包含行情源 adapter，也不包含 NATS/Kafka broker 本身。它只是封装传输客户端、schema 解码、topic/subject 规则、事件回调、回放/live 切换等逻辑。

```text
md-gateway -> broker/transport -> strategy + md-client
```

如果消费者愿意直接使用 NATS/Kafka 原生客户端，也可以绕过 `md-client`，只依赖公开的 schema 和 topic 规范。

## Codegen

项目预留了构建期 codegen 位置：

```text
tools/md-codegen/
docs/codegen.md
cmake/MdfCodegen.cmake
```

codegen 只用于构建期生成 C++ 样板，不在行情热路径解释配置文件。供应商 API 文档由外部文档项目维护，本仓库后续只消费它产出的稳定 IR。

## 初始构建

当前项目只是骨架。装好 CMake 后可以执行：

```bash
cmake -S . -B build
cmake --build build
./build/md-node --roles=gateway --config configs/dev/mock.yaml
```

## 路线图

1. 稳定 `md-core` 事件模型、Lv3 订单级模型、参考数据模型和 replay 文件格式。
2. 实现统一 message log、mock/replay adapter 和一个 crypto adapter。
3. 补 Binance Spot WebSocket live client，并把 `MessageLogWriter` 接入 gateway pipeline。
4. 增加 NATS 发布和 `md-client-nats`。
5. 增加 CTP、IB 等 adapter，并通过平台/依赖开关隔离闭源 SDK。
6. 实现 `md-runtime` 的 Linux 运行时能力：CPU 绑核、clock reader、memory lock、线程 profile。
7. 增加共享内存传输和延迟 benchmark。
8. 增加 `md-cluster` 的 lease/assignment 支持。
9. 在 IDC 网络能力确认后，再增加 UDP 组播传输。
10. 等第一个真实 adapter 和外部 API 文档 IR 稳定后，再实现 `tools/md-codegen`。
