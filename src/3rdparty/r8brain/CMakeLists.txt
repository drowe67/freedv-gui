add_library(r8brain STATIC fft/pffft.c fft/pffft_common.c)
target_include_directories(r8brain INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(r8brain PRIVATE -DPFFFT_STATIC_DEFINE)
