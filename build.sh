#!/usr/bin/env bash

gcc -c src/internal.c -I src $(pkg-config --cflags freetype2) -g -o src/internal.o   &
gcc -c src/array.c -I src $(pkg-config --cflags freetype2) -g -o src/array.o   &
gcc -c src/font.c -I src $(pkg-config --cflags freetype2) -g -o src/font.o   &
gcc -c src/parse.c -I src $(pkg-config --cflags freetype2) -g -o src/parse.o   &
gcc -c src/presentation.c -I src $(pkg-config --cflags freetype2) -g -o src/presentation.o   &
gcc -c src/slide.c -I src $(pkg-config --cflags freetype2) -g -o src/slide.o &
wait
gcc src/*.o -lfreetype -lSDL2 -lOpenGL -o slide
