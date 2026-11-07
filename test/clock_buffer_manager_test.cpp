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

// ============================================================
// Additional tests added with Codex assistance start here.
// The provided ClockTest1 above is unchanged.
// ============================================================

TEST(ClockBufferManagerTest, SecondChanceAndReadRefresh) {
  ScopedTestFile file("clock_second_chance");
  ClockBufferManager mgr(3);
  int a, b, c, d, e;
  Page* page = nullptr;
  ASSERT_EQ(mgr.alloc_page(file.get(), a, page), Status::Ok);
  ASSERT_EQ(mgr.alloc_page(file.get(), b, page), Status::Ok);
  ASSERT_EQ(mgr.alloc_page(file.get(), c, page), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), a, false), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), b, false), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), c, false), Status::Ok);

  // All three get a second chance; the next sweep replaces A.
  const auto a_frame = test::find_frame(mgr, file.get(), a);
  ASSERT_TRUE(a_frame.has_value());
  ASSERT_EQ(mgr.alloc_page(file.get(), d, page), Status::Ok);
  EXPECT_FALSE(test::find_frame(mgr, file.get(), a).has_value());
  EXPECT_EQ(test::find_frame(mgr, file.get(), d), a_frame);
  ASSERT_EQ(mgr.unpin_page(file.get(), d, false), Status::Ok);

  // B and C had their bits cleared. Reading B gives it another chance,
  // so the hand must skip B and replace C instead.
  ASSERT_EQ(mgr.read_page(file.get(), b, page), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), b, false), Status::Ok);
  const auto c_frame = test::find_frame(mgr, file.get(), c);
  ASSERT_TRUE(c_frame.has_value());
  ASSERT_EQ(mgr.alloc_page(file.get(), e, page), Status::Ok);
  EXPECT_TRUE(test::find_frame(mgr, file.get(), b).has_value());
  EXPECT_FALSE(test::find_frame(mgr, file.get(), c).has_value());
  EXPECT_EQ(test::find_frame(mgr, file.get(), e), c_frame);
  ASSERT_EQ(mgr.unpin_page(file.get(), e, false), Status::Ok);
}

TEST(ClockBufferManagerTest, SkipsPinnedPagesAndContinuesSweeping) {
  ScopedTestFile file("clock_pinned_reference");
  ClockBufferManager mgr(2);
  int a, b, c, d;
  Page* page = nullptr;
  ASSERT_EQ(mgr.alloc_page(file.get(), a, page), Status::Ok);
  ASSERT_EQ(mgr.alloc_page(file.get(), b, page), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), b, false), Status::Ok);

  // A remains pinned while the hand passes it twice and replaces B.
  ASSERT_EQ(mgr.alloc_page(file.get(), c, page), Status::Ok);
  EXPECT_TRUE(test::find_frame(mgr, file.get(), a).has_value());
  EXPECT_FALSE(test::find_frame(mgr, file.get(), b).has_value());
  ASSERT_EQ(mgr.unpin_page(file.get(), a, false), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), c, false), Status::Ok);

  // A's preserved reference bit gives it a second chance. Both bits are
  // cleared on the first sweep, so the second sweep selects A.
  ASSERT_EQ(mgr.alloc_page(file.get(), d, page), Status::Ok);
  EXPECT_FALSE(test::find_frame(mgr, file.get(), a).has_value());
  EXPECT_TRUE(test::find_frame(mgr, file.get(), c).has_value());
  ASSERT_EQ(mgr.unpin_page(file.get(), d, false), Status::Ok);
}

TEST(ClockBufferManagerTest, DirtyEvictionPersistsData) {
  ScopedTestFile file("clock_dirty_eviction");
  ClockBufferManager mgr(1);
  int a, b;
  Page* page = nullptr;
  ASSERT_EQ(mgr.alloc_page(file.get(), a, page), Status::Ok);
  test::write_string(*page, "survives clock eviction");
  ASSERT_EQ(mgr.unpin_page(file.get(), a, true), Status::Ok);
  mgr.clear_buf_stats();

  ASSERT_EQ(mgr.alloc_page(file.get(), b, page), Status::Ok);
  EXPECT_FALSE(test::find_frame(mgr, file.get(), a).has_value());
  EXPECT_EQ(mgr.get_buf_stats().diskwrites, 1u);
  ASSERT_EQ(mgr.unpin_page(file.get(), b, false), Status::Ok);

  Page disk_page{};
  ASSERT_EQ(file.get()->read_page(a, disk_page), Status::Ok);
  EXPECT_EQ(test::read_string(disk_page), "survives clock eviction");
  ASSERT_EQ(mgr.read_page(file.get(), a, page), Status::Ok);
  EXPECT_EQ(test::read_string(*page), "survives clock eviction");
  EXPECT_EQ(mgr.get_buf_stats().diskreads, 1u);
  ASSERT_EQ(mgr.unpin_page(file.get(), a, false), Status::Ok);
}

TEST(ClockBufferManagerTest, AllPinnedRejectsReadAndAllocation) {
  ScopedTestFile file("clock_all_pinned");
  ClockBufferManager mgr(1);
  int a, b;
  Page* page = nullptr;
  ASSERT_EQ(file.get()->allocate_page(b), Status::Ok);
  ASSERT_EQ(mgr.alloc_page(file.get(), a, page), Status::Ok);
  const auto original_frame = test::find_frame(mgr, file.get(), a);
  ASSERT_TRUE(original_frame.has_value());
  mgr.clear_buf_stats();

  EXPECT_EQ(mgr.read_page(file.get(), b, page), Status::BufferExceeded);
  int new_page;
  EXPECT_EQ(mgr.alloc_page(file.get(), new_page, page), Status::BufferExceeded);
  EXPECT_EQ(test::find_frame(mgr, file.get(), a), original_frame);
  EXPECT_EQ(mgr.frame_descriptor(*original_frame).pin_count, 1);
  EXPECT_EQ(mgr.get_buf_stats().accesses, 0u);
  EXPECT_EQ(mgr.get_buf_stats().diskreads, 0u);
  EXPECT_EQ(mgr.get_buf_stats().diskwrites, 0u);

  ASSERT_EQ(mgr.unpin_page(file.get(), a, false), Status::Ok);
  ASSERT_EQ(mgr.read_page(file.get(), b, page), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), b, false), Status::Ok);
}

}  // namespace
}  // namespace minirel
