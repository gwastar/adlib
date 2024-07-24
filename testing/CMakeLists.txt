project(adlib-testing)

add_library(testing STATIC testing.c common.c)
target_include_directories(testing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(ENABLE_SINGLE_TESTS "enable single tests" OFF)
if(${ENABLE_SINGLE_TESTS})
  add_library(single_testing STATIC single_testing.c common.c)
  target_include_directories(single_testing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
