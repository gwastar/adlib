executables = [
  'bitmap',
  'cbuf',
  'error',
  'lfqueue',
  'list',
  'locktest',
  'malloc',
  'mbuf',
  'mem_arena',
  'mprintf',
  'sort',
]

staging_c_args = [
  '-Wno-unused-function',
  '-Wno-unused-variable',
]

foreach exe : executables
  executable(exe, exe + '.c', link_with : adlib, include_directories : adlib_inc, c_args : staging_c_args)
endforeach