# Damaged archive regression test

qt_add_executable(damaged_archive_test
    main.cpp
)
yacreader_apply_build_options(damaged_archive_test)
target_link_libraries(damaged_archive_test PRIVATE
    Qt6::Core
    Qt6::Test
    cbx_backend
)
if(WIN32)
    target_link_libraries(damaged_archive_test PRIVATE oleaut32 ole32)
endif()
add_test(NAME damaged_archive_test COMMAND damaged_archive_test)
