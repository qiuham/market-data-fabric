# Market Data Fabric

`market-data-fabric` is a C++ market data platform skeleton for multi-market quantitative systems. It is designed around a clean core library, provider-specific adapters, pluggable transports, recorder/replay, and a consumer-side client library.

The goal is to support crypto, China A-shares, US equities, and futures through multiple providers while keeping the core reusable for lower-latency or HFT-style pipelines.

## Design Goals

- Normalize many external feeds into one internal event model.
- Keep `md-core` transport-agnostic and vendor-agnostic.
- Let strategies consume live data, replay data, NATS, Kafka, shared memory, or future transports through one client abstraction.
- Support distributed deployment through logical control-plane and data-plane separation.
- Keep provider field conversion close to each adapter, not in the service layer.
- Plan low-latency hooks such as CPU affinity, clock sources, preallocation, and hot-path isolation without forcing them into ordinary deployments.
- Prefer Linux for production validation while allowing portable core development on macOS/Apple Silicon.

## Repository Layout

```text
apps/
  md-node/              Single binary entry point with role-based startup.

libs/
  md-core/              HFT-reusable event types, IDs, timestamps, sequence checks, sinks.
  md-refdata/           Instruments, venues, calendars, trading sessions, precision, symbol mapping.
  md-book/              L1/L2/MBO book builders and validators.
  md-codecs/            Shared protocol decoding helpers.
  md-adapters/          Provider adapters and provider-specific field mappers.
  md-transport/         Publisher/subscriber implementations: NATS, Kafka, shm, TCP, multicast, file.
  md-service/           Gateway runtime, pipeline builder, lifecycle, health, metrics.
  md-cluster/           Assignment, leases, failover, publisher epochs, node registry.
  md-runtime/           CPU affinity, scheduler, clocks, memory profiles, low-latency hooks.
  md-replay/            Raw and normalized recording plus replay readers.
  md-client/            Consumer-side library for strategies and services.

schemas/                Wire schemas for broker/file/shm payloads.
configs/                Development, production, and reference-data examples.
docs/                   Architecture and mapping notes.
tests/                  Unit, integration, replay, fuzz, and benchmark tests.
```

## Runtime Roles

The intended runtime can follow a Kafka-like role model: code is separated by responsibility, but deployment can be combined or isolated.

```text
md-node --roles=gateway
md-node --roles=controller
md-node --roles=gateway,controller
```

Early deployments can run only static gateway configs. Later deployments can add leases and assignments through Kubernetes, etcd, Consul, or an optional controller role.

## Data Plane

```text
external feed / SDK
  -> adapter client
  -> decoder
  -> provider field mapper
  -> md-core event
  -> sequence checker / book builder
  -> transport publisher
  -> NATS / Kafka / shm / TCP / multicast / file
```

Market data ticks should not go through the control plane or a consensus path. Consensus/leases are only for low-frequency metadata such as ownership, assignment, config version, and failover.

## Control Plane

Logical control-plane state includes:

- Node registry and heartbeat.
- Source and symbol assignment.
- Primary/standby leases.
- Publisher epoch for failover detection.
- Config version and rollout state.

The first version can use static YAML. Production can later use Kubernetes Lease/CRD, etcd, Consul, or a dedicated controller role.

## Adapter Field Mapping

Provider-specific conversion lives next to each adapter:

```text
libs/md-adapters/{asset_class}/{provider}/
  src/*_field_mapper.cpp
  schema/*.mapping.yaml
  docs/field_mapping.md
  tests/*_field_mapper_test.cpp
```

Standard fields are defined in `libs/md-core`. Reference-data context such as symbol mapping, price scale, quantity scale, calendars, and sessions belongs in `libs/md-refdata`.

## Consumer Library

`md-client` is a consumer-side SDK. It does not package brokers or feed adapters. It wraps transport clients and schema decoding so strategies can use one interface for live data, replay, NATS, Kafka, or shared memory.

```text
md-gateway -> broker/transport -> strategy + md-client
```

Consumers can also bypass `md-client` and use native NATS/Kafka clients directly with the published schemas.

## Initial Build

```bash
cmake -S . -B build
cmake --build build
./build/md-node --roles=gateway --config configs/dev/mock.yaml
```

## Roadmap

1. Stabilize `md-core` events, reference data, and replay file format.
2. Implement mock/replay adapters and one crypto adapter.
3. Add NATS publishing and consumer-side `md-client-nats`.
4. Add CTP and IB adapters behind platform-specific build flags.
5. Add `md-runtime` Linux implementations for CPU affinity, clock readers, memory locking, and thread profiles.
6. Add shared-memory transport and latency benchmarks.
7. Add lease/assignment support through `md-cluster`.
8. Add UDP multicast only after IDC/network capability is confirmed.
