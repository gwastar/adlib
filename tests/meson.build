m_dep = cc.find_library('m', required : false)

tests = [
  'array',
  'avl_tree',
  'btree_map',
  'btree_set',
  'charconv',
  'dbuf',
  'dstring',
  'hash',
  'hashmap',
  'hashset',
  'heap',
  'json',
  'random',
  'rb_tree',
  'stringview',
  'uint128',
  'utils',
]

# build a static library out of each source file so that we don't compile every file twice
targets = []
foreach name : tests
  targets += static_library(name, name + '.c', dependencies : m_dep, c_args : ['-D__ADLIB_TESTS__'], link_with : adlib, include_directories : [adlib_inc, testing_inc])
endforeach

executable('testsuite', link_whole: targets, link_with: testing)

if get_option('single-tests')
  foreach t : targets
    e = executable(t.name(), link_whole: t, link_with: single_testing)
    test(t.name(), e, timeout : 600)
  endforeach
endif
