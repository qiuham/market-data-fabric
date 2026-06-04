# md-runtime

Runtime utilities for low-latency and production deployment concerns.

This module is intentionally separate from `md-core`. `md-core` defines data and pipeline concepts; `md-runtime` can provide OS-specific implementations for CPU affinity, thread priority, clocks, memory locking, huge page hints, and runtime profiles.

Planned areas:

- CPU pinning and NUMA placement.
- Scheduler policy and thread priority.
- Clock source selection and timestamp calibration.
- Page-fault avoidance through preallocation and memory locking.
- Cache-line alignment helpers.
- Linux-specific network/runtime tuning hooks.
- macOS-safe no-op or best-effort fallbacks for development.
