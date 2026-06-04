# Market Data Schema v1

This directory is reserved for wire schemas such as FlatBuffers, Cap'n Proto, SBE, or Protobuf.

The C++ structs in `libs/md-core` define the in-process event model. Wire schemas should preserve the same semantic fields and include explicit schema versioning.
