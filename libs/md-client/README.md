# md-client

Consumer-side market data library for strategies and downstream services.

It wraps transport clients and schema decoding. It does not include feed adapters or broker servers. Consumers may bypass it and use native NATS/Kafka clients directly if desired.
