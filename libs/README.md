# Libraries

This directory contains reusable C++ libraries. Keep dependencies flowing inward toward `md-core`; outer layers may depend on inner layers, but `md-core` must not depend on adapters, transports, services, or cluster code.
