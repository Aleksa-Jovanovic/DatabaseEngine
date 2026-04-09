cmake_minimum_required(VERSION 3.16)

project(DatabaseEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(db_storage
    src/storage/disk/disk_manager.cpp
    src/storage/heap/heap_file.cpp
)

target_include_directories(db_storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(DatabaseEngine
    src/main.cpp
)

target_link_libraries(DatabaseEngine
    PRIVATE
        db_storage
)
