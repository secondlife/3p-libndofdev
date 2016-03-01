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

build=${AUTOBUILD_BUILD_ID:=0}
echo "${VERSION}.${build}" > "${stage}/VERSION.txt"

case "$AUTOBUILD_PLATFORM" in
    windows*)
        pushd "$TOP/$SOURCE_DIR/src"
            load_vsvars
            build_sln "$PROJECT.sln" "Release|$AUTOBUILD_WIN_VSPLATFORM"
    
            mkdir -p "$stage/lib/release"

            if [ "$AUTOBUILD_ADDRSIZE" = 32 ]
            then cp Release/*\.lib $stage/lib/release/
            else cp x64/Release/*\.lib $stage/lib/release/
            fi
        popd
    ;;
    "darwin")
        # arch, min SDK, deploy SDK etc. set in CMake file
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

