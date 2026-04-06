add_library(r8brain STATIC fft/pffft_double.c)
target_include_directories(r8brain INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
