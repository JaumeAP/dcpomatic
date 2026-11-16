#!/bin/bash
# Builds build/bcast_to_loopback.dylib next to this script (macOS, deployment target 11.0, ad hoc signed) when it is
# missing or older than its source, and prints its path.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=$here/build/bcast_to_loopback.dylib
mkdir -p "$here/build"
if [ ! -f "$out" ] || [ "$here/bcast_to_loopback.c" -nt "$out" ]; then
	/usr/bin/clang -dynamiclib -O2 -Wall -mmacosx-version-min=11.0 -o "$out" "$here/bcast_to_loopback.c"
	codesign -f -s - "$out" 2>/dev/null
fi
echo "$out"
