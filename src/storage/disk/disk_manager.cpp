#include "storage/disk/disk_manager.h"

#include <cstring>    // C-strings are inherited from the C language and are null-terminated character arrays (char[] or char*). Use malloc and free.
#include <filesystem> // fstream manages the contents of a file (reading/writing), while filesystem manages the metadata and environment surrounding that file
#include <stdexcept>  //  Defines a set of standard exception classes that both the C++ Standard Library and user programs can use to report common errors.

namespace db {

DiskManager::DiskManager(const std::string& file_name) : file_name_(file_name) {
    open_or_create();
    initialize_page_count();
}

DiskManager::~DiskManager() {
    if (db_file_.is_open()) {
        db_file_.flush();
        db_file_.close();
    }
}

void DiskManager::open_or_create() {
    db_file_.open(file_name_, std::ios::in | std::ios::out | std::ios::binary);

    if (!db_file_.is_open()) {
        std::ofstream new_file(file_name_, std::ios::binary);
        new_file.close();

        db_file_.open(file_name_, std::ios::in | std::ios::out | std::ios::binary);

        if (!db_file_.is_open()) {
            throw std::runtime_error("Failed to open database file: " + file_name_);
        }
    }
}

void DiskManager::initialize_page_count() {
    namespace fs = std::filesystem;

    const auto file_size = fs::exists(file_name_) ? fs::file_size(file_name_) : 0;
    page_count_ = static_cast<std::uint32_t>(file_size / PAGE_SIZE);
}

void DiskManager::write_page(std::uint32_t page_id, const char* data) {
    if (page_id >= page_count_) {
        throw std::out_of_range("Cannot write to unallocated page");
    }

    // Translate page id into byte position in the file:
    // page 0 -> byte 0
    // page 1 -> byte PAGE_SIZE
    // page N -> byte N * PAGE_SIZE
    const std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;

    // Move to the start of the target page.
    db_file_.seekp(offset);
    // Write one full page of bytes.
    db_file_.write(data, PAGE_SIZE);
    // Push buffered data to disk.
    db_file_.flush();

    if (!db_file_) {
        throw std::runtime_error("Failed to write page");
    }
}

void DiskManager::read_page(std::uint32_t page_id, char* out_data) {
    // If the requested page does not exist yet, return an empty page.
    if (page_id >= page_count_) {
        std::memset(out_data, 0, PAGE_SIZE);
        return;
    }

    // Translate page id into byte position in the file:
    // page 0 -> byte 0
    // page 1 -> byte PAGE_SIZE
    // page N -> byte N * PAGE_SIZE
    const std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;

    // Move to the start of the target page.
    db_file_.seekg(offset);
    // Read one full page of bytes.
    db_file_.read(out_data, PAGE_SIZE);

    std::streamsize bytes_read = db_file_.gcount();
    // If fewer bytes were read, zero-fill the rest of the page buffer.
    if (bytes_read < PAGE_SIZE) {
        std::memset(out_data + bytes_read, 0, PAGE_SIZE - bytes_read);
    }
}

std::uint32_t DiskManager::allocate_page() {
    return page_count_++;
}

std::uint32_t DiskManager::get_page_count() const {
    return page_count_;
}

}
