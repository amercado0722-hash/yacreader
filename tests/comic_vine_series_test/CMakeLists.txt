# Reading Comic Vine's answer

qt_add_executable(comic_vine_series_test
    main.cpp
)
yacreader_apply_build_options(comic_vine_series_test)
target_include_directories(comic_vine_series_test PRIVATE
    ${CMAKE_SOURCE_DIR}/YACReaderLibrary
)
target_link_libraries(comic_vine_series_test PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME comic_vine_series_test COMMAND comic_vine_series_test)
