#pragma once

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <vector>

#include "file.h"
#include "page.h"
#include "status.h"

namespace minirel {

using FrameId = std::size_t;

struct BufStats {
  std::size_t accesses = 0;    // successful read_page/alloc_page calls
  std::size_t diskreads = 0;   // pages read from disk
  std::size_t diskwrites = 0;  // pages written back to disk

  void clear() { *this = BufStats{}; }
};

// Metadata for a single buffer pool frame.
struct FrameDescriptor {
  File* file = nullptr;
  int page_no = INVALID_PAGE_NO;
  int pin_count = 0;
  bool dirty = false; // true if the page has been written to since being read in
  bool valid = false;  // true if the frame holds a page
};

// Identifies a page across all files.
struct PageKey {
  const File* file;
  int page_no;

  bool operator==(const PageKey& other) const {
    return file == other.file && page_no == other.page_no;
  }
};

struct PageKeyHash {
  std::size_t operator()(const PageKey& key) const {
    std::size_t h = std::hash<const File*>{}(key.file);
    return h ^ (std::hash<int>{}(key.page_no) + 0x9e3779b9 + (h << 6) + (h >> 2));
  }
};

// Hash table structure that stores the mapping from pages to the frames that contain them
using PageTable = std::unordered_map<PageKey, FrameId, PageKeyHash>;

// Buffer manager base class.
class BufferManager {
 public:
  explicit BufferManager(std::size_t num_frames)
      : frames_(num_frames), pool_(num_frames) {}
  virtual ~BufferManager();

  BufferManager(const BufferManager&) = delete;
  BufferManager& operator=(const BufferManager&) = delete;
  BufferManager(BufferManager&&) = delete;
  BufferManager& operator=(BufferManager&&) = delete;

/**
   * @brief Reads a page from the buffer pool. If the page is already in the
   * buffer pool, its pin count is incremented and the page is returned. If
   * there are empty, unpinned frames in the buffer pool, the page is read
   * into one of them and returned. If there are no empty frames, a victim
   * page is chosen to be evicted using the replacement policy. If the evicted
   * page is dirty, it is flushed to disk before the new page is read in. Then,
   * the new page will be read into the now empty frame and returned.
   * On success, `page` points to the page in the buffer pool. It should 
   * also update the buffer statistics as appropriate.
   * 
   * This function should return BadFilePtr if the file pointer is null, and
   * BufferExceeded if every frame in the buffer pool is pinned. It should
   * also propagate any errors returned by File::read_page or File::write_page
   * if necessary. It should return Ok on success.
   * 
   * @param file File pointer to the file containing the page to be read
   * @param page_no Page number of the page to be read
   * @param page Pointer to the page in the buffer pool
   * @return Status 
   */
  [[nodiscard]] virtual Status read_page(File* file, int page_no, Page*& page) = 0;

/**
   * @brief Reduces the pin count of page `page_no` of `file` by 1. If the
   * page is dirty, it is marked accordingly so it is written back if it gets
   * evicted.
   * 
   * Returns PageNotFound if the page does not exist in the buffer pool, or
   * PageNotPinned if the page's pin count is already 0. It should return Ok
   * on success.
   * 
   * @param file File pointer to the file containing the page to be unpinned
   * @param page_no Page number of the page to be unpinned 
   * @param dirty Dirty flag (indicates whether the page was written to since being read into the buffer pool)
   * @return Status 
   */
  [[nodiscard]] virtual Status unpin_page(File* file, int page_no, bool dirty) = 0;

/**
   * @brief Allocates a new page in `file` and places it in the buffer pool
   * with a pin count of 1. If there are empty, unpinned frames in the buffer
   * pool, we place the new page in one of them. If there are no empty frames,
   * a victim page is chosen to be evicted using the replacement policy. If the
   * evicted page is dirty, it is flushed to disk before the new page is read in. Then,
   * the allocated page will be read into the now empty frame and returned.
   * 
   * Unlike read_page, the contents of the new page are not read from disk and
   * the page is filled with zeros. Additionally, keep in mind that a frame
   * should be obtained before we allocate the page in the file so that we do
   * not waste resources if the buffer pool is full. On success, `page_no`
   * contains the new page's page number, and `page` points to the allocated
   * page in the buffer pool. It should also update the buffer statistics
   * as appropriate.
   *
   * This function should return BadFilePtr if the file pointer is null, and
   * BufferExceeded if every frame in the buffer pool is pinned. It should
   * also propagate any errors returned by File::allocate_page or
   * File::write_page if necessary. It should return Ok on success.
   *
   * @param file File pointer to the file in which to allocate the page
   * @param page_no Page number of the newly allocated page
   * @param page Pointer to the new page in the buffer pool
   * @return Status
   */
  [[nodiscard]] virtual Status alloc_page(File* file, int& page_no, Page*& page) = 0;

/**
   * @brief Writes every dirty page of `file` in the buffer pool back to disk
   * and evicts all of the file's pages from the buffer pool. This should be
   * called before a file is closed. It should also update the buffer statistics
   * as appropriate.
   *
   * This function should return PagePinned, without writing or evicting
   * anything, if any page of `file` is still pinned. It should also
   * propagate any errors returned by File::write_page if necessary. It
   * should return Ok on success.
   *
   * @param file File pointer to the file whose pages are to be flushed
   * @return Status
   */
  [[nodiscard]] virtual Status flush_file(const File* file) = 0;

/**
   * @brief Deletes page `page_no` from File `file`. If the page is in the buffer
   * pool, it is first evicted without being written back to disk, since its
   * contents are being thrown away. The page is then disposed of in the file.
   *
   * This function should return BadFilePtr if the file pointer is null,
   * PagePinned if the page is in the buffer pool and is still pinned. It should
   * also propagate any errors returned by File::dispose_page if necessary.
   * It should return Ok on success.
   *
   * @param file File pointer to the file containing the page to be disposed
   * @param page_no Page number of the page to be disposed
   * @return Status
   */
  [[nodiscard]] virtual Status dispose_page(File* file, int page_no) = 0;

  /**
   * @brief Virtual method that prints the state of the buffer manager. 
   * Feel free to print any information that you may find useful.
   */
  virtual void print_self() const = 0;

  /**
   * @brief Returns buffer statistics struct.
   * @return The buffer statistics.
   */
  const BufStats& get_buf_stats() const { return buf_stats_; }

  /**
   * @brief Clears the buffer statistics.
   * @return void
   */
  void clear_buf_stats() { buf_stats_.clear(); }

  // Required: used by tests, do not change.
  std::size_t num_frames() const { return frames_.size(); }
  const FrameDescriptor& frame_descriptor(FrameId frame) const {
    return frames_.at(frame);
  }

 protected:
 // Required: do not rename or remove these variables. They are used by the tests.
 // Part of the assignment is figuring out how to to use them correctly in your clock
 // and LRU buffer manager implementations.

 // Buffer statistics. These stats should be updated appropriately in your implementation.
  BufStats buf_stats_;

  // The frame descriptors contain the list of frames in the buffer pool and their metadata
  // (e.g., which pages are in the buffer pool)
  std::vector<FrameDescriptor> frames_;

  // The pool of pages in the buffer pool. 
  std::vector<Page> pool_;
};

}  // namespace minirel
