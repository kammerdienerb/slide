#ifndef __PRESENTATION_H__
#define __PRESENTATION_H__

#include "internal.h"
#include "array.h"
#include "tree.h"
#include "font.h"
#include "threadpool.h"

enum {
    PRES_NULL,
    PRES_PARA,
    PRES_PARA_ELEM,
    PRES_BREAK,
    PRES_VSPACE,
    PRES_VFILL,
    PRES_POINT,
    PRES_SAVE,
    PRES_RESTORE,
    PRES_COUNTER,
    PRES_BULLET,
    PRES_IMAGE,
    PRES_GOTO,
    PRES_GOTOX,
    PRES_GOTOY,
    PRES_TRANSLATE,
};

#define MAX_BULLET_LEVEL (3)

#define JUST_L (1)
#define JUST_C (2)
#define JUST_R (3)

#define PRES_BOLD      (1ULL << 0)
#define PRES_ITALIC    (1ULL << 1)
#define PRES_UNDERLINE (1ULL << 2)

typedef struct {
    int      kind;
    int      x, y, w, h;
    int      level;
    array_t  text;
    array_t  para_elems;
    i32      font_id,
             font_bold_id,
             font_italic_id,
             font_bold_italic_id;
    u32      font_size;
    u32      r, g, b;
    u32      l_margin, r_margin;
    int      justification;
    union {
    char    *image;
    char    *mark_name;
    };
    u32      flags;

    array_t  all_text;
    array_t  wrap_points;
    array_t  line_widths;
} pres_elem_t;

typedef char *macro_name_t;
use_tree(macro_name_t, array_t);
typedef tree(macro_name_t, array_t)    macro_map_t;
typedef tree_it(macro_name_t, array_t) macro_map_it;

typedef SDL_Texture *sdl_texture_t;

typedef struct {
    void          *image_data;
    sdl_texture_t  texture;
    int            w, h;
} pres_image_data_t;

typedef char *image_path_t;
use_tree(image_path_t, pres_image_data_t);
typedef tree(image_path_t, pres_image_data_t)    image_map_t;
typedef tree_it(image_path_t, pres_image_data_t) image_map_it;

typedef char *mark_name_t;
use_tree(mark_name_t, int);
typedef tree(mark_name_t, int)    mark_map_t;
typedef tree_it(mark_name_t, int) mark_map_it;

typedef struct {
    pthread_mutex_t err_mtx;

    SDL_Renderer *sdl_ren;
    array_t       elements;
    array_t       fonts;
    macro_map_t   macros;
    char         *collect_macro;
    int           beg_end_match;
    array_t       macro_use_stack;
    u32           r, g, b;
    double        speed;
    char         *bullet_strings[MAX_BULLET_LEVEL];
    image_map_t   images;
    mark_map_t    marks;
    u32           counter;

    u32           w, h;
    font_cache_t *cur_font;
    int           draw_x,   draw_y;
    int           view_x,   view_y;
    int           max_view_slides;
    u32           n_points, point;
    int           save_points[2048];
    int           dst_view_y;
    u64           anim_t;
    int           is_animating;
    int           movement_started;
    int           is_translating;

    char         *pres_dir;
} pres_t;

pres_t build_presentation(const char *path, SDL_Renderer *sdl_ren);
void free_presentation(pres_t *pres);
char * pres_get_font_name_by_id(pres_t *pres, u32 id);
font_cache_t * pres_get_elem_font(pres_t *pres, pres_elem_t *elem);
pres_image_data_t * pres_get_image_data(pres_t *pres, const char *image);
sdl_texture_t pres_get_image_texture(pres_t *pres, const char *image);
array_t get_wrap_points(pres_t *pres, const unsigned char *bytes, int l_margin, int r_margin, array_t *line_widths);

void pres_clear_and_draw_bg(pres_t *pres);
void draw_presentation(pres_t *pres);
void draw_presentation_no_clear(pres_t *pres);
void update_presentation(pres_t *pres);
void pres_restore_point(pres_t *pres, int point);
void pres_next_point(pres_t *pres);
void pres_prev_point(pres_t *pres);
void pres_first_point(pres_t *pres);
void pres_last_point(pres_t *pres);

#endif
