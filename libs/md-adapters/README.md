# md-adapters

Adapters connect to external feeds or vendor SDKs, decode provider messages, map fields, and emit `md-core` events.

Do not create generic `AStockAdapter` or `FuturesAdapter` classes as the primary boundary. Prefer provider-specific adapters such as `binance`, `ctp`, `ib`, or `xtp` because feed semantics vary by provider.
