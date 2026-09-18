# File name metadata parsing test

qt_add_executable(filename_metadata_test
    main.cpp
)
yacreader_apply_build_options(filename_metadata_test)
target_include_directories(filename_metadata_test PRIVATE
    ${CMAKE_SOURCE_DIR}/YACReaderLibrary
)
target_link_libraries(filename_metadata_test PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME filename_metadata_test COMMAND filename_metadata_test)
