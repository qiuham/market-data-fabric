# md-transport

Publisher and subscriber transports for standardized market data events.

Planned transports:

- NATS for service-friendly pub/sub.
- Kafka for durable streams, research, replay, and ETL.
- Shared memory for same-host low-latency consumers.
- TCP fanout for simple deployments.
- UDP multicast for IDC networks that explicitly support it.
- File output for recording.
