cmake_minimum_required(VERSION 3.16)

project(DatabaseEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(db_storage
    src/index/bplustree.cpp
    src/index/bplustree_leaf.cpp
    src/index/bplustree_internal.cpp
    src/storage/disk/disk_manager.cpp
    src/storage/heap/heap_file.cpp
    src/storage/page/slotted_page.cpp
    src/storage/page/var_record_serializer.cpp
    src/storage/cache/page_cache_manager.cpp
    src/table/table.cpp
    src/table/row_serializer.cpp
    src/catalog/catalog.cpp
    src/catalog/catalog_serializer.cpp
    src/sql/tokenizer.cpp
    src/sql/parser.cpp
    src/sql/ast.cpp
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

function(add_db_test test_name test_source)
    add_executable(${test_name}
        ${test_source}
    )

    target_compile_definitions(${test_name}
        PRIVATE
            DB_TESTING
    )

    target_link_libraries(${test_name}
        PRIVATE
            db_storage
    )
endfunction()

add_db_test(SlottedPageTest tests/slotted_page_test.cpp)
add_db_test(SlottedPageFullTest tests/slotted_page_full_test.cpp)
add_db_test(SlottedPageDeleteTest tests/slotted_page_delete_test.cpp)
add_db_test(SlottedPageVarRecordTest tests/slotted_page_var_record_test.cpp)
add_db_test(SlottedPageUpdateTest tests/slotted_page_update_test.cpp)
add_db_test(SlottedPageCompactionTest tests/slotted_page_compaction_test.cpp)
add_db_test(HeapFileTest tests/heap_file_test.cpp)
add_db_test(HeapFileRowIdTest tests/heap_file_row_id_test.cpp)
add_db_test(HeapFileVarRecordTest tests/heap_file_var_record_test.cpp)
add_db_test(VarRecordSerializerTest tests/var_record_serializer_test.cpp)
add_db_test(PageCacheManagerTest tests/page_cache_manager_test.cpp)
add_db_test(BPlusTreeLeafPageTest tests/index/bplustree_leaf_page_test.cpp)
add_db_test(BPlusTreeInternalPageTest tests/index/bplustree_internal_page_test.cpp)
add_db_test(BPlusTreeSearchTest tests/index/bplustree_search_test.cpp)
add_db_test(BPlusTreeInsertTest tests/index/bplustree_insert_test.cpp)
add_db_test(TableTest tests/table/table_test.cpp)
add_db_test(RowSerializerTest tests/table/row_serializer_test.cpp)
add_db_test(CatalogTest tests/catalog/catalog_test.cpp)
add_db_test(ParserTest tests/sql/parser_test.cpp)