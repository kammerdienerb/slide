#ifndef __PRESENTATION_H__
#define __PRESENTATION_H__

#include "internal.h"
#include "array.h"
#include "tree.h"
#include "font.h"
#include "threadpool.h"

#define PRES_PARA      (1)
#define PRES_BREAK     (2)
#define PRES_VSPACE    (3)
#define PRES_VFILL     (4)
#define PRES_POINT     (5)
#define PRES_BULLET    (6)
#define PRES_IMAGE     (7)
#define PRES_TRANSLATE (8)

#define MAX_BULLET_LEVEL (3)

#define JUST_L (1)
#define JUST_C (2)
#define JUST_R (3)

typedef struct {
    int      kind;
    int      x, y, w, h;
    int      level;
    array_t  text;
    u32      font_id;
    u32      font_size;
    u32      r, g, b;
    u32      l_margin, r_margin;
    int      justification;
    char    *image;
} pres_elem_t;

typedef char *macro_name_t;
use_tree(macro_name_t, array_t);
typedef tree(macro_name_t, array_t)    macro_map_t;
typedef tree_it(macro_name_t, array_t) macro_map_it;

typedef char *image_path_t;
typedef SDL_Texture *sdl_texture_t;
use_tree(image_path_t, sdl_texture_t);
typedef tree(image_path_t, sdl_texture_t)    image_map_t;
typedef tree_it(image_path_t, sdl_texture_t) image_map_it;

typedef struct {
    tp_t            *tp;
    pthread_mutex_t  images_mutex;

    SDL_Renderer *sdl_ren;
    array_t       elements;
    array_t       fonts;
    macro_map_t   macros;
    array_t      *collect_lines;
    u32           r, g, b;
    float         speed;
    char         *bullet_strings[MAX_BULLET_LEVEL];
    image_map_t   images;

    u32           w, h;
    font_cache_t *cur_font;
    int           draw_x,   draw_y;
    int           view_x,   view_y;
    u32           n_points, point;
    int           save_points[2048];
    int           dst_view_y;
    int           is_animating;
    int           is_translating;
} pres_t;

pres_t build_presentation(const char *path, SDL_Renderer *sdl_ren);
void free_presentation(pres_t *pres);
char * pres_get_font_name_by_id(pres_t *pres, u32 id);
sdl_texture_t pres_get_image_texture(pres_t *pres, const char *image);

void draw_presentation(pres_t *pres);
void pres_next_point(pres_t *pres);
void pres_prev_point(pres_t *pres);
void pres_first_point(pres_t *pres);
void pres_last_point(pres_t *pres);

#endif
