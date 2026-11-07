#pragma once

#include <ostream>
#include <string_view>

namespace minirel {

// Status codes returned by the File and BufferManager classes.
enum class Status {
  Ok,

  // File errors
  BadFilePtr, // file pointer is null
  BadFile, // file path is empty or otherwise invalid
  FileExists, // file already exists when trying to create it
  UnixError, // some other error from the underlying OS
  BadPageNo, // page number is invalid (e.g., 0 or negative)

  // Buffer manager errors
  PageNotFound,    // (file, page_no) is not resident in the buffer pool
  BufferExceeded,  // every frame is pinned; nothing can be evicted
  PageNotPinned,   // unpin requested on a page whose pin count is already 0
  PagePinned,      // operation requires the page to be unpinned
};

std::string_view to_string(Status status);

std::ostream& operator<<(std::ostream& os, Status status);

}  // namespace minirel
