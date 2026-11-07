#pragma once

#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "buffer_manager.h"
#include "file.h"
#include "page.h"

namespace minirel::test {

// Creates and opens a new database file in the system temp directory, and
// deletes it when the object is destroyed. Declare and initialize an instance
// of this class before you declare a BufferManager object that will use this file,
// so that the BufferManager is destroyed before the file is deleted.
class ScopedTestFile {
 public:
  explicit ScopedTestFile(std::string_view stem)
      : path_(std::filesystem::temp_directory_path() /
              (std::string(stem) + "." + std::to_string(::getpid()) + ".db")) {
    std::filesystem::remove(path_);
    if (File::create(path_.string()) != Status::Ok) {
      throw std::runtime_error("failed to create " + path_.string());
    }
    file_.emplace(path_.string());
  }

  ~ScopedTestFile() {
    file_.reset();
    std::filesystem::remove(path_);
  }

  ScopedTestFile(const ScopedTestFile&) = delete;
  ScopedTestFile& operator=(const ScopedTestFile&) = delete;

  File* get() { return &*file_; }

 private:
  std::filesystem::path path_;
  std::optional<File> file_;
};

// Writes a NULL-terminated string to the start of a page.
inline void write_string(Page& page, std::string_view s) {
  std::memcpy(page.data.data(), s.data(), s.size());
  page.data[s.size()] = '\0';
}

// Converts the page's data to a std::string.
inline std::string read_string(const Page& page) {
  return std::string(page.data.data());
}

// Returns the frame holding page `page_no` of `file`, found by scanning the
// buffer manager's frame table, or nullopt if the page is not resident.
template <typename BufMgr>
std::optional<FrameId> find_frame(const BufMgr& buf_mgr, const File* file,
                                  int page_no) {
  for (FrameId frame = 0; frame < buf_mgr.num_frames(); ++frame) {
    const FrameDescriptor& desc = buf_mgr.frame_descriptor(frame);
    if (desc.valid && desc.file == file && desc.page_no == page_no) {
      return frame;
    }
  }
  return std::nullopt;
}

}  // namespace minirel::test
