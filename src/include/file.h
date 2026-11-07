#pragma once

#include <string>

#include "page.h"
#include "status.h"

namespace minirel {

/**
 * @brief File class that manages a database file on disk. A file is a
 * sequence of fixed-size pages, which are 1024 bytes each. The first
 * page (page 0) is a header page that is used internally by the File class
 * to track metadata. The second page (and onwards) are data pages.
 */
class File {
 public:
  /**
   * @brief Creates a new, empty file on disk at `path`. The new file contains
   * only the header page, and has no data pages. This method does not open the
   * file. A File object must be constructed with the path to open the file.
   *
   * This function returns BadFile if `path` is empty, FileExists if a file
   * already exists at `path`, and UnixError if the file could not be created
   * or written. It returns Ok on success.
   *
   * @param path Path of the file to be created
   * @return Status
   */
  static Status create(const std::string& path);

  /**
   * @brief Deletes the file at `path` from disk. 
   *
   * This function returns BadFile if `path` is empty, and UnixError if the
   * file could not be removed. It returns Ok on success.
   *
   * @param path Path of the file to be deleted
   * @return Status
   */
  static Status destroy(const std::string& path);

  /**
   * @brief Opens an existing file at `path` for reading and writing. Prior to
   * using this constructor, the file must have been created with File::create.
   *
   * This constructor throws std::system_error if the file cannot be opened.
   *
   * @param path Path of the file to be opened
   */
  explicit File(std::string path);

  /**
   * @brief Destructor for the File class. 
   * This method closes the file if it is open.
   */
  ~File();

  // Files cannot be copied, since each File object owns an open file
  // descriptor. Files can be moved, but keep in mind that moving a File gives
  // it a new address.
  File(const File&) = delete;
  File& operator=(const File&) = delete;
  File(File&& other) noexcept;
  File& operator=(File&& other) noexcept;

  /**
   * @brief Allocates a new page in the file. If a previously disposed page is
   * available on the free list, it is reused. Otherwise, the file is extended
   * by one zero-filled page. On success, `page_no` contains the number of the
   * newly allocated page. The contents of a reused page on disk are not cleared.
   *
   * This function returns UnixError if the underlying disk I/O fails. It
   * returns Ok on success.
   *
   * @param page_no Page number of the newly allocated page
   * @return Status
   */
  Status allocate_page(int& page_no);

  /**
   * @brief Disposes of a page in the file, returning it to the file's free list
   * so that it can be reused by a later call to allocate_page. The page's
   * contents on disk are overwritten.
   *
   * This function returns BadPageNo if `page_no` is less than 1, is past the
   * end of the file, or is the file's first data page (which cannot be
   * disposed of). It returns UnixError if the underlying disk I/O fails. It
   * returns Ok on success.
   *
   * @param page_no Page number of the page to be disposed of
   * @return Status
   */
  Status dispose_page(int page_no);

  /**
   * @brief Reads page `page_no` from disk into `page`. 
   * Each call of `read_page` performs one disk read.
   *
   * This function returns BadPageNo if `page_no` is less than 1, and
   * UnixError if the page could not be read (for example, if `page_no` is
   * past the end of the file). It returns Ok on success.
   *
   * @param page_no Page number of the page to be read
   * @param page Page that the contents are read into
   * @return Status
   */
  Status read_page(int page_no, Page& page) const;

  /**
   * @brief Writes `page` to disk as page `page_no`.
   * Each call to `write_page` performs one disk write.
   *
   * This function returns BadPageNo if `page_no` is less than 1, and
   * UnixError if the page could not be written. It returns Ok on success.
   *
   * @param page_no Page number of the page to be written
   * @param page Page whose contents are written to disk
   * @return Status
   */
  Status write_page(int page_no, const Page& page);

  /**
   * @brief Gets the number of the first data page in the file. On success,
   * `page_no` contains the first data page's number, or INVALID_PAGE_NO if
   * the file has no data pages. The first data page is the first page ever
   * allocated in the file, and it is never disposed of.
   *
   * This function returns UnixError if the file's header page could not be
   * read. It returns Ok on success.
   *
   * @param page_no Page number of the first data page
   * @return Status
   */
  Status get_first_page(int& page_no) const;

  /**
   * @brief Returns the path the file was opened with.
   * @return The file's path.
   */
  const std::string& name() const { return path_; }

 private:
  // Raw I/O functions used in the File class implementation.
  Status read_raw(int page_no, Page& page) const;
  Status write_raw(int page_no, const Page& page);
  void close() noexcept;

  std::string path_;
  int fd_ = -1;
};

}  // namespace minirel
