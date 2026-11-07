#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "clock_buffer_manager.h"
#include "lru_buffer_manager.h"

using namespace minirel;

// Basic benchmark where we allocate 100 pages, write into them,
// read them back, then unpin them. Returns the number of milliseconds
// the benchmark takes to run.
double basic_workload(const std::shared_ptr<BufferManager>& buf_mgr) {
  constexpr int kNumPages = 100;
  const std::string path = "benchmark_basic.db";

  File::destroy(path);
  if (File::create(path) != Status::Ok) return -1;
  double elapsed_ms = -1;
  {
    File file(path);
    int page_nos[kNumPages];
    Page* page = nullptr;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < kNumPages; ++i) {
      if (buf_mgr->alloc_page(&file, page_nos[i], page) != Status::Ok) return -1;
      std::string msg = "page " + std::to_string(page_nos[i]);
      std::memcpy(page->data.data(), msg.c_str(), msg.size() + 1);
    }

    for (int i = 0; i < kNumPages; ++i) {
      if (buf_mgr->read_page(&file, page_nos[i], page) != Status::Ok) return -1;
    }

    // Each page was pinned once by alloc_page and once by read_page.
    for (int i = 0; i < kNumPages; ++i) {
      if (buf_mgr->unpin_page(&file, page_nos[i], true) != Status::Ok) return -1;
      if (buf_mgr->unpin_page(&file, page_nos[i], false) != Status::Ok) return -1;
    }

    auto end = std::chrono::steady_clock::now();
    elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    if (buf_mgr->flush_file(&file) != Status::Ok) return -1;
  }
  File::destroy(path);
  return elapsed_ms;
}

int main() {
  // Running workload on LRU buffer manager instance.
  std::cout << "LRU Buffer Manager:   " 
            << basic_workload(std::make_shared<LRUBufferManager>(100)) 
            << " ms\n";

  // Running workload on Clock buffer manager instance.
  std::cout << "Clock Buffer Manager: " 
            << basic_workload(std::make_shared<ClockBufferManager>(100))
            << " ms\n";
  return 0;
}
