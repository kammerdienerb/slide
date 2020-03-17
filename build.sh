#!/usr/bin/env bash

#CFG=-g -O0
CFG=-O3

gcc -Wall -c src/internal.c     -I src $(pkg-config --cflags freetype2) ${CFG} -o src/internal.o     &
gcc -Wall -c src/stb_image.c    -I src $(pkg-config --cflags freetype2) ${CFG} -o src/stb_image.o    &
gcc -Wall -c src/array.c        -I src $(pkg-config --cflags freetype2) ${CFG} -o src/array.o        &
gcc -Wall -c src/font.c         -I src $(pkg-config --cflags freetype2) ${CFG} -o src/font.o         &
gcc -Wall -c src/presentation.c -I src $(pkg-config --cflags freetype2) ${CFG} -o src/presentation.o &
gcc -Wall -c src/slide.c        -I src $(pkg-config --cflags freetype2) ${CFG} -o src/slide.o        &
wait
gcc src/*.o -lfreetype -lSDL2 -lOpenGL -lm -o slide
