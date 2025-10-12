# json
add_subdirectory(json)

# Catch2
if (BUILD_TESTS)
  add_subdirectory(Catch2)
endif()
