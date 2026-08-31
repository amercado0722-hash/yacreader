# Series matching confidence test

qt_add_executable(series_match_test
    main.cpp
)
yacreader_apply_build_options(series_match_test)
target_include_directories(series_match_test PRIVATE
    ${CMAKE_SOURCE_DIR}/YACReaderLibrary
)
target_link_libraries(series_match_test PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME series_match_test COMMAND series_match_test)
