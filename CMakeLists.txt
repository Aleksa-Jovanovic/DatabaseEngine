cmake_minimum_required(VERSION 3.16)

project(DatabaseEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(db_storage
    src/storage/disk/disk_manager.cpp
    src/storage/heap/heap_file.cpp
    src/storage/page/slotted_page.cpp
    src/storage/page/var_record_serializer.cpp
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

add_executable(SlottedPageTest
    tests/slotted_page_test.cpp
)

target_link_libraries(SlottedPageTest
    PRIVATE
        db_storage
)

add_executable(SlottedPageFullTest
    tests/slotted_page_full_test.cpp
)

target_link_libraries(SlottedPageFullTest
    PRIVATE
        db_storage
)

add_executable(SlottedPageDeleteTest
    tests/slotted_page_delete_test.cpp
)

target_link_libraries(SlottedPageDeleteTest
    PRIVATE
        db_storage
)

add_executable(SlottedPageVarRecordTest
    tests/slotted_page_var_record_test.cpp
)

target_link_libraries(SlottedPageVarRecordTest
    PRIVATE
        db_storage
)

add_executable(SlottedPageUpdateTest
    tests/slotted_page_update_test.cpp
)

target_link_libraries(SlottedPageUpdateTest
    PRIVATE
        db_storage
)

add_executable(HeapFileTest
    tests/heap_file_test.cpp
)

target_link_libraries(HeapFileTest
    PRIVATE
        db_storage
)

add_executable(VarRecordSerializerTest
    tests/var_record_serializer_test.cpp
)

target_link_libraries(VarRecordSerializerTest
    PRIVATE
        db_storage
)
