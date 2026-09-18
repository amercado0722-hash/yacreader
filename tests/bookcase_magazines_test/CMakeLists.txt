# Magazine and issue splitting test

qt_add_executable(bookcase_magazines_test
    main.cpp
)
yacreader_apply_build_options(bookcase_magazines_test)
target_include_directories(bookcase_magazines_test PRIVATE
    ${CMAKE_SOURCE_DIR}/YACReaderLibrary
)
target_link_libraries(bookcase_magazines_test PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME bookcase_magazines_test COMMAND bookcase_magazines_test)
