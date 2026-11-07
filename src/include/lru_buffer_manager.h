#pragma once

#include "buffer_manager.h"
#include <list>

namespace minirel {

/**
 * @brief Buffer manager using the least recently used (LRU) replacement policy.
 * Please refer to the writeup for a more detailed description of the LRU
 * replacement policy.
 */
class LRUBufferManager : public BufferManager {
 public:
  explicit LRUBufferManager(std::size_t num_frames);
  ~LRUBufferManager() override;

  [[nodiscard]] Status read_page(File* file, int page_no, Page*& page) override;
  [[nodiscard]] Status unpin_page(File* file, int page_no, bool dirty) override;
  [[nodiscard]] Status alloc_page(File* file, int& page_no, Page*& page) override;
  [[nodiscard]] Status flush_file(const File* file) override;
  [[nodiscard]] Status dispose_page(File* file, int page_no) override;
  void print_self() const override;

  private:
    PageTable page_table_; // maps page keys to frame IDs
    std::list<FrameId> lru_list_; // list of unpinned frames in oldest to newest order
    Status acquire_frame(FrameId& frame_id); // find an empty frame or evict oldest unpinned frame
};

}  // namespace minirel
