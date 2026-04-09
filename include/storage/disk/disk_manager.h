#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include "common/constants.h"

namespace db {

// DiskManager provides page-based access to the database file.
// It maps logical page ids to fixed-size byte regions on disk.
class DiskManager {
public:
  explicit DiskManager(const std::string& file_name);
  ~DiskManager();

  // Write one full page to an already allocated page id.
  void write_page(std::uint32_t page_id, const char* data);
  // Read one full page into out_data.
  // If the page is not allocated, out_data is zero-filled.
  void read_page(std::uint32_t page_id, char* out_data);
  // Reserve a new logical page id and return it.
  std::uint32_t allocate_page();
  // Return the current number of allocated pages.
  std::uint32_t get_page_count() const;

private:
  std::fstream db_file_;
  std::string file_name_;
  std::uint32_t page_count_ = 0;
  
  // Open the database file if it exists, or create it first.
  void open_or_create();
  // Initialize page_count_ from the current file size.
  void initialize_page_count();
};

}  // namespace db
