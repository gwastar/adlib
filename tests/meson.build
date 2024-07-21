m_dep = cc.find_library('m', required : false)

hash_comparison_dependencies = [
  ['xxhash', 'libxxhash', '>=0.8'],
  ['gcrypt', 'libgcrypt', '>=1.10'],
  ['gnutls', 'gnutls', '>=3.8'],
]

hash_comparison_c_args = []
hash_comparison_deps = []
foreach dep : hash_comparison_dependencies
  d = dependency(dep[1], version : dep[2], required : false)
  if d.found()
     hash_comparison_deps += d
     hash_comparison_c_args += '-DHAVE_' + dep[0].to_upper() + '=1'
  endif
endforeach

tests_sources = [
  'array.c',
  'avl_tree.c',
  'btree_map.c',
  'btree_set.c',
  'charconv.c',
  'dbuf.c',
  'dstring.c',
  'hash.c',
  'hashmap.c',
  'hashset.c',
  'heap.c',
  'json.c',
  'random.c',
  'rb_tree.c',
  'selftests.c',
  'uint128.c',
  'utils.c',
]

targets = [
  {'name': 'array_benchmark', 'sources': 'array_benchmark.c',},
  {'name': 'btree_map_benchmark', 'sources': 'btree_map_benchmark.c',},
  {'name': 'btree_set_benchmark', 'sources': 'btree_set_benchmark.c',},
  {'name': 'charconv_benchmark', 'sources': 'charconv_benchmark.c',},
  {'name': 'hash_benchmark', 'sources': 'hash_benchmark.c',},
  {'name': 'hash_comparison', 'sources': 'hash_comparison.c', 'deps': hash_comparison_deps, 'c_args': hash_comparison_c_args},
  {'name': 'hashtable_benchmark', 'sources': 'hashtable_benchmark.c',},
  {'name': 'heap_benchmark', 'sources': 'heap_benchmark.c',},
  {'name': 'random_benchmark', 'sources': 'random_benchmark.c',},
  {'name': 'tests', 'sources': tests_sources + ['testing.c', 'testing_common.c'], 'deps': m_dep, 'c_args': ['-D__ADLIB_TESTS__']},
]

foreach target : targets
  deps = target.get('deps', [])
  c_args = target.get('c_args', [])
  name = target['name']
  e = executable(name, target['sources'], dependencies : deps, c_args : c_args, link_with : adlib, include_directories : adlib_inc)
  set_variable(name, e)
endforeach

if get_option('single-tests')
  foreach src : tests_sources
    name = src.substring(0, -2)
    e = executable(name, [src, 'single_testing.c', 'testing_common.c'], dependencies : m_dep, c_args : ['-D__ADLIB_TESTS__'], link_with : adlib, include_directories : adlib_inc)
    test(name, e, timeout : 600)
  endforeach
endif
