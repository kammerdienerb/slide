#!/usr/bin/env bash

# CFG="-g -O0"
CFG="-O3"

FT_CFLAGS=$(pkg-config --cflags freetype2)
FT_LDFLAGS=$(pkg-config --libs freetype2)
SDL_CFLAGS=$(pkg-config --cflags sdl2)
SDL_LDFLAGS=$(pkg-config --libs sdl2)

CFLAGS="-Wall -I src ${FT_CFLAGS} ${SDL_CFLAGS}"
LDFLAGS="${FT_LDFLAGS} ${SDL_LDFLAGS} -lm -lpthread"

gcc -c src/threadpool.c   ${CFLAGS} ${CFG} -o src/threadpool.o   &
gcc -c src/internal.c     ${CFLAGS} ${CFG} -o src/internal.o     &
gcc -c src/stb_image.c    ${CFLAGS} ${CFG} -o src/stb_image.o    &
gcc -c src/array.c        ${CFLAGS} ${CFG} -o src/array.o        &
gcc -c src/font.c         ${CFLAGS} ${CFG} -o src/font.o         &
gcc -c src/presentation.c ${CFLAGS} ${CFG} -o src/presentation.o &
gcc -c src/pdf.c          ${CFLAGS} ${CFG} -o src/pdf.o          &
gcc -c src/slide.c        ${CFLAGS} ${CFG} -o src/slide.o        &
wait
gcc src/*.o ${LDFLAGS} ${CFG} -o slide
# rm -rf src/*.o
