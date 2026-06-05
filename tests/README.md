# tests 测试目录

测试目录用于放单元测试、集成测试、回放测试、fuzz 和 benchmark。

当前已有 smoke test：

- `raw_file_smoke.cpp`：验证 raw 文件写入、100 MiB 风格容量限制和读取。
- `binance_spot_streams_smoke.cpp`：验证 Binance Spot stream 名称、URL 和 raw envelope 生成。

装好 CMake 后可以通过 `ctest` 运行。当前环境没有 CMake 时，也可以用 `c++ -std=c++20` 手动编译这些 smoke test。
