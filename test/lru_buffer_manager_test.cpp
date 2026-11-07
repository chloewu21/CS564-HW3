#include "lru_buffer_manager.h"

#include <gtest/gtest.h>

#include <string>

#include "test_util.h"
#include <random>
namespace minirel {
namespace {

using test::ScopedTestFile;

// ============================================================
// Small Example Test
// ============================================================

TEST(LRUBufferManagerTest, LRUTest1) {
  // Create a temporary file and buffer pool instance for testing.
  ScopedTestFile file("lru_basic_read");
  LRUBufferManager mgr(2);

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
// Additional tests added.
// ============================================================

TEST(LRUBufferManagerTest, EvictsInLastUnpinOrder) {
  ScopedTestFile file("lru_eviction_order");
  LRUBufferManager mgr(2);
  int a, b, c;
  Page* page = nullptr;

  ASSERT_EQ(mgr.alloc_page(file.get(), a, page), Status::Ok);
  ASSERT_EQ(mgr.alloc_page(file.get(), b, page), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), a, false), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), b, false), Status::Ok);

  // Reading A removes it from the candidates. Unpinning it makes it newest.
  ASSERT_EQ(mgr.read_page(file.get(), a, page), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), a, false), Status::Ok);
  const auto victim_frame = test::find_frame(mgr, file.get(), b);
  ASSERT_TRUE(victim_frame.has_value());

  ASSERT_EQ(mgr.alloc_page(file.get(), c, page), Status::Ok);
  EXPECT_TRUE(test::find_frame(mgr, file.get(), a).has_value());
  EXPECT_FALSE(test::find_frame(mgr, file.get(), b).has_value());
  EXPECT_EQ(test::find_frame(mgr, file.get(), c), victim_frame);
  ASSERT_EQ(mgr.unpin_page(file.get(), c, false), Status::Ok);
}

TEST(LRUBufferManagerTest, DirtyEvictionPersistsData) {
  ScopedTestFile file("lru_dirty_eviction");
  LRUBufferManager mgr(1);
  int a, b;
  Page* page = nullptr;

  ASSERT_EQ(mgr.alloc_page(file.get(), a, page), Status::Ok);
  test::write_string(*page, "survives eviction");
  ASSERT_EQ(mgr.unpin_page(file.get(), a, true), Status::Ok);
  mgr.clear_buf_stats();

  // Allocating B must write dirty A back before reusing its only frame.
  ASSERT_EQ(mgr.alloc_page(file.get(), b, page), Status::Ok);
  EXPECT_FALSE(test::find_frame(mgr, file.get(), a).has_value());
  EXPECT_EQ(mgr.get_buf_stats().diskwrites, 1u);
  ASSERT_EQ(mgr.unpin_page(file.get(), b, false), Status::Ok);

  // Verify the actual file contents independently of the buffer manager.
  Page disk_page{};
  ASSERT_EQ(file.get()->read_page(a, disk_page), Status::Ok);
  EXPECT_EQ(test::read_string(disk_page), "survives eviction");

  ASSERT_EQ(mgr.read_page(file.get(), a, page), Status::Ok);
  EXPECT_EQ(test::read_string(*page), "survives eviction");
  EXPECT_EQ(mgr.get_buf_stats().diskreads, 1u);
  ASSERT_EQ(mgr.unpin_page(file.get(), a, false), Status::Ok);
}

TEST(LRUBufferManagerTest, AllPinnedRejectsReadAndAllocation) {
  ScopedTestFile file("lru_all_pinned");
  LRUBufferManager mgr(1);
  int a, b;
  Page* page = nullptr;

  // Create B directly in the file so reading it will require another frame.
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

  // Once A is unpinned, the same read should succeed.
  ASSERT_EQ(mgr.unpin_page(file.get(), a, false), Status::Ok);
  ASSERT_EQ(mgr.read_page(file.get(), b, page), Status::Ok);
  ASSERT_EQ(mgr.unpin_page(file.get(), b, false), Status::Ok);
}

}  // namespace
}  // namespace minirel
