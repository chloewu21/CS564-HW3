#include "lru_buffer_manager.h"

namespace minirel {

LRUBufferManager::LRUBufferManager(std::size_t num_frames)
    : BufferManager(num_frames) {}

LRUBufferManager::~LRUBufferManager() {}

Status LRUBufferManager::read_page(File* file, int page_no, Page*& page) {
  (void)file;
  (void)page_no;
  (void)page;
  return Status::Ok;
}

Status LRUBufferManager::unpin_page(File* file, int page_no, bool dirty) {
  (void)file;
  (void)page_no;
  (void)dirty;
  return Status::Ok;
}

Status LRUBufferManager::alloc_page(File* file, int& page_no, Page*& page) {
  (void)file;
  (void)page_no;
  (void)page;
  return Status::Ok;
}

Status LRUBufferManager::flush_file(const File* file) {
  (void)file;
  return Status::Ok;
}

Status LRUBufferManager::dispose_page(File* file, int page_no) {
  (void)file;
  (void)page_no;
  return Status::Ok;
}

void LRUBufferManager::print_self() const {}

}  // namespace minirel
