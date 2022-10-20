add_library(wave
  header/data_header.h
  header/data_header.cc
  header/riff_header.h
  header/riff_header.cc
  header/fmt_header.h
  header/fmt_header.cc
  header/wave_header.h
  header/wave_header.cc

  header.h
  header.cc
  header_list.h
  header_list.cc

  error.h
  file.h
  file.cc
)

# include path
target_include_directories(wave
  INTERFACE
    ${src}
)

# install rules
install(TARGETS wave
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(FILES
  file.h
  error.h
  DESTINATION include/wave
)

# tests
if (${wave_enable_tests})
  add_executable(wave_tests
    file_test.cc
    header_test.cc
  )

  add_dependencies(wave_tests
    external_googletest
  )

  if (UNIX AND NOT APPLE)
    target_link_libraries(wave_tests
      gtest
      gtest_main
      wave
      pthread
    )
  elseif(WIN32)
    target_link_libraries(wave_tests
      optimized gtest
      optimized gtest_main
      debug gtestd
      debug gtest_maind
      wave
    )
  else()
    target_link_libraries(wave_tests
      optimized gtest
      optimized gtest_main
      debug gtestd
      debug gtest_maind
      wave
    )
  endif()

  target_include_directories(wave_tests
    PUBLIC
      ${gtest_install_dir}/include
  )
  target_compile_definitions(wave_tests
    PUBLIC -DTEST_RESOURCES_PATH="${test_resource_path}"
  )
endif ()
