# tests

测试目录用于放单元测试、集成测试、fuzz 和 benchmark。

当前 smoke tests：

- `core_events_smoke.cpp`：验证统一事件模型和 instrument 模型的基础语义。
- `binance_feeds_smoke.cpp`：验证 Binance feed key、connection spec 和 envelope 生成。
- `binance_book_ticker_mapper_smoke.cpp`：验证 Binance `bookTicker -> Quote` 字段转换。
- `binance_trade_mapper_smoke.cpp`：验证 Binance `trade -> Trade` 字段转换。
- `websocket_endpoint_smoke.cpp`：验证 `md-net` WebSocket endpoint 解析。
- `spsc_ring_smoke.cpp`：验证 SPSC ring 的顺序、满队列和双线程读写。
- `feed_session_smoke.cpp`：验证 feed session 重试、停止条件和统计。

运行：

```bash
ctest --test-dir cmake-build-debug --output-on-failure
```
