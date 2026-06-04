# Binance Adapter

Skeleton for Binance spot/futures WebSocket and REST snapshot handling.

Responsibilities:

- Connect to Binance endpoints.
- Decode raw trade/depth messages.
- Convert provider fields to `md-core` events.
- Handle snapshot + delta alignment and sequence gaps.
