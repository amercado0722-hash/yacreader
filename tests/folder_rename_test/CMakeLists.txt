qt_add_executable(folder_rename_test
    main.cpp
)
yacreader_apply_build_options(folder_rename_test)
target_link_libraries(folder_rename_test PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
    db_helper
    library_common
)
