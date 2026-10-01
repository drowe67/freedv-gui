include(FetchContent)

# CI DIAGNOSTIC (ms-ci-rade-loss-diag only): FREEDV_BACKEND_PATCH, if set in the
# environment, is a patch applied to freedv-backend after it's fetched (for A/B runs).
set(FREEDV_BACKEND_PATCH_CMD)
if(NOT "$ENV{FREEDV_BACKEND_PATCH}" STREQUAL "")
    message(STATUS "Applying freedv-backend patch $ENV{FREEDV_BACKEND_PATCH}")
    set(FREEDV_BACKEND_PATCH_CMD PATCH_COMMAND git apply --verbose $ENV{FREEDV_BACKEND_PATCH})
endif()

FetchContent_Declare(
    freedv_backend
    GIT_REPOSITORY https://github.com/tmiw/freedv-backend
    GIT_TAG main
    ${FREEDV_BACKEND_PATCH_CMD}
)

FetchContent_MakeAvailable(freedv_backend)

# rade_BINARY_DIR is set in freedv_backend but it doesn't propagate upward
# when using FetchContent. Need to reset it here so that macOS .app generation
# works properly.
set(rade_BINARY_DIR ${freedv_backend_BINARY_DIR}/rade_build)
