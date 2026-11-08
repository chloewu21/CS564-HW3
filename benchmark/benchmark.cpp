#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

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

// Read-only sequential scans
// 1000 pages, 100 scans, each scan reads all pages in order, unpinning each page immediately after reading it
double benchmark1(const std::shared_ptr<BufferManager>& buf_mgr) {
  constexpr int kNumPages = 1000;
  constexpr int kNumScans = 100;
  const std::string path = "benchmark1.db";

  File::destroy(path);
  if (File::create(path) != Status::Ok) {
    return -1;
  }
  double elapsed_ms = -1;
  {
    File file(path);
    int page_nos[kNumPages];
    Page* page = nullptr;

    for (int i = 0; i < kNumPages; ++i) { // allocate pages in file
      if (file.allocate_page(page_nos[i]) != Status::Ok) return -1;
    }

    buf_mgr->clear_buf_stats();
    auto start = std::chrono::steady_clock::now(); // start timing

    for (int scan = 0; scan < kNumScans; ++scan) {
      for (int i = 0; i < kNumPages; ++i) {
        if (buf_mgr->read_page(&file, page_nos[i], page) != Status::Ok) { // read every page in order
          std::cerr << "Benchmark 1: read failed\n";
          std::exit(EXIT_FAILURE);
        }
        if (buf_mgr->unpin_page(&file, page_nos[i], false) != Status::Ok) { // release each page immediately to allow replacement
          std::cerr << "Benchmark 1: unpin failed\n";
          std::exit(EXIT_FAILURE);
        }
      }
    }

    auto end = std::chrono::steady_clock::now(); // end timing
    elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    BufStats stats = buf_mgr->get_buf_stats();

    // Print stats
    if (buf_mgr->flush_file(&file) != Status::Ok) {
      std::cerr << "Benchmark 1: flush failed\n";
      std::exit(EXIT_FAILURE);
    }
    std::cout << "accesses=" << stats.accesses
              << ", diskreads=" << stats.diskreads
              << ", diskwrites=" << stats.diskwrites << '\n';
  }
  if (File::destroy(path) != Status::Ok) return -1;
  return elapsed_ms;
}

// Read-only random access (90% hot pages, 10% cold pages)
// Same number of reads as benchmark1 but each read is a random page with 
// 90% probability of being one of the first 100 pages and 10% probability of being one of the remaining 900 pages
double benchmark2(const std::shared_ptr<BufferManager>& buf_mgr) {
  constexpr int kNumPages = 1000;
  constexpr int kHotPages = 100;
  constexpr int kNumReads = 100000;
  const std::string path = "benchmark2.db";

  File::destroy(path);
  if (File::create(path) != Status::Ok) {
    return -1;
  }
  double elapsed_ms = -1;
  {
    File file(path);
    int page_nos[kNumPages];
    Page* page = nullptr;

    for (int i = 0; i < kNumPages; ++i) { // set up 1000 pages in the file
      if (file.allocate_page(page_nos[i]) != Status::Ok) return -1;
    }

    std::mt19937 generator(42); // random number generator
    std::uniform_int_distribution<int> choose_group(0, 9); // produces ints from 0 to 9 
    std::uniform_int_distribution<int> choose_hot(0, kHotPages - 1); // chooses array index from 0 to 99
    std::uniform_int_distribution<int> choose_cold(kHotPages, kNumPages - 1); // chooses index from 100 to 999
    std::vector<int> requests(kNumReads); // store the page numbers to be read
    for (int i = 0; i < kNumReads; ++i) {
      if (choose_group(generator) < 9) { // 90% change of choosing a hot page
        requests[i] = page_nos[choose_hot(generator)];
      } else { // 10% chance of choosing a cold page
        requests[i] = page_nos[choose_cold(generator)];
      }
    }

    buf_mgr->clear_buf_stats();
    auto start = std::chrono::steady_clock::now(); // start timing

    for (int i = 0; i < kNumReads; ++i) {
      if (buf_mgr->read_page(&file, requests[i], page) != Status::Ok) { // read page based on requests vector
        std::cerr << "Benchmark 2: read failed\n";
        std::exit(EXIT_FAILURE);
      }
      if (buf_mgr->unpin_page(&file, requests[i], false) != Status::Ok) { // unpin immediately to allow replacement
        std::cerr << "Benchmark 2: unpin failed\n";
        std::exit(EXIT_FAILURE);
      }
    }

    auto end = std::chrono::steady_clock::now(); // stop timing
    elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    BufStats stats = buf_mgr->get_buf_stats();

    // Print stats
    if (buf_mgr->flush_file(&file) != Status::Ok) {
      std::cerr << "Benchmark 2: flush failed\n";
      std::exit(EXIT_FAILURE);
    }
    std::cout << "accesses=" << stats.accesses
              << ", diskreads=" << stats.diskreads
              << ", diskwrites=" << stats.diskwrites << '\n';
  }

  if (File::destroy(path) != Status::Ok) return -1;
  return elapsed_ms;
}

int main() {
  // // Running workload on LRU buffer manager instance.
  // std::cout << "LRU Buffer Manager:   " 
  //           << basic_workload(std::make_shared<LRUBufferManager>(100)) 
  //           << " ms\n";

  // // Running workload on Clock buffer manager instance.
  // std::cout << "Clock Buffer Manager: " 
  //           << basic_workload(std::make_shared<ClockBufferManager>(100))
  //           << " ms\n";

  // Benchmark 1: Sequential access
  std::cout << "\nBenchmark 1: sequential scans\n";
  std::cout << "LRU Buffer Manager:   " 
            << benchmark1(std::make_shared<LRUBufferManager>(100)) 
            << " ms\n";
  std::cout << "Clock Buffer Manager: "
            << benchmark1(std::make_shared<ClockBufferManager>(100))
            << " ms\n";
  
  // Benchmark 2: Random access with a hot set
  std::cout << "\nBenchmark 2: random access with a hot set\n";
  std::cout << "LRU Buffer Manager:   "
            << benchmark2(std::make_shared<LRUBufferManager>(100))
            << " ms\n";
  std::cout << "Clock Buffer Manager: "
            << benchmark2(std::make_shared<ClockBufferManager>(100))
            << " ms\n";
  return 0;
}
