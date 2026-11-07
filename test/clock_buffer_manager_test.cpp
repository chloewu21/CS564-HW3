#include "clock_buffer_manager.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "test_util.h"

namespace minirel {
namespace {

using test::ScopedTestFile;

// ============================================================
// Small Example Test
// ============================================================

TEST(ClockBufferManagerTest, ClockTest1) {
  // Create a temporary file and buffer pool instance for testing.
  ScopedTestFile file("clock_basic_read");
  ClockBufferManager mgr(2);

  int page_no;
  Page* page = nullptr;

  // Allocate one page and write a string to it.
  ASSERT_EQ(mgr.alloc_page(file.get(), page_no, page), Status::Ok);
  test::write_string(*page, "hello");

  // Obtain the frame of the page. 
  auto frame = test::find_frame(mgr, file.get(), page_no);
  ASSERT_TRUE(frame.has_value());

  // When we re-read the page, we check to make sure we get the same frame
  // from the buffer pool.
  Page* cached = nullptr;
  ASSERT_EQ(mgr.read_page(file.get(), page_no, cached), Status::Ok);
  EXPECT_EQ(test::find_frame(mgr, file.get(), page_no), frame);

  // The page should be the same page as the one we allocated and wrote to.
  EXPECT_EQ(cached, page);

  // The pin count of the page should be 2 due to the alloc_page
  // call and the read_page call.
  EXPECT_EQ(mgr.frame_descriptor(*frame).pin_count, 2);

  // We unpin the page twice, one for each pin count increment, and 
  // flush it to disk.
  ASSERT_EQ(mgr.unpin_page(file.get(), page_no, false), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), page_no, true), Status::Ok);
  ASSERT_EQ(mgr.flush_file(file.get()), Status::Ok);

  // When we re-read the page, we should be able to read the data properly.
  ASSERT_EQ(mgr.read_page(file.get(), page_no, page), Status::Ok);
  EXPECT_EQ(test::read_string(*page), "hello");

  // Cleanup by unpinning the page after the read_page call.
  ASSERT_EQ(mgr.unpin_page(file.get(), page_no, false), Status::Ok);
}

}  // namespace
}  // namespace minirel