#include "font.h"

int init_font(void) {
    int err;

    font_map = tree_make_c(font_name_t, font_cache_t, strcmp);

    err = FT_Init_FreeType(&ft_lib);

    if (err) {
        printf("font init error\n");
        return 0;
    }

    return 1;
}

font_cache_t *get_or_load_font(const char *name, u32 size, SDL_Renderer *sdl_ren) {
    char          lookup_buff[256];
    font_map_it   it;
    int           err;
    font_cache_t  cache;
    FT_Bitmap     b;
    FT_GlyphSlot  g;
    u32          *pixels, max_w, max_h, num_pixels, p, num_glyph_pixels, r, c, max_glyph_pixels;
    int           i, j, k, l, m, x, y;
    SDL_Rect      rect;


    snprintf(lookup_buff, sizeof(lookup_buff), "%s:%u", name, size);

    it = tree_lookup(font_map, lookup_buff);

    if (tree_it_good(it)) {
        return &tree_it_val(it);
    }

    err = FT_New_Face(ft_lib, name, 0, &cache.ft_face);

    if (err == FT_Err_Unknown_File_Format) {
        ERR("font not a font error\n");
    } else if (err) {
        ERR("font load error\n");
    }

    err = FT_Set_Char_Size(
                cache.ft_face, /* handle to face object           */
                0,             /* char_width in 1/64th of points  */
                size*64,       /* char_height in 1/64th of points */
                300,           /* horizontal device resolution    */
                300 );         /* vertical device resolution      */

    if (err) {
        ERR("font size err\n");
    }

    /* Find out how many pixels to allocate. */
    max_w = max_h = 0;
    for (i = 0; i < 256; i += 1) {
        FT_Load_Char(cache.ft_face, i, FT_LOAD_RENDER);
        g = cache.ft_face->glyph;
        b = g->bitmap;

        if (b.width > max_w) { max_w = b.width; }
        if (b.rows > max_h)  { max_h = b.rows;  }
    }
    max_glyph_pixels = max_w * max_h;
    num_pixels       = 256 * max_glyph_pixels;

    rect.x = rect.y = 0;
    rect.w = 16 * max_w;
    rect.h = 16 * max_h;

    cache.line_height = max_h;

    /* Create the pixel buffer and texture. */
    pixels = (u32*)malloc(sizeof(u32) * num_pixels);
    memset(pixels, 0, sizeof(u32) * num_pixels);
    cache.ascii_texture = SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, rect.w, rect.h);
    SDL_SetTextureBlendMode(cache.ascii_texture, SDL_BLENDMODE_BLEND);

    /* Draw each char into the pixel buffer. */
    /* Set entry values. */
    p = 0;
    for (i = 0; i < 16; i += 1) {
        for (j = 0; j < 16; j += 1) {
            y = i * max_h;
            x = j * max_w;

            c = i * 16 + j;

            if (!isprint(c)) {
                for (k = 0; k < max_h; k += 1) {
                    for (l = 0; l < max_w; l += 1) {
                        m = k * max_w + l;
                        pixels[p + m] = 0x00000000;
                    }
                }
                goto next;
            }

            FT_Load_Char(cache.ft_face, c, FT_LOAD_RENDER);
            g                = cache.ft_face->glyph;
            b                = g->bitmap;
            num_glyph_pixels = b.width * b.rows;

            cache.ascii_entries[c].texture       = cache.ascii_texture;
            cache.ascii_entries[c].x             = x;
            cache.ascii_entries[c].y             = y;
            cache.ascii_entries[c].w             = b.width;
            cache.ascii_entries[c].h             = b.rows;
            cache.ascii_entries[c].adjust_x      = g->bitmap_left;
            cache.ascii_entries[c].adjust_y      = g->bitmap_top;
            cache.ascii_entries[c].pen_advance_x = g->advance.x >> 6;
            cache.ascii_entries[c].pen_advance_y = g->advance.y >> 6;

#define P() (((i * max_h + k) * rect.w) + (j * max_w) + l)

            for (k = 0; k < b.rows; k += 1) {
                for (l = 0; l < b.width; l += 1) {
                    m = k * b.width + l;
                    r = b.buffer[m];
                    p = P();
                    if (r) {
                        pixels[p] |= r << 24;
                        pixels[p] |= r << 16;
                        pixels[p] |= r << 8;
                        pixels[p] |= r;
                    } else {
                        pixels[p] = 0x00000000;
                    }
                }
                for (l = b.width; l < max_w; l += 1) {
                    p = P();
                    pixels[p] = 0x00000000;
                }
            }
            for (k = b.rows; k < max_h; k += 1) {
                for (l = 0; l < max_w; l += 1) {
                    p = P();
                    pixels[p] = 0x00000000;
             }
            }

#undef P
next:;
        }
    }

    SDL_UpdateTexture(cache.ascii_texture, &rect, pixels, sizeof(u32) * rect.w);
    free(pixels);

    it = tree_insert(font_map, strdup(lookup_buff), cache);

    return &tree_it_val(it);
}

font_entry_t *get_glyph(font_cache_t *font, char_code_t ch) {
    if (ch < 256) {
        return &font->ascii_entries[ch];
    }

    return NULL;
}
