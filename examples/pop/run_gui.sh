#!/usr/bin/env bash
# Build and launch the windowed Prince of Persia GUI (pop_gui), macOS only.
#
# Forces Apple clang instead of $CC/$CXX: on machines where those point at
# Homebrew GCC (common with a conda/Homebrew toolchain on PATH), the full
# profile fails to compile the engine's Objective-C++ (.mm) Metal/Cocoa
# bridging code with errors like "stray '@' in program" -- GCC doesn't
# understand Objective-C++ at all. See POP.md "Windowed (macOS) version".
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${repo_root}/build_full"

if ! command -v brew >/dev/null 2>&1 || ! brew list glfw >/dev/null 2>&1; then
    echo "glfw not found -- installing via Homebrew..."
    brew install glfw
fi

if ! xcrun --find metal >/dev/null 2>&1 || ! xcrun metal --version >/dev/null 2>&1; then
    echo "Metal Toolchain missing -- downloading (this is a one-time ~700MB fetch)..."
    xcodebuild -runFirstLaunch
    xcodebuild -downloadComponent MetalToolchain
fi

cmake -S "$repo_root" -B "$build_dir" \
    -DLOGOSPHERE_PROFILE=full \
    -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++

cmake --build "$build_dir" --target pop_gui -j "$(sysctl -n hw.ncpu)"

exec "${build_dir}/pop/pop_gui"
