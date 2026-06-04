# Deployment Notes

## First Version

Use static YAML assignment and run one `md-node` process per source or shard.

```text
md-node --roles=gateway --config configs/prod/crypto/binance_spot.yaml
md-node --roles=gateway --config configs/prod/cn_futures/ctp.yaml
```

## Later Versions

Add `md-cluster` support for leases and assignments through one of:

- Kubernetes Lease/ConfigMap/CRD
- etcd
- Consul
- a dedicated `controller` role

Market data messages should continue to flow through data-plane transports, not through the control plane.
