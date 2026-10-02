include(FetchContent)

FetchContent_Declare(
    freedv_backend
    # TEMPORARY: points at the matching freedv-backend PR branch for review
    # and testing. Revert to tmiw/freedv-backend main once that PR merges.
    GIT_REPOSITORY https://github.com/barjac/freedv-backend
    GIT_TAG bcj-leveler-limiter
)

FetchContent_MakeAvailable(freedv_backend)

# rade_BINARY_DIR is set in freedv_backend but it doesn't propagate upward
# when using FetchContent. Need to reset it here so that macOS .app generation
# works properly.
set(rade_BINARY_DIR ${freedv_backend_BINARY_DIR}/rade_build)
