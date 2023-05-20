#include "scoped_timer.h"

#include <assert.h>

int main() {
  {
    ScopedTimer s;
    auto count = s.elapsed_msec().count();
    assert(count >= 0);
    // I hope this doesn't take more than a second to run.
    assert(count <= 1000);
  }
}
