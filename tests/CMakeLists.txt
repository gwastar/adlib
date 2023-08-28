project(adlib-tests)

function(target_add_adlib TARGET)
  if(${TESTS_USE_SINGLE_HEADERS})
    target_include_directories(${TARGET} PRIVATE ${SINGLE_HEADERS_DIR})
    add_dependencies(${TARGET} single_header_library)
  else()
    target_link_libraries(${TARGET} ad-static)
  endif()
endfunction()

macro(add_standalone NAME)
  add_executable(${NAME} ${NAME}.c ${ARGN})
  target_add_adlib(${NAME})
endmacro()

add_standalone(array_benchmark)
add_standalone(hash_benchmark)
add_standalone(hash_comparison)
add_standalone(hashtable_benchmark)
add_standalone(random_benchmark)

include(FindPkgConfig)
if(${PKG_CONFIG_FOUND})
  pkg_check_modules(XXHASH libxxhash>=0.8.0)
  if(${XXHASH_FOUND})
    target_include_directories(hash_comparison PRIVATE ${XXHASH_INCLUDE_DIRS})
    target_link_libraries(hash_comparison ${XXHASH_LIBRARIES})
    target_compile_definitions(hash_comparison PRIVATE HAVE_XXHASH)
  endif()
  pkg_check_modules(GCRYPT libgcrypt>=1.10)
  if(${GCRYPT_FOUND})
    target_include_directories(hash_comparison PRIVATE ${GCRYPT_INCLUDE_DIRS})
    target_link_libraries(hash_comparison ${GCRYPT_LIBRARIES})
    target_compile_definitions(hash_comparison PRIVATE HAVE_GCRYPT)
  endif()
  pkg_check_modules(GNUTLS gnutls>=3.8)
  if(${GNUTLS_FOUND})
    target_include_directories(hash_comparison PRIVATE ${GNUTLS_INCLUDE_DIRS})
    target_link_libraries(hash_comparison ${GNUTLS_LIBRARIES})
    target_compile_definitions(hash_comparison PRIVATE HAVE_GNUTLS)
  endif()
endif()

set(TEST_SOURCES
  array.c
  avl_tree.c
  charconv.c
  dbuf.c
  dstring.c
  hash.c
  hashmap.c
  hashset.c
  json.c
  random.c
  rb_tree.c
  utils.c
)

add_executable(tests testing.c ${TEST_SOURCES})
if(${TESTS_USE_SINGLE_HEADERS})
  target_include_directories(tests PRIVATE ${SINGLE_HEADERS_DIR})
  add_dependencies(tests single_header_library)
else()
  target_link_libraries(tests ad-static)
endif()

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

target_link_libraries(tests Threads::Threads m)
