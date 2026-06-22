# tests 测试目录

测试目录用于放单元测试、集成测试、回放测试、fuzz 和 benchmark。

当前已有 smoke test：

- `message_log_smoke.cpp`：验证 message log 写入、100 MiB 风格容量限制和读取。
- `binance_feeds_smoke.cpp`：验证 Binance feed key、connection spec 和 envelope 生成。
- `websocket_endpoint_smoke.cpp`：验证 `md-net` WebSocket endpoint 解析。
- `spsc_ring_smoke.cpp`：验证 SPSC ring 的顺序、满队列和双线程读写。

装好 CMake 后可以通过 `ctest` 运行。
