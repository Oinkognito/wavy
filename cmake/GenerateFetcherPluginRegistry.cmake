function(generate_fetcher_plugin_registry OUTPUT_HEADER ENTRIES)
  string(REPLACE "\n" ";" FETCHER_PLUGIN_LIST "${ENTRIES}")
  set(backend_list "")

  foreach(entry ${FETCHER_PLUGIN_LIST})
    string(STRIP "${entry}" entry)
    if(entry STREQUAL "")
      continue()
    endif()

    string(REPLACE "|" ";" parts "${entry}")
    list(LENGTH parts part_count)

    if(NOT part_count EQUAL 2)
      message(WARNING "Invalid plugin entry format: '${entry}'")
      continue()
    endif()

    list(GET parts 0 plugin_name)
    list(GET parts 1 plugin_lib)

    set(backend_list "${backend_list}  {.name=\"${plugin_name}\", .plugin_lib=\"${plugin_lib}\", .plugin_path=\"lib${plugin_lib}.so\"},\n")
  endforeach()

  # Configure the header from the template
  set(FETCHER_FACTORY_LIST "${backend_list}")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libwavy/fetcherConfig.h.in ${OUTPUT_HEADER} @ONLY)
endfunction()
