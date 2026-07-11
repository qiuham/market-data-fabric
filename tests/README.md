# tests

测试目录按模块分层，避免后续平铺变乱。

当前结构：

```text
tests/adapters/crypto/binance/      Binance feed 和 mapper smoke tests
tests/adapters/cn/stock/tonglian/        通联 A 股逐笔 mapper smoke tests
tests/core/                  md-core 事件模型 smoke tests
tests/net/                   net endpoint / transport smoke tests
tests/runtime/               runtime 队列等运行时 smoke tests
tests/service/               service session / pipeline smoke tests
tests/replay/                replay / MessageLog 预留测试，默认不构建
```

默认 smoke tests：

- `tests/core/core_events_smoke.cpp`：验证统一事件模型和 instrument 模型的基础语义。
- `tests/adapters/crypto/binance/binance_feeds_smoke.cpp`：验证 Binance feed key、connection spec 和 envelope 生成。
- `tests/adapters/crypto/binance/binance_book_ticker_mapper_smoke.cpp`：验证 Binance `bookTicker -> Quote` 字段转换。
- `tests/adapters/crypto/binance/binance_trade_mapper_smoke.cpp`：验证 Binance `trade -> Trade` 字段转换。
- `tests/adapters/cn/stock/tonglian/mapper_smoke.cpp`：验证通联 A 股逐笔字段到 `BookOrder` / `BookTransaction` 的字段转换；本地存在 `../trading-core/include` 时自动加入 CTest。
- `tests/net/websocket_endpoint_smoke.cpp`：验证 `net` WebSocket endpoint 解析。
- `tests/runtime/spsc_ring_smoke.cpp`：验证 SPSC ring 的顺序、满队列和双线程读写。
- `tests/service/feed_session_smoke.cpp`：验证 feed session 重试、停止条件和统计。

运行：

```bash
ctest --test-dir cmake-build-debug --output-on-failure
```

`tests/replay/message_log_smoke.cpp` 不是当前主线，默认不构建；需要验证时配置 `-DMDF_BUILD_REPLAY_TESTS=ON`。
