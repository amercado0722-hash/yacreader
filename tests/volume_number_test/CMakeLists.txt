# Volume number parsing test

qt_add_executable(volume_number_test
    main.cpp
)
yacreader_apply_build_options(volume_number_test)
target_include_directories(volume_number_test PRIVATE
    ${CMAKE_SOURCE_DIR}/YACReaderLibrary
)
target_link_libraries(volume_number_test PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME volume_number_test COMMAND volume_number_test)
