include(FetchContent)

FetchContent_Declare(
    freedv_backend
    GIT_REPOSITORY https://github.com/tmiw/freedv-backend
    # TODO: revert to radev2-dev once tmiw/freedv-backend#48 (real-time-safe
    # async ulog front end) is merged. Pinned to a SHA so CI picks up the
    # exact branch state regardless of FetchContent caching.
    GIT_TAG 61ed161bc34515e867b24bb7692ed0d426052ab6
)

FetchContent_MakeAvailable(freedv_backend)

# rade_BINARY_DIR is set in freedv_backend but it doesn't propagate upward
# when using FetchContent. Need to reset it here so that macOS .app generation
# works properly.
set(rade_BINARY_DIR ${freedv_backend_BINARY_DIR}/rade_build)
