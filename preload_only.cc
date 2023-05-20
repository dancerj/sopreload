#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>

#include "elfphdr.h"

int main(int argc, char** argv) {
  const char* filename = (argc == 1 ? "out/preload_only" : argv[1]);
  if (!LoadElfFile(filename)) {
    std::cerr << "Failed to load ELF file " << filename << std::endl;
    exit(1);
  }
  return 0;
}
