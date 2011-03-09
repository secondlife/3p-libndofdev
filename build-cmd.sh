#!/bin/bash

TOP="$(dirname "$0")"

# turn on verbose debugging output for parabuild logs.
set -x
# make errors fatal
set -e

PROJECT="libndofdev"
VERSION="0.1"
SOURCE_DIR="$PROJECT"

if [ -z "$AUTOBUILD" ] ; then 
    fail
fi

if [ "$OSTYPE" = "cygwin" ] ; then
    export AUTOBUILD="$(cygpath -u $AUTOBUILD)"
fi

# load autbuild provided shell functions and variables
set +x
eval "$("$AUTOBUILD" source_environment)"
set -x

stage="$(pwd)"
case "$AUTOBUILD_PLATFORM" in
    "windows")
        pushd "$TOP/$SOURCE_DIR/src"
            load_vsvars
            build_sln "$PROJECT.sln" "Debug|Win32"
            build_sln "$PROJECT.sln" "Release|Win32"
    
            mkdir -p "$stage/lib/debug"
            mkdir -p "$stage/lib/release"
            
            cp Release/*\.lib $stage/lib/release/
            cp Debug/*\.lib $stage/lib/debug/
        popd
    ;;
    "darwin")
        make
        mkdir -p "$stage/lib/release"
        cp "src/libndofdev.dylib" "$stage/lib/release"
        pushd "$stage/lib/release/"
            fix_dylib_id libndofdev.dylib
        popd
    ;;
    "linux")
    ;;
esac

mkdir -p "$stage/include/"
cp "$TOP/$SOURCE_DIR/src/ndofdev_external.h" "$stage/include/"
mkdir -p "$stage/LICENSES"
cp -v "$TOP/$SOURCE_DIR/COPYING"  "$stage/LICENSES/$PROJECT.txt"

pass

