#!/bin/bash

cd "$(dirname "$0")"

# turn on verbose debugging output for parabuild logs.
set -x
# make errors fatal
set -e

PROJECT="libndofdev"
VERSION="0.1"
SOURCE_DIR="$PROJECT/src"

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

stage="$(pwd)/stage"
pushd "$SOURCE_DIR"
    case "$AUTOBUILD_PLATFORM" in
        "windows")
			pwd
            load_vsvars
            build_sln "$PROJECT.sln" "Debug|Win32"
            build_sln "$PROJECT.sln" "Release|Win32"

            mkdir -p "$stage/lib/debug"
            mkdir -p "$stage/lib/release"
			
            cp Release/*\.lib $stage/lib/release/
            cp Debug/*\.lib $stage/lib/release/

            mkdir -p "$stage/include/$PROJECT"
            cp -v *.h "$stage/include/$PROJECT"
        ;;
        "darwin")
            ./configure --prefix="$stage"
            make
            make install
			mkdir -p "$stage/include/$PROJECT"
        ;;
        "linux")
            CFLAGS="-m32" CXXFLAGS="-m32" ./configure --prefix="$stage"
            make
            make install
			mkdir -p "$stage/include/$PROJECT"
        ;;
    esac
    mkdir -p "$stage/LICENSES"
    cp -v ../COPYING  "$stage/LICENSES/$PROJECT.txt"
popd

pass

