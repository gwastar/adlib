project('adlib', 'c',
  version : '0.0.1',
  license : 'MIT',
  meson_version : '>= 0.64.0',
  default_options : [ 'warning_level=3',
                      'default_library=static',
                      'c_std=gnu11',
                      'b_pie=true',
                      'b_lto=true',
                      #'b_sanitize=address,undefined',
                      'buildtype=release' ])

cc = meson.get_compiler('c')

c_args = [
  '-Wformat=2',
  '-Wimplicit-fallthrough',
  '-Wimplicit-fallthrough=5',
  '-Wno-language-extension-token',
  '-fstrict-flex-arrays=3',

  '-fno-semantic-interposition',

  # TODO

  #'-march=native',
  #'-mtune=native',
  
  # '-fanalyzer',
  # '-Wsuggest-attribute=pure',
  # '-Wsuggest-attribute=const',
  # '-Wsuggest-attribute=noreturn',
  # '-Wmissing-noreturn',
  # '-Wsuggest-attribute=malloc',
  # '-Wsuggest-attribute=format',

  # '-funsigned-char',
]

if ['1', '2', '3', 's'].contains(get_option('optimization'))
    c_args += '-D_FORTIFY_SOURCE=3'
endif

c_link_args = [
  '-Wl,-Bsymbolic-functions'
]

add_project_arguments(cc.get_supported_arguments(c_args), language: 'c')
add_project_link_arguments(cc.get_supported_link_arguments(c_link_args), language: 'c')

cdata = configuration_data()
disable_feature_detection = get_option('disable-feature-detection')
cdata.set('__DISABLE_FEATURE_DETECTION', disable_feature_detection)

cdata.set('ARRAY_INITIAL_SIZE', get_option('array-initial-size'))
cdata.set('ARRAY_GROWTH_FACTOR_NUMERATOR', get_option('array-growth-factor-numerator'))
cdata.set('ARRAY_GROWTH_FACTOR_DENOMINATOR', get_option('array-growth-factor-denominator'))
cdata.set('DBUF_INITIAL_SIZE', get_option('dbuf-initial-size'))
cdata.set('DSTRING_INITIAL_SIZE', get_option('dstring-initial-size'))
cdata.set('DSTRING_GROWTH_FACTOR_NUMERATOR', get_option('dstring-growth-factor-numerator'))
cdata.set('DSTRING_GROWTH_FACTOR_DENOMINATOR', get_option('dstring-growth-factor-denominator'))
cdata.set('HASHTABLE_' + get_option('hashtable-implementation').to_upper(), true)

have_typeof = cc.compiles('int main() { typeof(int) x = 0; return x; }', name : 'typeof')
cdata.set('HAVE_TYPEOF', have_typeof)
cdata.set('HAVE_MALLOC_USABLE_SIZE', cc.has_function('malloc_usable_size'))

if not disable_feature_detection
  cdata.set('HAVE_BUILTIN_ADD_OVERFLOW', cc.has_function('__builtin_add_overflow'))
  cdata.set('HAVE_BUILTIN_BSWAP', cc.has_function('__builtin_bswap16') and cc.has_function('__builtin_bswap32') and cc.has_function('__builtin_bswap64'))
  cdata.set('HAVE_BUILTIN_CHOOSE_EXPR', cc.has_function('__builtin_choose_expr'))
  cdata.set('HAVE_BUILTIN_CLZ', cc.has_function('__builtin_clz'))
  cdata.set('HAVE_BUILTIN_CONSTANT_P', cc.has_function('__builtin_constant_p'))
  cdata.set('HAVE_BUILTIN_CTZ', cc.has_function('__builtin_ctz'))
  cdata.set('HAVE_BUILTIN_EXPECT', cc.has_function('__builtin_expect'))
  cdata.set('HAVE_BUILTIN_MUL_OVERFLOW', cc.has_function('__builtin_mul_overflow'))
  cdata.set('HAVE_BUILTIN_OBJECT_SIZE', cc.has_function('__builtin_object_size'))
  cdata.set('HAVE_BUILTIN_DYNAMIC_OBJECT_SIZE', cc.has_function('__builtin_dynamic_object_size'))
  cdata.set('HAVE_BUILTIN_SUB_OVERFLOW', cc.has_function('__builtin_sub_overflow'))
  cdata.set('HAVE_BUILTIN_POPCOUNT', cc.has_function('__builtin_popcount'))
  cdata.set('HAVE_BUILTIN_UNREACHABLE', cc.has_function('__builtin_unreachable'))
  
  cdata.set('HAVE_STRNLEN', cc.has_function('strnlen', args : '-D_GNU_SOURCE'))
  cdata.set('HAVE_MEMMEM', cc.has_function('memmem', args : '-D_GNU_SOURCE'))
  cdata.set('HAVE_MEMRCHR', cc.has_function('memrchr', args : '-D_GNU_SOURCE'))
endif

cdata.set('BYTE_ORDER_IS_BIG_ENDIAN', host_machine.endian() == 'big')
cdata.set('BYTE_ORDER_IS_LITTLE_ENDIAN', host_machine.endian() == 'little')


configure_file(output : 'config.h', input : 'include/config.h.in', configuration : cdata, format : 'cmake@', install : true, install_dir : 'include/adlib', install_tag : 'devel')

adlib_inc = include_directories('include', '.')

sources = [
  'array.c',
  'avl_tree.c',
  'btree.c',
  'charconv.c',
  'dbuf.c',
  'dstring.c',
  'fortify.c',
  'hash.c',
  'hashtable.c',
  'random.c',
  'rb_tree.c',
  'utils.c',
]

prefixed_sources = []
foreach source : sources
  prefixed_sources += 'src' / source
endforeach

adlib = library('adlib', prefixed_sources, include_directories : adlib_inc, install : true)

install_subdir('include', install_dir : 'include/adlib', strip_directory : true, install_tag : 'devel', exclude_files : 'config.h.in')

if get_option('tests')
  subdir('tests')
endif

if get_option('staging')
  subdir('staging')
endif
