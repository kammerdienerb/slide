#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd $DIR

# CFG="-g -O0"
CFG="-O3"

FT_CFLAGS=$(pkg-config --cflags freetype2)
FT_LDFLAGS=$(pkg-config --libs freetype2)
SDL_CFLAGS=$(pkg-config --cflags sdl2)
SDL_LDFLAGS=$(pkg-config --libs sdl2)
HPDF_CFLAGS="-Ilibharu/build/include -Ilibharu/include"
HPDF_LDFLAGS="-Llibharu/build/src -lhpdf -lz -lpng"

CFLAGS="-Wall -I src ${FT_CFLAGS} ${SDL_CFLAGS} ${HPDF_CFLAGS}"
LDFLAGS="${FT_LDFLAGS} ${SDL_LDFLAGS} ${HPDF_LDFLAGS} -lm -lpthread"

pids=""

function add_bg {
    $@ &
    pids+=" $!"
}

function wait_all {
    for p in $pids; do
        if ! wait $p; then
            return 1
        fi
    done

    return 0
}


cd libharu
rm -rf build
mkdir build
cd build
cmake .. -DBUILD_SHARED_LIBS=OFF || exit $?
make -j || exit $?

cd $DIR
add_bg gcc -c src/threadpool.c   ${CFLAGS} ${CFG} -o src/threadpool.o
add_bg gcc -c src/internal.c     ${CFLAGS} ${CFG} -o src/internal.o
add_bg gcc -c src/stb_image.c    ${CFLAGS} ${CFG} -o src/stb_image.o
add_bg gcc -c src/array.c        ${CFLAGS} ${CFG} -o src/array.o
add_bg gcc -c src/font.c         ${CFLAGS} ${CFG} -o src/font.o
add_bg gcc -c src/presentation.c ${CFLAGS} ${CFG} -o src/presentation.o
add_bg gcc -c src/pdf.c          ${CFLAGS} ${CFG} -o src/pdf.o
add_bg gcc -c src/slide.c        ${CFLAGS} ${CFG} -o src/slide.o

wait_all || exit $?

gcc src/*.o ${LDFLAGS} ${CFG} -o slide || echo $?
echo "Built slide."
