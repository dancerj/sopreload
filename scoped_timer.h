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
#ifndef SCOPED_TIMER_H_
#define SCOPED_TIMER_H_
/**
 * Utility class to get usecs spent.
 */
#include <chrono>
#include <utility>

/*
  system_clock, monotonic_clock and high_resolution_clock are supposed to exist,
  we have steady_clock and system_lock.
 */

class ScopedTimer {
 public:
  ScopedTimer() : begin_(std::chrono::steady_clock::now()) {}
  ~ScopedTimer() {}

  std::pair<std::chrono::microseconds, std::chrono::microseconds> pair(
      std::chrono::steady_clock::time_point base) const {
    std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    return {
        std::chrono::duration_cast<std::chrono::microseconds>(begin_ - base),
        std::chrono::duration_cast<std::chrono::microseconds>(end - base)};
  }

  std::chrono::microseconds elapsed_msec() const {
    std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - begin_);
  }

 private:
  std::chrono::steady_clock::time_point begin_;
};
#endif
