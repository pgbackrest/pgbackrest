#!/bin/sh
####################################################################################################################################
# Build the fuzz targets in test/fuzz into $OUT, each with a seed corpus from test/fuzz/corpus when one exists. The environment is
# the one OSS-Fuzz defines: CC and CFLAGS instrument the build for the sanitizers and the fuzzing engine, LIB_FUZZING_ENGINE is
# linked into each target, and WORK is where the build goes. Debug enables asserts, which abort the target when they fail.
####################################################################################################################################
set -eu

repo=$(cd "$(dirname "$0")/../.." && pwd)

meson setup "$WORK/build" "$repo" --buildtype=plain -Ddebug=true -Db_ndebug=false -Dfuzz-engine="$LIB_FUZZING_ENGINE"
meson compile -C "$WORK/build"

for fuzz in "$WORK"/build/test/fuzz-*; do
    # Skip the object directories
    [ -f "$fuzz" ] || continue

    name=$(basename "$fuzz")
    corpus="$repo/test/fuzz/corpus/${name#fuzz-}"

    cp "$fuzz" "$OUT/"

    if [ -d "$corpus" ]; then
        zip -jq "$OUT/${name}_seed_corpus.zip" "$corpus"/*
    fi
done
