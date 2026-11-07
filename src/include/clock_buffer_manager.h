#pragma once

#include "buffer_manager.h"
#include <list>

namespace minirel {

/**
 * @brief Buffer manager using the clock replacement policy. Please
 * refer to the writeup for a more detailed description of the clock
 * replacement policy.
 *
 * Notably, it is possible that every frame in the buffer pool is pinned
 * when read_page or alloc_page is called. In that case, nothing can be
 * evicted, so your clock algorithm should not sweep forever. Instead, it
 * should stop and return BufferExceeded. 
 */ 
class ClockBufferManager : public BufferManager {
 public:
  explicit ClockBufferManager(std::size_t num_frames);
  ~ClockBufferManager() override;

  // Functions that you must implement for the clock buffer manager.
  [[nodiscard]] Status read_page(File* file, int page_no, Page*& page) override;
  [[nodiscard]] Status unpin_page(File* file, int page_no, bool dirty) override;
  [[nodiscard]] Status alloc_page(File* file, int& page_no, Page*& page) override;
  [[nodiscard]] Status flush_file(const File* file) override;
  [[nodiscard]] Status dispose_page(File* file, int page_no) override;
  void print_self() const override;

  private:
    PageTable page_table_; // maps page keys to frame IDs
    std::list<FrameId> lru_list_; // list of frames with pin count = 0 in oldest to newest order
    Status acquire_frame(FrameId& frame_id); // find an empty frame or evict oldest unpinned frame
};

}  // namespace minirel
