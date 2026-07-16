# marketdata

统一行情接入、标准化、回放和服务运行时。所有标准行情事件使用 sibling
`trading-core`，订单簿重建使用 sibling `orderbook`；本仓库不再维护第二套核心
事件或订单簿模型。

## 结构

```text
include/marketdata/
  feed/          原始消息、连接和 feed 描述
  codecs/        JSON、二进制和定点数解析
  providers/      按市场/provider组织的decoder与组合mapper
  markets/        SSE、SZSE 等交易所语义
  replay/        原始/标准事件日志与确定性回放
  runtime/       当前实际使用的SPSC与日志辅助
  service/       session、恢复和发布编排
  net/           WebSocket 等网络后端
apps/gateway/    行情网关程序
tools/           离线验证、基准和代码生成
tests/           组件与真实语义回归测试
docs/            架构、组件和供应商说明
```

仓库只导出一个 CMake 接口目标：

```cmake
target_link_libraries(your_target PRIVATE marketdata)
```

## 数据路径

```text
provider raw message
  -> provider decoder
  -> provider mapper
       -> SequenceGate（过滤前）
  -> market mapper
  -> trading-core event
  -> router
  -> orderbook / journal / publisher
```

供应商层解释字段名、SDK 枚举和供应商传输；市场层解释交易所订单、成交、状态和
序号语义。两层通过 header-only facade 在同一调用栈内组合，不增加线程、队列、
虚函数或动态分配。

provider路径固定为`providers/<market>/<provider>`，例如
`providers/cn/stock/tonglian`、`providers/cn/stock/csmar`和
`providers/crypto/binance`。同市场多个provider复用
`markets/<market>/<venue>`，不能复制一份交易所规则到每个provider中。

`mappers/SequenceGate`由provider mapper持有，不增加运行层级。实时完整partition使用
Contiguous模式检查严格连续；希施玛这类已经按单票拆分的离线文件使用Monotonic模式，
只拒绝重复和回退，不能把其他证券造成的正常跳号误判成丢包。正常路径无锁、无分配。

希施玛沪市ORDER与TRANSACTION按同一`RecID`合并后复用沪市市场mapper。集合竞价
中性成交扣减双方事实委托；连续竞价B/S成交只扣减被动方，因为主动单可能直接成交
而没有独立委托记录。该规则属于沪市语义，不在离线验证工具中重复实现。

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

主要程序：

```text
marketdata-gateway       实时行情网关
tonglian-mapper-bench    通联 mapper/序号热路径基准
```

## 约束

- `trading-core` 是唯一标准事件和基础交易类型来源。
- `orderbook` 是唯一订单簿实现；模拟撮合由 `venue-sim` 负责。
- provider 特殊字段不进入 trading-core 或 orderbook。
- 快照只用于离线验证，不修正生产 MBO。
- 热路径默认无锁、无动态分配、无逐 tick 日志。
- README、设计说明、必要注释、日志和用户提示统一使用中文。

沪深逐笔重建结论见 `docs/cn-stock-book-rebuild.md`；离线验证和回溯工具已迁移到
同级仓库 `trading-evaluation`，避免生产行情仓库承担观察者职责。
