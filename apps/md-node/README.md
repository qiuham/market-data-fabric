# md-node

`md-node` 是当前唯一进程入口。

当前结构：

- `main.cpp`：日志初始化和命令分发。
- `cli_options.hpp`：通用 CLI option / log level 解析。
- `binance_live_options.*`：Binance live CLI 参数和 usage。
- `binance_mapper_handlers.*`：Binance normalized mapper handler 和调试输出。
- `binance_live_runner.cpp`：Binance live/preview 的连接、session、pipeline 编排。

当前支持：

```bash
md-node --binance-feed-spec-preview
md-node --binance-live --symbol=BTCUSDT --feed=bookTicker --messages=3 --normalize
md-node --binance-live --symbol=BTCUSDT --feed=trade --messages=3 --normalize
md-node --binance-live --symbol=BTCUSDT --feed=bookTicker --messages=3 --log-normalized=3
```

后续再增加 `--roles=gateway`、配置文件加载和 publisher pipeline。
