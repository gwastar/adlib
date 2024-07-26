project(adlib-tests)

set(TESTS
  array
  avl_tree
  btree_map
  btree_set
  charconv
  dbuf
  dstring
  hash
  hashmap
  hashset
  heap
  json
  random
  rb_tree
  stringview
  uint128
  utils
)

get_target_property(TESTING_INCLUDES testing INCLUDE_DIRECTORIES)

# build a static library out of each source file so that we don't compile every file twice
foreach(TEST IN LISTS TESTS)
  add_library(_${TEST} STATIC ${TEST}.c)
  target_compile_definitions(_${TEST} PRIVATE __ADLIB_TESTS__)
  target_link_libraries(_${TEST} ad-static m)
  target_include_directories(_${TEST} PRIVATE ${TESTING_INCLUDES})
endforeach()

# cmake doesn't allow add_executable with no sources
set(DUMMY_FILE ${CMAKE_CURRENT_BINARY_DIR}/null.c)
file(WRITE ${DUMMY_FILE} "")

add_executable(testsuite ${DUMMY_FILE})
target_compile_definitions(testsuite PRIVATE __ADLIB_TESTS__)
list(TRANSFORM TESTS PREPEND _ OUTPUT_VARIABLE PREFIXED_TESTS)
target_link_libraries(testsuite
  -Wl,--whole-archive
  ${PREFIXED_TESTS}
  -Wl,--no-whole-archive
  testing
)

if(${ENABLE_SINGLE_TESTS})
  foreach(TEST IN LISTS TESTS)
    add_executable(${TEST} ${DUMMY_FILE})
    target_link_libraries(${TEST}
      -Wl,--whole-archive
      _${TEST}
      -Wl,--no-whole-archive
      single_testing
    )
    add_test(NAME ${TEST} COMMAND $<TARGET_FILE:${TEST}>)
  endforeach()
endif()
