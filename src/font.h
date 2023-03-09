#ifndef __FONT_H__
#define __FONT_H__

#include "internal.h"
#include "tree.h"

typedef char        *font_name_t;
typedef SDL_Texture *texture_ptr_t;
typedef u64          char_code_t;

typedef struct {
    texture_ptr_t texture;    /* which texture the entry is in       */
    u32           x, y, w, h; /* containing rectangle in the texture */
    u32           adjust_x,
                  adjust_y;
    u32           pen_advance_x,
                  pen_advance_y;
} font_entry_t;

use_tree(char_code_t, font_entry_t);
typedef tree(char_code_t, font_entry_t)    font_entry_map_t;
typedef tree_it(char_code_t, font_entry_t) font_entry_map_it;

typedef struct {
    FT_Face           ft_face;
    texture_ptr_t     ascii_texture;
    font_entry_t      ascii_entries[256];
    font_entry_map_t  non_ascii_entry_map;
    u32               line_height;
    u32               size;
    const char       *path;
} font_cache_t;

use_tree(font_name_t, font_cache_t);

typedef tree(font_name_t, font_cache_t)    font_map_t;
typedef tree_it(font_name_t, font_cache_t) font_map_it;


extern font_map_t font_map;
extern FT_Library ft_lib;

int           init_font(void);
font_cache_t *get_or_load_font(const char *name, u32 size, SDL_Renderer *sdl_ren);
char_code_t   get_char_code(const char *str, int *n_bytes);
font_entry_t *get_glyph(font_cache_t *font, char_code_t ch, SDL_Renderer *sdl_ren);
void          set_font_color(font_cache_t *font, int r, int g, int b);

#endif
