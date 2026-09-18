# Which folders the sorter is allowed to move

qt_add_executable(series_sorter_test
    main.cpp
)
yacreader_apply_build_options(series_sorter_test)
target_include_directories(series_sorter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/YACReaderLibrary
)
target_link_libraries(series_sorter_test PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME series_sorter_test COMMAND series_sorter_test)
