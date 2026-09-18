include(FetchContent)

FetchContent_Declare(
    freedv_backend
    GIT_REPOSITORY https://github.com/tmiw/freedv-backend
    # Pinned instead of floating on main: freedv-backend#55 (merged
    # 2026-09-18) added ccache-launcher forwarding to RNNoise's autotools
    # build by baking it into CC (CC="ccache <compiler>"). On macOS lint
    # CI that broke RNNoise's own ./configure with "C compiler cannot
    # create executables" -- Ebur128/RADE's nested-CMake launcher
    # forwarding in the same PR isn't implicated, but reverting the whole
    # commit is the only fix available from this repo without editing
    # freedv-backend itself. Move back to `main` (or a newer commit) once
    # the RNNoise regression is root-caused and fixed there.
    GIT_TAG 8990bd09ea9f1dbb9585e4c9e53fc89df20ca082
)

FetchContent_MakeAvailable(freedv_backend)

# rade_BINARY_DIR is set in freedv_backend but it doesn't propagate upward
# when using FetchContent. Need to reset it here so that macOS .app generation
# works properly.
set(rade_BINARY_DIR ${freedv_backend_BINARY_DIR}/rade_build)
