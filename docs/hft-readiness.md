# HFT Readiness Plan

This project is primarily a market data fabric for medium-frequency systems, but the core should stay reusable for lower-latency and HFT-style deployments. The rule is simple: plan the hooks early, but keep slow service concerns out of the hot path.

## Boundary

HFT-style pipelines may reuse:

- `md-core` event types, headers, IDs, sequence metadata, and sink concepts.
- `md-refdata` lightweight symbol/precision lookups.
- `md-book` book builders.
- selected `md-codecs` and provider decoders.
- `md-runtime` low-latency runtime helpers.
- replay readers for deterministic testing.

They should not depend on:

- NATS or Kafka transports.
- service supervisors.
- control-plane watches or leases in the tick path.
- dynamic allocation-heavy config systems.
- blocking logs, metrics, databases, or network calls in callbacks.

## CPU and Thread Placement

Planned runtime features:

- Pin feed IO, decode, book, publisher, and recorder threads to explicit CPUs.
- Keep control-plane and metrics threads away from hot data-plane cores.
- Support Linux `pthread_setaffinity_np` behind `md-runtime`.
- Provide macOS no-op/best-effort behavior for Apple Silicon development.
- Keep per-thread profiles in config so production can change placement without code changes.

Example roles:

```text
feed_io      -> isolated CPU 2
decoder      -> isolated CPU 3
book_builder -> isolated CPU 4
publisher    -> isolated CPU 5
recorder     -> non-hot CPU 8
controller   -> non-hot CPU 9
metrics      -> non-hot CPU 9
```

## Timing and Clocks

Planned clock support:

- Distinguish source timestamp, receive timestamp, normalized timestamp, and publish timestamp.
- Use `CLOCK_MONOTONIC_RAW` by default on Linux for local latency measurement.
- Leave room for TSC-based readers where CPU/TSC stability is validated.
- Leave room for PHC/NIC hardware timestamps in colocated or direct-feed deployments.
- Require PTP/chrony discipline for cross-machine latency interpretation.

Never mix wall-clock correctness and latency measurement without explicitly recording the clock source.

## Memory and Allocation

Planned memory profile:

- Preallocate hot-path buffers, object pools, and ring buffers.
- Avoid `std::string`, JSON objects, heap allocation, and logging in hot callbacks.
- Use fixed-point `int64_t` prices and quantities.
- Align hot structs and ring slots to cache lines where needed.
- Support Linux `mlockall`/page-fault avoidance in production profiles.
- Keep huge page support optional and benchmark-driven.

## Queues and Back Pressure

Planned queue model:

- Prefer SPSC rings where topology allows it.
- Use bounded queues only; no unbounded growth in market data paths.
- Slow consumers must not block feed ingestion.
- Policy should be explicit: drop, skip-to-latest, disconnect consumer, or route to a durable stream.

## Networking

Planned Linux production features:

- UDP multicast support only after IDC/VLAN validation.
- `recvmmsg`/batch receive for UDP feeds where useful.
- TCP options and busy polling profiles where measured to help.
- NIC RSS/IRQ affinity documentation for production hosts.
- Future extension point for AF_XDP/DPDK/kernel-bypass direct feeds.

These features belong in transport/runtime layers, not in `md-core`.

## Observability Without Hot-Path Damage

Latency metrics should be sampled or written to lock-free side channels. Avoid blocking exporters in hot callbacks. Record at least:

- source-to-receive latency when source timestamps are trustworthy.
- receive-to-publish latency.
- publish-to-consume latency for md-client transports.
- sequence gaps, duplicates, stale streams, reconnect count, and book validation failures.

## Config Surface

A low-latency profile should be explicit and optional:

```yaml
runtime_profile: low_latency
threads:
  feed_io:
    cpu: 2
    scheduler: fifo
    priority: 70
  decoder:
    cpu: 3
    scheduler: fifo
    priority: 65
memory:
  preallocate: true
  lock_pages: true
  prefer_huge_pages: false
clock:
  source: monotonic_raw
  require_ptp_sync: true
```

## Development vs Production

Apple Silicon macOS is fine for core development, parsers, replay, and tests. Production latency validation should happen on the target Linux CPU architecture with the target kernel, NIC, clock sync, and deployment topology.
