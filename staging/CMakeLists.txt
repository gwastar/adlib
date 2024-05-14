project(adlib-staging)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function -Wno-unused-variable")

set(EXECUTABLES
  bitmap
  cbuf
  error
  heap
  lfqueue
  list
  locktest
  malloc
  mbuf
  mem_arena
  mprintf
  sort
)

foreach(EXECUTABLE IN LISTS EXECUTABLES)
  add_executable(${EXECUTABLE} ${EXECUTABLE}.c)
  target_link_libraries(${EXECUTABLE} ad-static)
endforeach()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(locktest Threads::Threads)
