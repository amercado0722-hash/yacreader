# EPUB page index test

qt_add_executable(epub_page_index_test
    main.cpp
)
yacreader_apply_build_options(epub_page_index_test)
target_link_libraries(epub_page_index_test PRIVATE
    Qt6::Core
    Qt6::Test
    epub_page_index
)
add_test(NAME epub_page_index_test COMMAND epub_page_index_test)
