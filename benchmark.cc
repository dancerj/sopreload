#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <numeric>
#include <vector>

#include "scoped_timer.h"

namespace {
int ForkExec(char** argv) {
  pid_t child;

  switch (child = fork()) {
    case -1:
      perror("fork");
      exit(1);
      break;
    case 0:
      close(1);
      close(2);
      execv(argv[0], argv);
      perror("execv");
      exit(1);
    default:;
  };

  int status;
  waitpid(child, &status, 0);
  return WEXITSTATUS(status);
}
}  // anonymous namespace

void PrintDurations(const char * label, std::vector<uint64_t> durations) {
  std::cout << label << std::endl;
  for (auto duration : durations) {
    std::cout << duration << " ";
  }
  std::cout << std::endl;
  uint64_t sum = std::reduce(durations.begin(), durations.end());
  std::cout << "mean: " << (sum / durations.size()) << std::endl;
}

int main(int argc, char** argv) {
  if (argc <= 2) {
    std::cout << R"()" << std::endl;

    return 1;
  }
  const size_t iterations = std::stoi(argv[1]);
  std::vector<uint64_t> durations;
  std::vector<uint64_t> cached_durations;
  for (size_t i = 0; i < iterations; ++i) {
    // Note: this requires root access via sudo. Dropping page cache
    // is not usually doable from a regular user.
    system("sudo bash -c 'echo 3 > /proc/sys/vm/drop_caches'");
    {
      ScopedTimer t;
      ForkExec(&argv[2]);
      durations.push_back(t.elapsed_msec().count());
    }
    {
      ScopedTimer t;
      ForkExec(&argv[2]);
      cached_durations.push_back(t.elapsed_msec().count());
    }
  }

  PrintDurations("Uncached", durations);
  PrintDurations("Cached", cached_durations);


  return 0;
}
