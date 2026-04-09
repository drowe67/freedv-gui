add_library(r8brain STATIC fft/pffft_double.c fft/pffft_common.c)
target_include_directories(r8brain INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
