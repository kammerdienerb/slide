#!/usr/bin/env bash

# CFG="-g -O0"
CFG="-arch arm64 -O3"

SDL2_PREFIX=/opt/homebrew/Cellar/sdl2/2.0.20

gcc -Wall -c src/threadpool.c   -I src $(pkg-config --cflags freetype2) -I${SDL2_PREFIX}/include ${CFG} -o src/threadpool.o   &
gcc -Wall -c src/internal.c     -I src $(pkg-config --cflags freetype2) -I${SDL2_PREFIX}/include ${CFG} -o src/internal.o     &
gcc -Wall -c src/stb_image.c    -I src $(pkg-config --cflags freetype2) -I${SDL2_PREFIX}/include ${CFG} -o src/stb_image.o    &
gcc -Wall -c src/array.c        -I src $(pkg-config --cflags freetype2) -I${SDL2_PREFIX}/include ${CFG} -o src/array.o        &
gcc -Wall -c src/font.c         -I src $(pkg-config --cflags freetype2) -I${SDL2_PREFIX}/include ${CFG} -o src/font.o         &
gcc -Wall -c src/presentation.c -I src $(pkg-config --cflags freetype2) -I${SDL2_PREFIX}/include ${CFG} -o src/presentation.o &
gcc -Wall -c src/pdf.c          -I src $(pkg-config --cflags freetype2) -I${SDL2_PREFIX}/include ${CFG} -o src/pdf.o          &
gcc -Wall -c src/slide.c        -I src $(pkg-config --cflags freetype2) -I${SDL2_PREFIX}/include ${CFG} -o src/slide.o        &
wait
gcc -arch arm64 src/*.o $(pkg-config --libs freetype2) -L${SDL2_PREFIX}/lib -lSDL2 -lm -lpthread -o slide
rm -rf src/*.o
