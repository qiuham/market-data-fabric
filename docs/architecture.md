# Architecture

## Responsibility Boundaries

`md-core` is the reusable core and should stay free of broker, SDK, database, service, and OS-specific dependencies. It contains standard event types, timestamps, sequence metadata, and sink interfaces.

`md-adapters` owns provider-specific behavior. Every provider can have different session semantics, symbol formats, timestamps, sequence numbers, book depth, gap recovery, reconnect behavior, and SDK threading rules.

`md-service` assembles a gateway pipeline inside a process. It loads config, creates adapters, attaches recorders and publishers, exposes health, and manages lifecycle.

`md-cluster` owns distributed metadata concepts: assignment, leases, node registry, failover, and publisher epochs. It is a logical control plane and must not sit in the market data tick path.

`md-client` is the consumer-side library. Strategies can link it to consume live or replay data without knowing whether the transport is NATS, Kafka, shared memory, or another backend.

`md-runtime` contains low-latency runtime hooks such as CPU affinity, scheduler policy, clock readers, memory locking, and platform-specific tuning. These hooks are optional and should not make `md-core` OS-specific.

## Kafka-Style Roles

The project can use one binary with multiple roles:

```text
md-node --roles=gateway
md-node --roles=controller
md-node --roles=gateway,controller
```

This keeps development simple while preserving the option to isolate controllers from gateway workers later.

## HFT Reuse Boundary

HFT-style code should reuse only the low-level pieces:

- `md-core`
- lightweight `md-refdata`
- `md-book`
- selected `md-codecs`
- selected feed decoders
- `md-runtime` low-latency hooks
- replay readers for testing

It should not depend on NATS, Kafka, Prometheus, databases, service supervisors, or dynamic control-plane clients in the hot path.
