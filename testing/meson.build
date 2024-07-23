testing_inc = include_directories('include')
testing = static_library('testing', 'testing.c', include_directories: testing_inc)
if get_option('single-tests')
  single_testing = static_library('single_testing', 'single_testing.c', include_directories: testing_inc)
endif
