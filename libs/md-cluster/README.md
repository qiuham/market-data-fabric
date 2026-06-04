# md-cluster

Logical control-plane primitives for distributed deployments.

Owns node registration, heartbeats, leases, source assignment, failover, and publisher epochs. Market data ticks must not flow through this layer.
