#include "marketdata/replay/checkpoint.hpp"

#include <cassert>

int main() {
  md::replay::CheckpointCoordinator checkpoint;
  assert(checkpoint.commit(3, 100, 4096));
  assert(checkpoint.committed().replay_from_sequence() == 101);
  assert(!checkpoint.commit(3, 100, 8192));
  assert(!checkpoint.commit(4, 101, 8192));
  assert(!checkpoint.commit(3, 101, 2048));
  assert(checkpoint.commit(3, 101, 8192));
  return 0;
}
