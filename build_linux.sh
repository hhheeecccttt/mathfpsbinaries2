#!/usr/bin/env bash
# Linux build (the original project is an MSVC solution; this builds the same
# sources with g++ and outputs a runnable game folder).
#
# Dependencies:
#   - glfw3, freetype2, zlib  (system packages, found via pkg-config)
#   - assimp 5.4 built as a static library (not packaged here); set ASSIMP_DIR
#     to the assimp *build* directory (the one containing lib/libassimp.a and
#     include/assimp/revision.h). Build it with:
#
#       git clone --depth 1 --branch v5.4.3 https://github.com/assimp/assimp
#       cmake -S assimp -B assimp/build -DCMAKE_BUILD_TYPE=Release \
#             -DASSIMP_BUILD_TESTS=OFF -DASSIMP_BUILD_ASSIMP_TOOLS=OFF \
#             -DASSIMP_BUILD_ZLIB=ON -DASSIMP_WARNINGS_AS_ERRORS=OFF \
#             -DBUILD_SHARED_LIBS=OFF
#       cmake --build assimp/build -j
#
# Notes:
#   - FMOD is stubbed (fmod_stub.cpp): the game runs identically, just silent.
#   - Net.cpp uses POSIX sockets on non-Windows builds.
set -euo pipefail

cd "$(dirname "$0")"
ASSIMP_DIR="${ASSIMP_DIR:-/tmp/opencode/deps/assimp/build}"
OUT="${OUT:-../mathfpsbinaries2}"
BUILD=/tmp/opencode/build
mkdir -p "$BUILD"

if [ ! -f "$ASSIMP_DIR/lib/libassimp.a" ]; then
    echo "assimp static lib not found in $ASSIMP_DIR/lib (see header of this script)" >&2
    exit 1
fi

# glad.c is C, everything else C++
gcc -O2 -c glad.c -I dependencies/includes -o "$BUILD/glad.o"

rm -f "$BUILD"/{Character,EBO,Engine,main,Mouse,Net,Physics,RenderSystem,shaderClass,Sound,SoundSystem,texture,tFunctions,VAO,VBO,fmod_stub}.o
g++ -O2 -DNDEBUG -std=c++17 -Wno-changes-meaning -c \
    Character.cpp EBO.cpp Engine.cpp main.cpp Mouse.cpp Net.cpp Physics.cpp \
    RenderSystem.cpp shaderClass.cpp Sound.cpp SoundSystem.cpp texture.cpp \
    tFunctions.cpp VAO.cpp VBO.cpp fmod_stub.cpp \
    -I dependencies/includes \
    -I "$ASSIMP_DIR/include" \
    -I /usr/include/freetype2 \
    -I FMOD/api/core/inc
mv ./*.o "$BUILD"/

g++ "$BUILD"/*.o -o "$BUILD/dogtags" \
    -L"$ASSIMP_DIR/lib" -lassimp \
    -lglfw -lfreetype -lGL -lz -lpthread -ldl

# Runtime folder: binary + assets + shaders (assets/*.png/.obj/... are read from
# the working directory, so run the game from the output folder).
mkdir -p "$OUT/assets" "$OUT/shaders"
cp -r assets/* "$OUT/assets/"
cp -r shaders/* "$OUT/shaders/"
cp "$BUILD/dogtags" "$OUT/"
chmod +x "$OUT/dogtags"
echo "built $OUT/dogtags"
