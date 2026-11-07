#include "file.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <system_error>
#include <utility>

namespace minirel {

namespace {

// Header page layout
struct FileHeader {
  int next_free;   // page number of first page on the free list, or -1 if it doesn't exist
  int first_page;  // page number of first data page, or -1 if it doesn't exist
  int num_pages;   // total number of pages in the file, including the header
};

static_assert(sizeof(FileHeader) <= MINIREL_PAGE_SIZE);

FileHeader decode_header(const Page& page) {
  FileHeader header;
  std::memcpy(&header, page.data.data(), sizeof header);
  return header;
}

void encode_header(const FileHeader& header, Page& page) {
  std::memcpy(page.data.data(), &header, sizeof header);
}

// Disposed pages store the next free page number in their first bytes.
int decode_next_free(const Page& page) {
  int next;
  std::memcpy(&next, page.data.data(), sizeof next);
  return next;
}

void encode_next_free(int next, Page& page) {
  std::memcpy(page.data.data(), &next, sizeof next);
}

off_t offset_of(int page_no) {
  return static_cast<off_t>(page_no) * static_cast<off_t>(MINIREL_PAGE_SIZE);
}

}  // namespace

Status File::create(const std::string& path) {
  if (path.empty()) return Status::BadFile;

  int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0666);
  if (fd < 0) return errno == EEXIST ? Status::FileExists : Status::UnixError;

  // An empty file contains just the header page.
  Page page;
  encode_header({/*next_free=*/-1, /*first_page=*/-1, /*num_pages=*/1}, page);
  bool ok = ::pwrite(fd, page.data.data(), MINIREL_PAGE_SIZE, 0) ==
            static_cast<ssize_t>(MINIREL_PAGE_SIZE);
  ok = (::close(fd) == 0) && ok;
  return ok ? Status::Ok : Status::UnixError;
}

Status File::destroy(const std::string& path) {
  if (path.empty()) return Status::BadFile;
  return std::remove(path.c_str()) == 0 ? Status::Ok : Status::UnixError;
}

File::File(std::string path) : path_(std::move(path)) {
  fd_ = ::open(path_.c_str(), O_RDWR);
  if (fd_ < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "failed to open " + path_);
  }
}

File::~File() { close(); }

File::File(File&& other) noexcept
    : path_(std::move(other.path_)), fd_(std::exchange(other.fd_, -1)) {}

File& File::operator=(File&& other) noexcept {
  if (this != &other) {
    close();
    path_ = std::move(other.path_);
    fd_ = std::exchange(other.fd_, -1);
  }
  return *this;
}

void File::close() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

Status File::allocate_page(int& page_no) {
  Page header_page;
  if (Status s = read_raw(0, header_page); s != Status::Ok) return s;
  FileHeader header = decode_header(header_page);

  if (header.next_free != -1) {
    // Reuse the first page on the free list.
    page_no = header.next_free;
    Page first_free;
    if (Status s = read_raw(page_no, first_free); s != Status::Ok) return s;
    header.next_free = decode_next_free(first_free);
  } else {
    // No free pages, so extend the file.
    page_no = header.num_pages;
    if (Status s = write_raw(page_no, Page{}); s != Status::Ok) return s;
    header.num_pages++;
    if (header.first_page == -1) header.first_page = page_no;
  }

  encode_header(header, header_page);
  return write_raw(0, header_page);
}

Status File::dispose_page(int page_no) {
  if (page_no < 1) return Status::BadPageNo;

  Page header_page;
  if (Status s = read_raw(0, header_page); s != Status::Ok) return s;
  FileHeader header = decode_header(header_page);

  // The first data page cannot be disposed of.
  if (header.first_page == page_no || page_no >= header.num_pages) {
    return Status::BadPageNo;
  }

  // Push the page onto the front of the free list.
  Page away;
  encode_next_free(header.next_free, away);
  header.next_free = page_no;

  if (Status s = write_raw(page_no, away); s != Status::Ok) return s;
  encode_header(header, header_page);
  return write_raw(0, header_page);
}

Status File::read_page(int page_no, Page& page) const {
  if (page_no < 1) return Status::BadPageNo;
  return read_raw(page_no, page);
}

Status File::write_page(int page_no, const Page& page) {
  if (page_no < 1) return Status::BadPageNo;
  return write_raw(page_no, page);
}

Status File::get_first_page(int& page_no) const {
  Page header_page;
  if (Status s = read_raw(0, header_page); s != Status::Ok) return s;
  page_no = decode_header(header_page).first_page;
  return Status::Ok;
}

Status File::read_raw(int page_no, Page& page) const {
  ssize_t n =
      ::pread(fd_, page.data.data(), MINIREL_PAGE_SIZE, offset_of(page_no));
  return n == static_cast<ssize_t>(MINIREL_PAGE_SIZE) ? Status::Ok
                                                      : Status::UnixError;
}

Status File::write_raw(int page_no, const Page& page) {
  ssize_t n =
      ::pwrite(fd_, page.data.data(), MINIREL_PAGE_SIZE, offset_of(page_no));
  return n == static_cast<ssize_t>(MINIREL_PAGE_SIZE) ? Status::Ok
                                                      : Status::UnixError;
}

}  // namespace minirel
