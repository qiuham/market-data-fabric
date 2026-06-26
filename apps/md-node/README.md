# md-node

`md-node` 是当前唯一进程入口。

当前支持：

```bash
md-node --binance-feed-spec-preview
md-node --binance-live --symbol=BTCUSDT --feed=bookTicker --messages=3 --normalize
md-node --binance-live --symbol=BTCUSDT --feed=trade --messages=3 --normalize
md-node --binance-live --symbol=BTCUSDT --feed=bookTicker --messages=3 --log-normalized=3
```

后续再增加 `--roles=gateway`、配置文件加载和 publisher pipeline。
