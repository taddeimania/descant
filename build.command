#!/bin/bash
# Builds Descant and installs it into ~/Library/Audio/Plug-Ins/VST3.
# Double-click in Finder, or run from a terminal. Output also goes to build/build.log.
cd "$(dirname "$0")"
mkdir -p build
{
  echo "=== configure $(date)"
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  echo "=== build $(date)"
  cmake --build build -j8
  echo "=== tests $(date)"
  ctest --test-dir build --output-on-failure
  echo "=== done $(date)"
} 2>&1 | tee build/build.log
echo "BUILD FINISHED - you can close this window"
