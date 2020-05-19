#ifndef __INTERNAL_H__
#define __INTERNAL_H__

#include "stb_image.h"

#include <SDL2/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <libgen.h>

#include <locale.h>
#define __USE_XOPEN
#define _XOPEN_SOURCE
#include <wchar.h>

typedef int32_t  i32;
typedef uint32_t u32;
typedef uint64_t u64;

#define likely(x)   (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))

u64 next_power_of_2(u64 x);
u64 gettime_ns(void);

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

#define FPS_TO_MS(fps)   ((1000) * (1.0 / (float)(fps)))
#define FPS_CAP_MS       ((u64)(FPS_TO_MS(60)))

#define DEFAULT_RES_W          (1440)
#define DEFAULT_RES_H          (1080)
#define DISPLAY_DELAY_FRAMES   (1)
#define NON_ANIM_DRAW_INTERVAL (8)

#endif
