executables = [
  'bitmap',
  'btree',
  'cbuf',
  'endian',
  'error',
  'heap',
  'lfqueue',
  'list',
  'locktest',
  'malloc',
  'mbuf',
  'mem_arena',
  'mprintf',
  'sort',
]

c_args = [
  '-Wno-unused-function',
  '-Wno-unused-variable',
]

foreach exe : executables
  executable(exe, exe + '.c', link_with : adlib, include_directories : adlib_inc, c_args : c_args)
endforeach