#include "clock_buffer_manager.h"

namespace minirel {

ClockBufferManager::ClockBufferManager(std::size_t num_frames)
    : BufferManager(num_frames), ref_bits_(num_frames, 0) {}

ClockBufferManager::~ClockBufferManager() {
  for (FrameId i = 0; i < frames_.size(); i++) {
    if (frames_[i].valid && frames_[i].dirty) {
      Status status = frames_[i].file->write_page(frames_[i].page_no, pool_[i]);
      if (status == Status::Ok) {
        buf_stats_.diskwrites++;
      }
    }
  }
}

Status ClockBufferManager::read_page(File* file, int page_no, Page*& page) {
  if (file == nullptr) {
    return Status::BadFilePtr;
  }
  PageKey key{file, page_no}; // look up page in page_table_ using file and and page #
  auto it = page_table_.find(key);
  if (it != page_table_.end()) { // page already in buffer pool
    FrameId frame_id = it->second;
    frames_[frame_id].pin_count++; // increment pin count
    ref_bits_[frame_id] = 1;
    page = &pool_[frame_id]; // set page to point to entry in pool
    buf_stats_.accesses++; // update buffer stats
  }
  else {
    FrameId frame_id;
    Status status = acquire_frame(frame_id); // try to acquire a frame
    if (status != Status::Ok) {
      return status; // return error if no frame could be acquired
    }
    status = file->read_page(page_no, pool_[frame_id]); // read page into frame
    if (status != Status::Ok) {
      return status; // return error if read_page failed
    }
    ref_bits_[frame_id] = 1;
    frames_[frame_id].file = file; // set frame metadata
    frames_[frame_id].page_no = page_no;
    frames_[frame_id].pin_count = 1;
    frames_[frame_id].dirty = false;
    frames_[frame_id].valid = true;
    page_table_[key] = frame_id; // add mapping to page_table_
    page = &pool_[frame_id]; // set page to point to entry in pool
    buf_stats_.accesses++; // update buffer stats
    buf_stats_.diskreads++; // update buffer stats
  }
  return Status::Ok;
}

Status ClockBufferManager::unpin_page(File* file, int page_no, bool dirty) {
  PageKey key{file, page_no}; // look up page in page table
  auto it = page_table_.find(key);
  if (it == page_table_.end()) { // page not in buffer pool
    return Status::PageNotFound;
  }
  if (frames_[it->second].pin_count == 0) { // page already unpinned
    return Status::PageNotPinned;
  }
  frames_[it->second].pin_count--; // decrement pin count
  
  if (dirty) { // if dirty, mark frame as dirty
    frames_[it->second].dirty = true;
  }
  return Status::Ok;
}

Status ClockBufferManager::alloc_page(File* file, int& page_no, Page*& page) {
  if (file == nullptr) {
    return Status::BadFilePtr;
  }
  FrameId frame_id;
  Status status = acquire_frame(frame_id); // try to acquire a frame
  if (status != Status::Ok) {
    return status; // return error if no frame could be acquired
  }

  status = file->allocate_page(page_no); // allocate new page in file
  if (status != Status::Ok) {
    return status; // return error if allocate_page failed
  }
  pool_[frame_id] = Page{}; // zero the page in buffer pool

  ref_bits_[frame_id] = 1; // set reference bit to 1
  frames_[frame_id].file = file; // set frame metadata
  frames_[frame_id].page_no = page_no;
  frames_[frame_id].pin_count = 1;
  frames_[frame_id].dirty = false;
  frames_[frame_id].valid = true;
  PageKey key{file, page_no};
  page_table_[key] = frame_id; // add mapping to page_table_
  page = &pool_[frame_id]; // set page to point to entry in pool
  buf_stats_.accesses++; // update buffer stats
  return Status::Ok;
}

Status ClockBufferManager::flush_file(const File* file) {
  if (file == nullptr) {
    return Status::BadFilePtr;
  }
  // Check if any pages are pinned
  for (FrameId i = 0; i < frames_.size(); i++) {
    if (frames_[i].file == file && frames_[i].pin_count > 0) {
      return Status::PagePinned; // return error if any page is pinned
    }
  }

  for (FrameId i = 0; i < frames_.size(); i++) {
    if (frames_[i].file == file && frames_[i].valid) {
      if (frames_[i].dirty) {
        Status status = frames_[i].file->write_page(frames_[i].page_no, pool_[i]); // write page to disk
        if (status != Status::Ok) {
          return status; // return error if write_page failed
        }
        buf_stats_.diskwrites++; 
      }
      PageKey key{frames_[i].file, frames_[i].page_no};
      page_table_.erase(key); // get rid of page from page table
      frames_[i] = FrameDescriptor{}; // reset frame metadata
      ref_bits_[i] = 0; // reset reference bit
    }
  }
  return Status::Ok;
}

Status ClockBufferManager::dispose_page(File* file, int page_no) {
  if (file == nullptr) {
    return Status::BadFilePtr;
  }
  PageKey key{file, page_no}; // look up page in page table
  auto it = page_table_.find(key);
  if (it != page_table_.end()) { // page is in buffer pool
    FrameId frame_id = it->second;
    if (frames_[frame_id].pin_count > 0) { // page is pinned
      return Status::PagePinned;
    }
    page_table_.erase(key); // remove from page table
    frames_[frame_id] = FrameDescriptor{}; // reset frame metadata
    ref_bits_[frame_id] = 0; // reset reference bit
  }
  return file->dispose_page(page_no); // dispose of page in file
}

void ClockBufferManager::print_self() const {}

Status ClockBufferManager::acquire_frame(FrameId& frame_id) {
  for (int i = 0; i < 2; i++) {
    for (std::size_t j = 0; j < frames_.size(); j++) {
      FrameId curr = clock_hand_;
      clock_hand_ = (clock_hand_ + 1) % frames_.size(); // advance clock hand to next frame

      if (!frames_[curr].valid) { // empty frame found
        ref_bits_[curr] = 0; // set reference bit to 0
        frame_id = curr;
        return Status::Ok;
      }
      if (frames_[curr].pin_count == 0) { // unpinned frame
        if (ref_bits_[curr] == 0) {
          if (frames_[curr].dirty) { // if dirty, write back to disk
            Status status = frames_[curr].file->write_page(frames_[curr].page_no, pool_[curr]);
            if (status != Status::Ok) {
              return status;
            }
            buf_stats_.diskwrites++; // update buffer stats
          }
          PageKey old_key{frames_[curr].file, frames_[curr].page_no};
          page_table_.erase(old_key); // remove from page table
          frames_[curr] = FrameDescriptor{}; // reset frame metadata
          frame_id = curr;
          return Status::Ok;
        } 
        else {
          ref_bits_[curr] = 0; // set reference bit to 0
        }
      }
    }
  }
  return Status::BufferExceeded; // no empty frames or unpinned pages
}

}  // namespace minirel
