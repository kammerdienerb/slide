#ifndef __INTERNAL_H__
#define __INTERNAL_H__

#include "stb_image.h"

#include <SDL2/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <assert.h>
#include <sys/signal.h>

#include <locale.h>
#define __USE_XOPEN
#define _XOPEN_SOURCE
#include <wchar.h>

typedef uint32_t u32;
typedef uint64_t u64;

#define likely(x)   (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))

u64 next_power_of_2(u64 x);

#define TIME_ON(label)                                                 \
do {                                                                   \
    u32 _##label##_time;                                               \
    _##label##_time = SDL_GetTicks();

#define TIME_OFF(label)                                              \
    printf("[%s] %ums\n", #label, SDL_GetTicks() - _##label##_time); \
} while (0)

#define ERR(...) do {                           \
fprintf(stderr, "[slide] ERROR: " __VA_ARGS__); \
    exit(1);                                    \
} while (0)

#define SCREEN_WIDTH  (2880) // ( 2  * 640)
#define SCREEN_HEIGHT (1800) // ( 2  * 480)

#endif
