#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>

#include "elfphdr.h"

int main(int argc, char** argv) {
  pid_t child;

  const char* kDefault[] = {"out/elfphdr", nullptr};
  const char** argv1 =
      (argc >= 2) ? const_cast<const char**>(&argv[1]) : kDefault;
  switch (child = fork()) {
    case -1:
      perror("fork");
      exit(1);
      break;
    case 0:
      execv(argv1[0], const_cast<char**>(argv1));
      perror("execv");
      exit(1);
    default:;
  };

  std::cout << "Running: " << argv1[0] << std::endl;
  const char* filename = argv1[0];
  if (!LoadElfFile(filename)) {
    std::cerr << "Failed to load ELF file " << filename << std::endl;
    exit(1);
  }

  int status;
  waitpid(child, &status, 0);
  return WEXITSTATUS(status);
}
