cmake_minimum_required(VERSION 3.20)
project(adlib)

include(CheckCSourceCompiles)
include(CheckIPOSupported)
include(CheckSymbolExists)
include(GNUInstallDirs)

set(CMAKE_DEFAULT_BUILD_TYPE Release)

check_ipo_supported(RESULT lto_supported)
if(${lto_supported})
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
     set(CMAKE_C_COMPILE_OPTIONS_IPO "-flto=auto")
  elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
     #set(CMAKE_C_COMPILE_OPTIONS_IPO "-flto=full")
  endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wformat=2 -Wimplicit-fallthrough")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fanalyzer")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wmissing-noreturn -Wsuggest-attribute=malloc -Wsuggest-attribute=format")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
set(CMAKE_C_FLAGS_RELEASE "-O2 -D_FORTIFY_SOURCE=3")
# set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -mtune=native -D_FORTIFY_SOURCE=3")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} -g")

option(ENABLE_ASAN "enable address sanitizer" OFF)
if(${ENABLE_ASAN})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
endif()
option(ENABLE_UBSAN "enable undefined behavior sanitizer" OFF)
if(${ENABLE_UBSAN})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
endif()
option(ENABLE_MSAN "enable memory sanitizer" OFF)
if(${ENABLE_MSAN})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory")
endif()
option(ENABLE_TSAN "enable thread sanitizer" OFF)
if(${ENABLE_TSAN})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
endif()

option(DISABLE_FEATURE_DETECTION "disable detection for compiler builtins and non-standard library functions to test the fallback codepaths" OFF)

check_c_source_compiles("int main() { typeof(int) x = 0; return x; }" HAVE_TYPEOF)
check_symbol_exists(malloc_usable_size "malloc.h" HAVE_MALLOC_USABLE_SIZE)
if(${DISABLE_FEATURE_DETECTION})
  set(__DISABLE_FEATURE_DETECTION ON)
else()
  check_c_source_compiles("int main() { int i; __builtin_add_overflow(0, 0, &i); return 0; }" HAVE_BUILTIN_ADD_OVERFLOW)
  check_c_source_compiles("int main() { __builtin_bswap16(1); __builtin_bswap32(1); __builtin_bswap64(1); return 0; }" HAVE_BUILTIN_BSWAP)
  check_c_source_compiles("int main() { if (__builtin_expect(0, 0)); return 0; }" HAVE_BUILTIN_EXPECT)
  check_c_source_compiles("int main() { __builtin_clz(1); return 0; }" HAVE_BUILTIN_CLZ)
  check_c_source_compiles("int main() { __builtin_ctz(1); return 0; }" HAVE_BUILTIN_CTZ)
  check_c_source_compiles("int main() { int i; __builtin_mul_overflow(0, 0, &i); return 0; }" HAVE_BUILTIN_MUL_OVERFLOW)
  check_c_source_compiles("int main() { char buf[1]; __builtin_object_size(buf, 0); return 0; }" HAVE_BUILTIN_OBJECT_SIZE)
  check_c_source_compiles("int main() { char buf[1]; __builtin_dynamic_object_size(buf, 0); return 0; }" HAVE_BUILTIN_DYNAMIC_OBJECT_SIZE)
  check_c_source_compiles("int main() { __builtin_choose_expr(1, 2, 3); return 0; }" HAVE_BUILTIN_CHOOSE_EXPR)
  check_c_source_compiles("int main() { __builtin_constant_p(1); return 0; }" HAVE_BUILTIN_CONSTANT_P)
  check_c_source_compiles("int main() { int i; __builtin_sub_overflow(0, 0, &i); return 0; }" HAVE_BUILTIN_SUB_OVERFLOW)
  check_c_source_compiles("int main() { __builtin_popcount(1); return 0; }" HAVE_BUILTIN_POPCOUNT)
  check_c_source_compiles("int main() { if (0) __builtin_unreachable(); return 0; }" HAVE_BUILTIN_UNREACHABLE)
  check_symbol_exists(strnlen "string.h" HAVE_STRNLEN)
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  check_symbol_exists(memmem "string.h" HAVE_MEMMEM)
  check_symbol_exists(memrchr "string.h" HAVE_MEMRCHR)
  list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
endif()

if (${CMAKE_C_BYTE_ORDER} STREQUAL LITTLE_ENDIAN)
  set(BYTE_ORDER_IS_LITTLE_ENDIAN ON)
elseif(${CMAKE_C_BYTE_ORDER} STREQUAL BIG_ENDIAN)
  set(BYTE_ORDER_IS_BIG_ENDIAN ON)
endif()

set(ARRAY_INITIAL_SIZE 8 CACHE STRING "Initial size of an array")
# ARRAY_GROWTH_FACTOR=1.6 (~ golden ratio)
set(ARRAY_GROWTH_FACTOR_NUMERATOR 8 CACHE STRING "Numerator of the array growth factor")
set(ARRAY_GROWTH_FACTOR_DENOMINATOR 5 CACHE STRING "Denominator of the array growth factor")

set(DBUF_INITIAL_SIZE 16 CACHE STRING "Initial size of a dynamic byte buffer (dbuf)")

set(DSTRING_INITIAL_SIZE 8 CACHE STRING "Initial size of a dstring")
# DSTRING_GROWTH_FACTOR=1.6 (~ golden ratio)
set(DSTRING_GROWTH_FACTOR_NUMERATOR 8 CACHE STRING "Numerator of the dstring growth factor")
set(DSTRING_GROWTH_FACTOR_DENOMINATOR 5 CACHE STRING "Denominator of the dstring growth factor")

set(HASHTABLE_IMPLEMENTATION "QUADRATIC" CACHE STRING "Hashtable implementation (QUADRATIC/HOPSCOTCH/ROBINHOOD)")
if(HASHTABLE_IMPLEMENTATION STREQUAL "QUADRATIC")
  set(HASHTABLE_QUADRATIC ON BOOL "")
elseif(HASHTABLE_IMPLEMENTATION STREQUAL "HOPSCOTCH")
  set(HASHTABLE_HOPSCOTCH ON BOOL "")
elseif(HASHTABLE_IMPLEMENTATION STREQUAL "ROBINHOOD")
  set(HASHTABLE_ROBINHOOD ON BOOL "")
else()
  message(FATAL_ERROR "Invalid hashtable implementation.")
endif()

configure_file(include/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/config.h)

set(SOURCES
  array.c
  avl_tree.c
  btree.c
  charconv.c
  dbuf.c
  dstring.c
  fortify.c
  hash.c
  hashtable.c
  random.c
  rb_tree.c
  utils.c
)

foreach(SOURCE IN LISTS SOURCES)
  get_filename_component(BASENAME ${SOURCE} NAME_WLE)
  string(TOUPPER ${BASENAME} OPTION_NAME)
  string(PREPEND OPTION_NAME DISABLE_)
  option(${OPTION_NAME} "disable ${SOURCE}" OFF)
  if(${OPTION_NAME})
    list(REMOVE_ITEM SOURCES ${SOURCE})
  endif()
endforeach()

list(LENGTH SOURCES NUM_SOURCES)
if(${NUM_SOURCES} EQUAL 0)
  message(FATAL_ERROR "You have disabled all sources.")
endif()

list(TRANSFORM SOURCES PREPEND src/)

set(SOURCE_INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(GENERATED_INCLUDE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include)

add_library(ad-static STATIC ${SOURCES})
target_include_directories(ad-static PUBLIC
  $<BUILD_INTERFACE:${SOURCE_INCLUDE_DIRECTORY}>
  $<BUILD_INTERFACE:${GENERATED_INCLUDE_DIRECTORY}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(ad-static PROPERTIES OUTPUT_NAME adlib)
option(BUILD_SHARED_LIBRARY "build shared library" OFF)
if(${BUILD_SHARED_LIBRARY})
  add_library(ad-shared SHARED ${SOURCES})
  target_include_directories(ad-shared PUBLIC
    $<BUILD_INTERFACE:${SOURCE_INCLUDE_DIRECTORY}>
    $<BUILD_INTERFACE:${GENERATED_INCLUDE_DIRECTORY}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
  set_target_properties(ad-shared PROPERTIES OUTPUT_NAME adlib)
endif()

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
  install(DIRECTORY ${SOURCE_INCLUDE_DIRECTORY}/
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.h")
  install(DIRECTORY ${GENERATED_INCLUDE_DIRECTORY}/
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.h")
  install(TARGETS ad-static ARCHIVE)
  if(${BUILD_SHARED_LIBRARY})
    install(TARGETS ad-shared LIBRARY)
  endif()
endif()

option(ENABLE_TESTS "enable tests" ON)
if(${ENABLE_TESTS})
  enable_testing()
  add_subdirectory(tests)
endif()

option(ENABLE_STAGING "compile code in staging" OFF)
if(${ENABLE_STAGING})
  add_subdirectory(staging)
endif()
