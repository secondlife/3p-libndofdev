#!/usr/bin/env bash

TOP="$(dirname "$0")"

# turn on verbose debugging output for parabuild logs.
exec 4>&1; export BASH_XTRACEFD=4; set -x
# make errors fatal
set -e
# complain about unset env variables
set -u

PROJECT="libndofdev"
# If there's a version number embedded in the source code somewhere, we
# haven't yet found it.
VERSION="0.1"
SOURCE_DIR="$PROJECT"

if [ -z "$AUTOBUILD" ] ; then 
    exit 1
fi

if [ "$OSTYPE" = "cygwin" ] ; then
    autobuild="$(cygpath -u $AUTOBUILD)"
else
    autobuild="$AUTOBUILD"
fi

stage="$(pwd)"

# load autobuild provided shell functions and variables
source_environment_tempfile="$stage/source_environment.sh"
"$autobuild" source_environment > "$source_environment_tempfile"
. "$source_environment_tempfile"

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
    darwin*)
        opts="-DTARGET_OS_MAC $LL_BUILD_RELEASE"
        cmake ../libndofdev -DCMAKE_CXX_FLAGS="$opts" -DCMAKE_C_FLAGS="$opts" \
            -DCMAKE_OSX_ARCHITECTURES="$AUTOBUILD_CONFIGURE_ARCH"
        make
        mkdir -p "$stage/lib/release"
        cp "src/libndofdev.dylib" "$stage/lib/release"
        pushd "$stage/lib/release/"
            fix_dylib_id libndofdev.dylib

            CONFIG_FILE="$build_secrets_checkout/code-signing-osx/config.sh"
            if [ -f "$CONFIG_FILE" ]; then
                source $CONFIG_FILE
                codesign --force --timestamp --sign "$APPLE_SIGNATURE" "libndofdev.dylib"
            else 
                echo "No config file found; skipping codesign."
            fi
        popd
    ;;
    linux*)
        # Given forking and future development work, it seems unwise to
        # hardcode the actual URL of the current project's libndofdef-linux
        # repository in this message. Try to determine the URL of this
        # libndofdev repository and prepend "open-" as a suggestion.
        echo "Linux libndofdev is in a separate open-libndofdev bitbucket repository \
-- try $(hg paths default | sed 's/libndofdev/open-&/')" 1>&2 ; exit 1
    ;;
esac

mkdir -p "$stage/include/"
cp "$TOP/$SOURCE_DIR/src/ndofdev_external.h" "$stage/include/"
mkdir -p "$stage/LICENSES"
cp -v "$TOP/$SOURCE_DIR/COPYING"  "$stage/LICENSES/$PROJECT.txt"
