#!/usr/bin/env bash
#
# Builds the benchmark in Release, runs it, and regenerates docs/results.md
# plus both scaling charts from the same numbers — so the table and the
# pictures can never disagree.
#
# Requires: cmake, a C++20 compiler, python3 with matplotlib.
#
# Hardware details are detected from this machine. Override them when
# publishing numbers measured elsewhere:
#
#   ./scripts/run_benchmarks.sh --cpu "AMD Ryzen 9 7950X" --cores 16 --threads 32
#
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$root/build-release"

# /proc/cpuinfo names are too verbose for a chart title: "11th Gen Intel(R)
# Core(TM) i7-1165G7 @ 2.80GHz" becomes "Intel Core i7-1165G7".
cpu="$(sed -n 's/^model name[[:space:]]*: //p' /proc/cpuinfo | head -1 \
       | sed -e 's/(R)//g; s/(TM)//g; s/[0-9]*th Gen //; s/ @ .*//' \
             -e 's/  */ /g; s/^ *//; s/ *$//')"
cores="$(lscpu | sed -n 's/^Core(s) per socket:[[:space:]]*//p' | head -1)"
sockets="$(lscpu | sed -n 's/^Socket(s):[[:space:]]*//p' | head -1)"
cores=$(( ${cores:-1} * ${sockets:-1} ))
threads="$(nproc)"
compiler=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --cpu)      cpu="$2";      shift 2 ;;
        --cores)    cores="$2";    shift 2 ;;
        --threads)  threads="$2";  shift 2 ;;
        --compiler) compiler="$2"; shift 2 ;;
        -h|--help)  sed -n '2,14p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

# Release only: an unoptimised build reports meaningless numbers.
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$build" --target benchmark >/dev/null

# Taken from CMake's own compiler detection rather than `c++ --version`,
# whose first line names the binary ("c++") instead of the toolchain.
if [[ -z "$compiler" ]]; then
    info="$(cat "$build"/CMakeFiles/*/CMakeCXXCompiler.cmake 2>/dev/null)"
    id="$(sed -n 's/^set(CMAKE_CXX_COMPILER_ID "\(.*\)")$/\1/p' <<<"$info")"
    ver="$(sed -n 's/^set(CMAKE_CXX_COMPILER_VERSION "\(.*\)")$/\1/p' <<<"$info")"
    [[ "$id" == "GNU" ]] && id="GCC"
    compiler="${id:-unknown} ${ver%.*}"
fi

echo "running benchmark (this takes a few minutes)..." >&2
"$build/benchmark" | tee /dev/stderr | python3 "$root/scripts/make_charts.py" \
    --cpu "$cpu" --cores "$cores" --threads "$threads" \
    --compiler "$compiler" --docs "$root/docs"
