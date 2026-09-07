add_executable(D1Hellforge WIN32
	D1Hellforge.rc
	content_profile.hpp
  item_advanced_editor.cpp
  item_advanced_editor.hpp
  item_display.cpp
  item_display.hpp
  item_display_win32.cpp
  item_display_win32.hpp
  devx_adapter.cpp
  devx_adapter.hpp
  inventory_renderer.cpp
  inventory_renderer.hpp
  item_catalog.cpp
  item_catalog.hpp
  item_transfer.cpp
  item_transfer.hpp
  item_generator.cpp
  item_generator.hpp
  item_workshop.cpp
  item_workshop.hpp
  legacy_item_import.cpp
  legacy_item_import.hpp
  ${PROJECT_SOURCE_DIR}/Source/utils/item_file.cpp
  main_windows.cpp
  save_document.cpp
  save_document.hpp
  stash_document.cpp
  stash_document.hpp
  ui_theme_win32.cpp
  ui_theme_win32.hpp)

target_include_directories(D1Hellforge PRIVATE
  "${PROJECT_SOURCE_DIR}/Source")

target_link_dependencies(D1Hellforge PRIVATE libdevilutionx)
target_link_libraries(D1Hellforge PRIVATE comctl32 comdlg32 gdiplus)

set_target_properties(D1Hellforge PROPERTIES
  OUTPUT_NAME "D1Hellforge")

add_custom_command(TARGET D1Hellforge POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:D1Hellforge>
    $<TARGET_FILE_DIR:D1Hellforge>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.txt"
    $<TARGET_FILE_DIR:D1Hellforge>
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:D1Hellforge>/assets"
  COMMAND_EXPAND_LISTS)
