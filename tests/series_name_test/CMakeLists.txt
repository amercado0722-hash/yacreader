# Series name cleaning test

qt_add_executable(series_name_test
    main.cpp
)
yacreader_apply_build_options(series_name_test)
target_include_directories(series_name_test PRIVATE
    ${CMAKE_SOURCE_DIR}/YACReaderLibrary
)
target_link_libraries(series_name_test PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME series_name_test COMMAND series_name_test)
