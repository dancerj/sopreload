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
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <vector>

class Threadpool {
 public:
  using Job = std::function<void()>;

  Threadpool(size_t n) {
    for (size_t i = 0; i < n; ++i) {
      executors_.emplace_back([this, i](std::stop_token stop_token) {
        // Loop as long as there's available jobs and stop is not requested.
        while (!stop_token.stop_requested() || jobs_.size()) {
          std::unique_lock<std::mutex> lm(m_);
          cv_.wait(lm, stop_token, [this]() { return (jobs_.size() != 0); });
          while (jobs_.size()) {
            auto job = std::move(jobs_.front());
            jobs_.pop();
            {
              running_++;
              lm.unlock();
              std::move(job)();
              lm.lock();
              running_--;
            }
            // Updated running state; someone might be doing Wait().
            if (!running_) {
              cv_.notify_all();
            }
          }
        }
      });
    }
  }

  ~Threadpool() {
    for (auto &t : executors_) {
      t.request_stop();
      t.join();
    }
  }

  // An approximate way of waiting. I get randomly woken up.
  void Wait() {
    std::unique_lock<std::mutex> lm(m_);
    while (jobs_.size() || running_) {
      // std::cout << jobs_.size() << " " << running_ << std::endl;
      cv_.wait(lm, [this]() { return !(jobs_.size() || running_); });
      cv_.notify_one();
    }
  }

  void Post(Job job) {
    std::lock_guard _(m_);
    jobs_.push(std::move(job));
    cv_.notify_one();
  }

 private:
  std::vector<std::jthread> executors_{};
  std::mutex m_{};
  std::condition_variable_any cv_{};
  std::queue<Job> jobs_{};
  int running_{0};
};
