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
  'json.c',
  'random.c',
  'rb_tree.c',
  'uint128.c',
  'utils.c',
]

targets = [
  {'name': 'array_benchmark', 'sources': 'array_benchmark.c',},
  {'name': 'btree_map_benchmark', 'sources': 'btree_map_benchmark.c',},
  {'name': 'btree_set_benchmark', 'sources': 'btree_set_benchmark.c',},
  {'name': 'hash_benchmark', 'sources': 'hash_benchmark.c',},
  {'name': 'hash_comparison', 'sources': 'hash_comparison.c', 'deps': hash_comparison_deps, 'c_args': hash_comparison_c_args},
  {'name': 'hashtable_benchmark', 'sources': 'hashtable_benchmark.c',},
  {'name': 'random_benchmark', 'sources': 'random_benchmark.c',},
  {'name': 'tests', 'sources': tests_sources + 'testing.c', 'deps': m_dep, 'c_args': ['-D__ADLIB_TESTS__']},
]

foreach target : targets
  if 'deps' in target
    deps = target['deps']
  else
    deps = []
  endif
  if 'c_args' in target
    c_args = target['c_args']
  else
    c_args = []
  endif
  name = target['name']
  e = executable(name, target['sources'], dependencies : deps, c_args : c_args, link_with : adlib, include_directories : adlib_inc)
  set_variable(name, e)
endforeach

test('testsuite', tests, timeout : 600)
benchmark('array_benchmark', array_benchmark)
benchmark('hash_benchmark', hash_benchmark)
benchmark('hash_comparison', hash_comparison)
benchmark('hashtable_benchmark', hashtable_benchmark)
benchmark('random_benchmark', random_benchmark)