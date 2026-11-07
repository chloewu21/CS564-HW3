#include "clock_buffer_manager.h"

namespace minirel {

ClockBufferManager::ClockBufferManager(std::size_t num_frames)
    : BufferManager(num_frames) {}

ClockBufferManager::~ClockBufferManager() {}

Status ClockBufferManager::read_page(File* file, int page_no, Page*& page) {
  (void)file;
  (void)page_no;
  (void)page;
  return Status::Ok;
}

Status ClockBufferManager::unpin_page(File* file, int page_no, bool dirty) {
  (void)file;
  (void)page_no;
  (void)dirty;
  return Status::Ok;
}

Status ClockBufferManager::alloc_page(File* file, int& page_no, Page*& page) {
  (void)file;
  (void)page_no;
  (void)page;
  return Status::Ok;
}

Status ClockBufferManager::flush_file(const File* file) {
  (void)file;
  return Status::Ok;
}

Status ClockBufferManager::dispose_page(File* file, int page_no) {
  (void)file;
  (void)page_no;
  return Status::Ok;
}

void ClockBufferManager::print_self() const {}

}  // namespace minirel
