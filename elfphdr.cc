/*
    Copyright 2023 Google LLC

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        https://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

*/
#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cassert>

#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "assertop.h"
#include "scoped_fd.h"
#include "scoped_timer.h"
#include "string_splitter.h"
#include "threadpool.h"

const int kNumThreads = 8;

template <int ELFCLASSXX, class ElfXX_Ehdr, class ElfXX_Shdr, class ElfXX_Phdr,
          class ElfXX_Dyn>
class ElfFile {
 public:
  ElfFile() {}

  bool load_elf(const std::string& filename) {
    fd_.reset(open(filename.c_str(), O_RDONLY | O_CLOEXEC));
    if (fd_.get() == -1) {
      return false;
    }
    struct stat st;
    assert(fstat(fd_.get(), &st) != -1);
    length_ = st.st_size;

    mem_ = mmap(NULL, length_, PROT_READ,
                MAP_SHARED /* it should not matter if it's read-only */,
                fd_.get(), 0);
    if (mem_ == MAP_FAILED) {
      std::cout << "failed to open " << filename << " of length " << length_
                << std::endl;
      perror("mmap");
    }
    assert(mem_ != MAP_FAILED);
    assert(mem_ != nullptr);

    data_ = static_cast<char*>(mem_);

    ASSERT_EQ(data_[0], 127);
    ASSERT_EQ(data_[1], 'E');
    ASSERT_EQ(data_[2], 'L');
    ASSERT_EQ(data_[3], 'F');
    if (data_[EI_CLASS] != ELFCLASSXX) {
      // Try another bitness
      return false;
    }
    ASSERT_EQ(data_[EI_DATA], ELFDATA2LSB);

    const auto& hdr = *reinterpret_cast<const ElfXX_Ehdr*>(data_);
    phdr_ = reinterpret_cast<const ElfXX_Phdr*>(data_ + hdr.e_phoff);
    for (int i = 0; i < hdr.e_phnum; i++) {
      loadPhdr(reinterpret_cast<const ElfXX_Phdr*>(data_ + hdr.e_phoff +
                                                   hdr.e_phentsize * i));
    }

    const ElfXX_Phdr* dynph = getPhdr(PT_DYNAMIC);
    assert(dynph);

    loadDynamicStrtab(dynph);
    loadDynamicNeeded(dynph);

    return true;
  }

  ~ElfFile() {
    if (data_) {
      int retval = munmap(mem_, length_);
      if (retval != 0) {
        perror("munmap");
        std::cout << (int64_t)mem_ << " ...  " << (int64_t)length_ << std::endl;
      }

      // Outside of critical section
      readahead(fd_.get(), 0, length_);
      fd_.reset(-1);
    }
  }

  const std::vector<std::string_view>& GetNeeded() const { return needed_; }

 private:
  void loadDynamicStrtab(const ElfXX_Phdr* dynph) {
    bool found = false;

    const ElfXX_Dyn* dyntab =
        reinterpret_cast<const ElfXX_Dyn*>(data_ + (dynph->p_offset));
    for (size_t i = 0; dyntab[i].d_tag != DT_NULL; ++i) {
      auto& entry = dyntab[i];
      if (entry.d_tag == DT_STRTAB) {
        // Check that there's only one entry.
        assert(!found);
        found = true;

        // just take one entry and use its offset. TODO this is wrong, fix it.
        ElfXX_Phdr p1 = phdr_[0];
        int64_t offset_delta = entry.d_un.d_ptr - p1.p_vaddr + p1.p_offset;

        const char* strtab_addr = data_ + offset_delta;
        dynamic_strtab_addr_ = strtab_addr;
      }
    }
    assert(found);
  }

  void loadDynamicNeeded(const ElfXX_Phdr* dynph) {
    // Find the dynamic stuff.
    assert(dynamic_strtab_addr_);
    const ElfXX_Dyn* dyntab =
        reinterpret_cast<const ElfXX_Dyn*>(data_ + (dynph->p_offset));
    for (size_t i = 0; dyntab[i].d_tag != DT_NULL; ++i) {
      auto& entry = dyntab[i];
      if (entry.d_tag == DT_NEEDED) {
        // Uses d_un.d_val as an offset into the string table.
        // From the spec: This element holds the string table offset of a
        // null-terminated string, giving the name of a needed library. The
        // offset is an index into the table recorded in the DT_STRTAB.
        needed_.emplace_back(dynamic_strtab_addr_ + entry.d_un.d_val);
      }
    }
  }

  std::string_view getShStrTabName(size_t offset) {
    assert(shstrtab_string_.size() > offset);
    return std::string_view(shstrtab_string_.data() + offset);
  }

  void loadPhdr(const ElfXX_Phdr* ph) {
    p_headers_.insert(std::make_pair(ph->p_type, ph));
  }

  const ElfXX_Phdr* getPhdr(uint32_t type) {
    auto it = p_headers_.find(type);
    if (it != p_headers_.end()) {
      return it->second;
    }
    return nullptr;
  }

  std::unordered_map<uint32_t, const ElfXX_Phdr*> p_headers_{};
  std::string_view shstrtab_string_{};
  ScopedFd fd_{};

  // mmap parameter stored for convenience for munmap.
  void* mem_{};
  // mmap result for use.
  const char* data_{};
  size_t length_{};
  const char* dynamic_strtab_addr_{};
  std::vector<std::string_view> needed_;
  const ElfXX_Phdr* phdr_;
};

// TODO: obtain it from somewhere else, taken from LD_DEBUG=libs ld.so, on a
// x86_64 machine.
const std::string_view kPaths =
    // from chromoting.
    "/usr/lib/mesa-diverted/x86_64-linux-gnu/tls/x86_64/x86_64:/usr/lib/"
    "mesa-diverted/x86_64-linux-gnu/tls/x86_64:/usr/lib/mesa-diverted/"
    "x86_64-linux-gnu/tls/x86_64:/usr/lib/mesa-diverted/x86_64-linux-gnu/tls:/"
    "usr/lib/mesa-diverted/x86_64-linux-gnu/x86_64/x86_64:/usr/lib/"
    "mesa-diverted/x86_64-linux-gnu/x86_64:/usr/lib/mesa-diverted/"
    "x86_64-linux-gnu/x86_64:/usr/lib/mesa-diverted/x86_64-linux-gnu:/usr/lib/"
    "x86_64-linux-gnu/mesa/tls/x86_64/x86_64:/usr/lib/x86_64-linux-gnu/mesa/"
    "tls/x86_64:/usr/lib/x86_64-linux-gnu/mesa/tls/x86_64:/usr/lib/"
    "x86_64-linux-gnu/mesa/tls:/usr/lib/x86_64-linux-gnu/mesa/x86_64/x86_64:/"
    "usr/lib/x86_64-linux-gnu/mesa/x86_64:/usr/lib/x86_64-linux-gnu/mesa/"
    "x86_64:/usr/lib/x86_64-linux-gnu/mesa:/usr/lib/x86_64-linux-gnu/dri/tls/"
    "x86_64/x86_64:/usr/lib/x86_64-linux-gnu/dri/tls/x86_64:/usr/lib/"
    "x86_64-linux-gnu/dri/tls/x86_64:/usr/lib/x86_64-linux-gnu/dri/tls:/usr/"
    "lib/x86_64-linux-gnu/dri/x86_64/x86_64:/usr/lib/x86_64-linux-gnu/dri/"
    "x86_64:/usr/lib/x86_64-linux-gnu/dri/x86_64:/usr/lib/x86_64-linux-gnu/"
    "dri:/usr/lib/x86_64-linux-gnu/gallium-pipe/tls/x86_64/x86_64:/usr/lib/"
    "x86_64-linux-gnu/gallium-pipe/tls/x86_64:/usr/lib/x86_64-linux-gnu/"
    "gallium-pipe/tls/x86_64:/usr/lib/x86_64-linux-gnu/gallium-pipe/tls:/usr/"
    "lib/x86_64-linux-gnu/gallium-pipe/x86_64/x86_64:/usr/lib/x86_64-linux-gnu/"
    "gallium-pipe/x86_64:/usr/lib/x86_64-linux-gnu/gallium-pipe/x86_64:/usr/"
    "lib/x86_64-linux-gnu/gallium-pipe"
    // system serch path
    "/lib/x86_64-linux-gnu/tls/x86_64/x86_64:/lib/x86_64-linux-gnu/tls/x86_64:/"
    "lib/x86_64-linux-gnu/tls/x86_64:/lib/x86_64-linux-gnu/tls:/lib/"
    "x86_64-linux-gnu/x86_64/x86_64:/lib/x86_64-linux-gnu/x86_64:/lib/"
    "x86_64-linux-gnu/x86_64:/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu/"
    "tls/x86_64/x86_64:/usr/lib/x86_64-linux-gnu/tls/x86_64:/usr/lib/"
    "x86_64-linux-gnu/tls/x86_64:/usr/lib/x86_64-linux-gnu/tls:/usr/lib/"
    "x86_64-linux-gnu/x86_64/x86_64:/usr/lib/x86_64-linux-gnu/x86_64:/usr/lib/"
    "x86_64-linux-gnu/x86_64:/usr/lib/x86_64-linux-gnu:/lib/tls/x86_64/x86_64:/"
    "lib/tls/x86_64:/lib/tls/x86_64:/lib/tls:/lib/x86_64/x86_64:/lib/x86_64:/"
    "lib/x86_64:/lib:/usr/lib/tls/x86_64/x86_64:/usr/lib/tls/x86_64:/usr/lib/"
    "tls/x86_64:/usr/lib/tls:/usr/lib/x86_64/x86_64:/usr/lib/x86_64:/usr/lib/"
    "x86_64:/usr/lib";

template <int ELFCLASSXX, class ElfXX_Ehdr, class ElfXX_Shdr, class ElfXX_Phdr,
          class ElfXX_Dyn>
class LibPreloader {
 public:
  LibPreloader() : paths_(string_splitter(kPaths, ":")) {}
  ~LibPreloader() {
    if (pool_) {
      pool_->Wait();
    }
    if (stats_.size()) {
      std::cout << "[" << std::endl;
      for (const auto& p : stats_) {
        std::cout << R"({"name": ")" << p.first
                  << R"(", "cat": "PERF", "ph": "B", "ts": )"
                  << p.second.start.count() << R"(, "pid": 0, "tid": )"
                  << p.second.tid << R"( },)" << std::endl
                  << R"({"name": ")" << p.first
                  << R"(", "cat": "PERF", "ph": "E", "ts": )"
                  << p.second.end.count() << R"(, "pid": 0, "tid": )"
                  << p.second.tid << R"( },)" << std::endl;
      }

      // Do not output the last ] because trailing , would destroy it.
      // std::cout << "]" << std::endl;
    }
  }

  struct Stats {
    std::chrono::microseconds start;
    std::chrono::microseconds end;
    pthread_t tid;
  };

  bool loadMain(const char* filename) {
    ScopedTimer load_time;
    ElfFile<ELFCLASSXX, ElfXX_Ehdr, ElfXX_Shdr, ElfXX_Phdr, ElfXX_Dyn> elf;
    if (!elf.load_elf(filename)) {
      return false;
    }

    pool_ = std::make_unique<Threadpool>(kNumThreads);
    // std::cout << "loading " << filename << std::endl;
    const auto needed = elf.GetNeeded();
    for (const auto s : needed) {
      std::string needed_s{s};
      pool_->Post([needed_s, this]() { load(needed_s); });
    }

    {
      auto p = load_time.pair(time_begin_);
      std::lock_guard _(m_);
      stats_[filename] = {p.first, p.second, pthread_self()};
    }

    return true;
  }

 private:
  void load(std::string name) {
    {
      std::lock_guard _(m_);
      auto& is_loaded = loaded_[name];
      if (is_loaded) {
        return;
      }
      // Mark that we're loading this so that our recursive loading doesn't
      // try loading again.
      is_loaded = true;
    }

    ScopedTimer load_time;

    for (const auto& path : paths_) {
      // TODO: this is lots of allocation, do I need to make it faster?
      std::string full_path = std::string(path) + "/" + std::string(name);

      ElfFile<ELFCLASSXX, ElfXX_Ehdr, ElfXX_Shdr, ElfXX_Phdr, ElfXX_Dyn> elf;
      if (!elf.load_elf(full_path)) {
        continue;
      }

      auto needed = elf.GetNeeded();
      for (const std::string_view s : needed) {
        std::string needed_s{s};
        pool_->Post([needed_s, this]() { load(needed_s); });
      }
      break;
    }

    {
      auto p = load_time.pair(time_begin_);
      std::lock_guard _(m_);
      stats_[name] = {p.first, p.second, pthread_self()};
    }
  }

 private:
  const std::vector<std::string_view> paths_{};

  std::mutex m_{};
  std::unordered_map<std::string, bool> loaded_{};
  std::unordered_map<std::string, Stats> stats_{};
  std::chrono::steady_clock::time_point time_begin_{
      std::chrono::steady_clock::now()};
  std::unique_ptr<Threadpool> pool_{};
};

bool LoadElfFile(const char* filename) {
  LibPreloader<ELFCLASS32, Elf32_Ehdr, Elf32_Shdr, Elf32_Phdr, Elf32_Dyn> p32;
  // std::cout << "try p32" << std::endl;

  if (p32.loadMain(filename)) {
    return true;
  }

  LibPreloader<ELFCLASS64, Elf64_Ehdr, Elf64_Shdr, Elf64_Phdr, Elf64_Dyn> p64;
  // std::cout << "try p64 as fallback" << std::endl;
  return p64.loadMain(filename);
}

/*

on cached case, overhead is about 10ms.

on regular case win is about 20% on blender.

*/
