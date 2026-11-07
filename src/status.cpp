#include "status.h"

namespace minirel {

std::string_view to_string(Status status) {
  switch (status) {
    case Status::Ok:             return "no error";
    case Status::BadFilePtr:     return "bad file pointer";
    case Status::BadFile:        return "bad filename";
    case Status::FileExists:     return "file exists already";
    case Status::UnixError:      return "Unix error";
    case Status::BadPageNo:      return "bad page number";
    case Status::PageNotFound:   return "page not in buffer pool";
    case Status::BufferExceeded: return "buffer pool full";
    case Status::PageNotPinned:  return "page not pinned";
    case Status::PagePinned:     return "page still pinned";
  }
  return "unknown status";
}

std::ostream& operator<<(std::ostream& os, Status status) {
  return os << to_string(status);
}

}  // namespace minirel
