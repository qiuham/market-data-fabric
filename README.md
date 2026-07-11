# marketdata

统一行情接入、标准化、回放和服务运行时。所有标准行情事件使用 sibling
`trading-core`，订单簿重建使用 sibling `orderbook`；本仓库不再维护第二套核心
事件或订单簿模型。

## 结构

```text
include/marketdata/
  feed/          原始消息、连接和 feed 描述
  codecs/        JSON、二进制和定点数解析
  adapters/      供应商 decoder 与组合 mapper
  venues/        SSE、SZSE 等交易所语义
  replay/        原始/标准事件日志与确定性回放
  runtime/       SPSC、绑核、时钟和内存辅助
  service/       session、恢复和发布编排
  reference/     行情标准化所需参考数据
  net/           WebSocket 等网络后端
  client/        标准事件消费接口
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
  -> venue normalizer
  -> trading-core event
  -> router
  -> orderbook / journal / publisher
```

供应商层解释字段名、SDK 枚举和供应商传输；市场层解释交易所订单、成交、状态和
序号语义。两层通过 header-only facade 在同一调用栈内组合，不增加线程、队列、
虚函数或动态分配。

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

主要程序：

```text
marketdata-gateway       实时行情网关
book-validate            沪深逐笔重建离线验证
tonglian-mapper-bench    通联 mapper/序号热路径基准
```

## 约束

- `trading-core` 是唯一标准事件和基础交易类型来源。
- `orderbook` 是唯一订单簿和模拟撮合实现。
- provider 特殊字段不进入 trading-core 或 orderbook。
- 快照只用于离线验证，不修正生产 MBO。
- 热路径默认无锁、无动态分配、无逐 tick 日志。
- README、设计说明、必要注释、日志和用户提示统一使用中文。

沪深逐笔重建结论与使用方法见 `docs/cn-stock-book-rebuild.md` 和
`tools/book-validate/README.md`。
