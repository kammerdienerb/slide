#include "presentation.h"

static array_t sh_split(char *s) {
    array_t  r;
    char    *copy,
            *sub,
            *sub_p;
    char     c, prev;
    int      len,
             start,
             end,
             q,
             sub_len,
             i;

    r     = array_make(char*);
    copy  = strdup(s);
    len   = strlen(copy);
    start = 0;
    end   = 0;
    prev  = 0;

    while (start < len && isspace(copy[start])) { start += 1; }

    while (start < len) {
        c   = copy[start];
        q   = 0;
        end = start;

        if (c == '#' && prev != '\\') {
            break;
        } else if (c == '"') {
            start += 1;
            prev   = copy[end];
            while (end + 1 < len
            &&    (copy[end + 1] != '"' || prev == '\\')) {
                end += 1;
                prev = copy[end];
            }
            q = 1;
        } else if (c == '\'') {
            start += 1;
            prev   = copy[end];
            while (end + 1 < len
            &&    (copy[end + 1] != '\'' || prev == '\\')) {
                end += 1;
                prev = copy[end];
            }
            q = 1;
        } else {
            while (end + 1 < len
            &&     !isspace(copy[end + 1])) {
                end += 1;
            }
        }

        sub_len = end - start + 1;
        if (q && sub_len == 0 && start == len) {
            sub    = malloc(2);
            sub[0] = copy[end];
            sub[1] = 0;
        } else {
            sub   = malloc(sub_len + 1);
            sub_p = sub;
            for (i = 0; i < sub_len; i += 1) {
                c = copy[start + i];
                if (c == '\\'
                &&  i < sub_len - 1
                &&  (copy[start + i + 1] == '"'
                || copy[start + i + 1] == '\''
                || copy[start + i + 1] == '#')) {
                    continue;
                }
                *sub_p = c;
                sub_p += 1;
            }
            *sub_p = 0;
        }

        array_push(r, sub);

        end  += q;
        start = end + 1;

        while (start < len && isspace(copy[start])) { start += 1; }
    }

    free(copy);

    return r;
}

char * pres_get_font_name_by_id(pres_t *pres, u32 id) {
    if (id >= array_len(pres->fonts)) {
        return NULL;
    }

    return *(char**)array_item(pres->fonts, id);
}

static u32 get_or_add_font_by_id(pres_t *pres, char *font) {
    char **it;
    u32    id;
    char  *copy;

    id = 0;
    array_traverse(pres->fonts, it) {
        if (strcmp(font, *it) == 0) {
            return id;
        }
        id += 1;
    }

    copy = strdup(font);
    array_push(pres->fonts, copy);

    return array_len(pres->fonts) - 1;
}

typedef struct {
    int          line;
    const char  *path;
    char        *cmd;
    pres_elem_t  elem;
    u32          font_id;
    u32          font_size;
    u32          r, g, b;
    u32          l_margin, r_margin;
    int          justification;
} build_ctx_t;

static void commit_element(pres_t *pres, build_ctx_t *ctx) {
    if (!ctx->elem.kind) {
        return;
    }

    ctx->elem.font_id       = ctx->font_id;
    ctx->elem.font_size     = ctx->font_size;
    ctx->elem.r             = ctx->r;
    ctx->elem.g             = ctx->g;
    ctx->elem.b             = ctx->b;
    ctx->elem.l_margin      = ctx->l_margin;
    ctx->elem.r_margin      = ctx->r_margin;
    ctx->elem.justification = ctx->justification;

    array_push(pres->elements, ctx->elem);

    memset(&ctx->elem, 0, sizeof(ctx->elem));
}

static int   I;
static float F;
static char *S;

#define BUILD_ERR(fmt, ...) do {                                 \
fprintf(stderr, "[slide] ERROR: %s :: %d in command '%s': " fmt, \
        ctx->path, ctx->line, ctx->cmd, ##__VA_ARGS__);          \
    exit(1);                                                     \
} while (0)

#define GET_I(idx)                                            \
do {                                                          \
    if (array_len(words) <= idx                               \
    ||  !sscanf(*(char**)array_item(words, idx), "%d", &I)) { \
        BUILD_ERR("expected integer for argument %d\n", idx); \
    }                                                         \
} while (0)

#define GET_F(idx)                                            \
do {                                                          \
    if (array_len(words) <= idx                               \
    ||  !sscanf(*(char**)array_item(words, idx), "%f", &F)) { \
        BUILD_ERR("expected float for argument %d\n", idx);   \
    }                                                         \
} while (0)

#define GET_S(idx)                                            \
do {                                                          \
    if (array_len(words) <= idx) {                            \
        BUILD_ERR("expected string for argument %d\n", idx);  \
    }                                                         \
    S = *(char**)array_item(words, idx);                      \
} while (0)


#define LIMIT(_f)                        \
do {                                     \
    if      ((_f) > 1.0) { (_f) = 1.0; } \
    else if ((_f) < 0.0) { (_f) = 0.0; } \
} while (0)


#define DEF_CMD(cmd_name) \
static void cmd_##cmd_name(pres_t *pres, build_ctx_t *ctx, array_t words)

DEF_CMD(point) {
    pres_elem_t *last;

    last = array_last(pres->elements);
    if (last->kind != PRES_BREAK) {
        commit_element(pres, ctx);
    }

    ctx->elem.kind = PRES_POINT;
    commit_element(pres, ctx);
}

DEF_CMD(speed) {
    GET_F(1);
    if (F < 0.0) {
        F = 0.1;
    } else if (F > 100.0) {
        F = 100.0;
    }
    pres->speed = F;
}

DEF_CMD(resolution) {
    GET_I(1);
    if (I <= 0) {
        BUILD_ERR("resolution values must be greater than zero\n");
    }
    pres->w = I;
    GET_I(2);
    if (I <= 0) {
        BUILD_ERR("resolution values must be greater than zero\n");
    }
    pres->h = I;
}

DEF_CMD(begin) {
    macro_map_it   it;
    char         **line_it;
    array_t       *lines, new_array;

    if (pres->collect_lines != NULL) {
        BUILD_ERR("begin within begin\n");
    }

    commit_element(pres, ctx);
    GET_S(1);

    it = tree_lookup(pres->macros, S);

    if (tree_it_good(it)) {
        lines = &tree_it_val(it);
        array_traverse(*lines, line_it) { free(*line_it); }
        array_clear(*lines);
    } else {
        new_array = array_make(char*);
        it        = tree_insert(pres->macros, strdup(S), new_array);
        lines     = &tree_it_val(it);
    }

    pres->collect_lines = lines;
}

DEF_CMD(end) {
    if (pres->collect_lines == NULL) {
        BUILD_ERR("nothing to end\n");
    }

    pres->collect_lines = NULL;
}

static void do_line(pres_t *pres, build_ctx_t *ctx, char *line);

DEF_CMD(use) {
    macro_map_it   it;
    char         **line;

    GET_S(1);
    it = tree_lookup(pres->macros, S);

    if (!tree_it_good(it)) {
        BUILD_ERR("'%s' has not been defined\n", S);
    }

    array_traverse(tree_it_val(it), line) {
        do_line(pres, ctx, *line);
    }
}

static int do_file(pres_t *pres, build_ctx_t *ctx, const char *path);

DEF_CMD(include) {
    GET_S(1);
    if (!do_file(pres, ctx, S)) {
        BUILD_ERR("could not open presentation file '%s'\n", S);
    }
}

DEF_CMD(font) {
    GET_S(1);
    ctx->font_id = get_or_add_font_by_id(pres, S);
}

DEF_CMD(size) {
    GET_I(1);
    ctx->font_size = I >= 0 ? I : 0 - I;
}

DEF_CMD(bg) {
    GET_F(1); LIMIT(F); pres->r = (u32)(F * 255);
    GET_F(2); LIMIT(F); pres->g = (u32)(F * 255);
    GET_F(3); LIMIT(F); pres->b = (u32)(F * 255);
}

static void parse_hex(pres_t *pres, build_ctx_t * ctx, const char *s, u32 *r, u32 *g, u32 *b) {
    u32 val;

    if (!sscanf(s, "%x", &val)) {
        BUILD_ERR("expected hex value\n");
    }

    *r = (val & 0xFF0000) >> 16;
    *g = (val & 0x00FF00) >> 8;
    *b = (val & 0x0000FF);
}

DEF_CMD(bgx) {
    GET_S(1);

    parse_hex(pres, ctx, S, &pres->r, &pres->g, &pres->b);
}

DEF_CMD(fg) {
    GET_F(1); LIMIT(F); ctx->r = (u32)(F * 255);
    GET_F(2); LIMIT(F); ctx->g = (u32)(F * 255);
    GET_F(3); LIMIT(F); ctx->b = (u32)(F * 255);
}

DEF_CMD(fgx) {
    GET_S(1);
    parse_hex(pres, ctx, S, &ctx->r, &ctx->g, &ctx->b);
}

DEF_CMD(lmargin) {
    GET_F(1); LIMIT(F);
    ctx->l_margin = F * pres->w;
}

DEF_CMD(rmargin) {
    GET_F(1); LIMIT(F);
    ctx->r_margin = F * pres->w;
}

DEF_CMD(margin) {
    GET_F(1); LIMIT(F);
    ctx->l_margin = ctx->r_margin = F * pres->w;
}

DEF_CMD(ljust) {
    ctx->justification = JUST_L;
}

DEF_CMD(cjust) {
    ctx->justification = JUST_C;
}

DEF_CMD(rjust) {
    ctx->justification = JUST_R;
}

DEF_CMD(vspace) {
    commit_element(pres, ctx);

    ctx->elem.kind = PRES_VSPACE;
    GET_F(1); LIMIT(F);
    ctx->elem.y = F * pres->h;

    commit_element(pres, ctx);
}

DEF_CMD(vfill) {
    commit_element(pres, ctx);
    ctx->elem.kind = PRES_VFILL;
    commit_element(pres, ctx);
}


DEF_CMD(bullet) {
    commit_element(pres, ctx);

    GET_I(1);

    if (I < 1 || I > 3) {
        BUILD_ERR("level must be between 1 and 3\n");
    }

    ctx->elem.level = I;
    ctx->elem.kind  = PRES_BULLET;
    ctx->elem.text  = array_make(char);
}

typedef struct {
    pres_t        *pres;
    build_ctx_t   *ctx;
    char          *path;
    sdl_texture_t *texture_ptr;
} async_load_image_payload_t;

static void async_load_image(void *arg) {
    async_load_image_payload_t *payload;
    pres_t                     *pres;
    build_ctx_t                *ctx;
    char                       *path;
    sdl_texture_t              *texture_ptr;
    int                         want_fmt, orig_fmt;
    int                         w, h, depth, pitch, pixel_format;
    unsigned char              *image_data;
    SDL_Surface                *surface;

    payload = arg;

    pres        = payload->pres;
    ctx         = payload->ctx;
    path        = payload->path;
    texture_ptr = payload->texture_ptr;

    want_fmt   = STBI_rgb_alpha;
    image_data = stbi_load(path, &w, &h, &orig_fmt, want_fmt);
    if (image_data == NULL) {
        BUILD_ERR("loading image '%s' failed\n    %s\n", path, stbi_failure_reason());
    }

    depth        = 32;
    pitch        = 4 * w;
    pixel_format = SDL_PIXELFORMAT_RGBA32;

    pthread_mutex_lock(&pres->images_mutex);
        surface      = SDL_CreateRGBSurfaceWithFormatFrom((void*)image_data, w, h,
                                                          depth, pitch, pixel_format);
        *texture_ptr = SDL_CreateTextureFromSurface(pres->sdl_ren, surface);
        SDL_FreeSurface(surface);
    pthread_mutex_unlock(&pres->images_mutex);

    stbi_image_free(image_data);

    free(arg);
}

static image_path_t ensure_image(pres_t *pres, build_ctx_t *ctx, const char *path) {
    image_map_it                it;
    async_load_image_payload_t *payload;

    it = tree_lookup(pres->images, (char*)path);

    if (tree_it_good(it)) {
        return tree_it_key(it);
    }

    it = tree_insert(pres->images, strdup(path), NULL);

    payload = malloc(sizeof(*payload));

    payload->pres        = pres;
    payload->ctx         = ctx;
    payload->path        = tree_it_key(it);
    payload->texture_ptr = &tree_it_val(it);

    tp_add_task(pres->tp, async_load_image, payload);

    return tree_it_key(it);
}

DEF_CMD(image) {
    commit_element(pres, ctx);

    GET_S(1);
    ctx->elem.kind  = PRES_IMAGE;
    ctx->elem.image = ensure_image(pres, ctx, S);

    GET_F(2); LIMIT(F);
    ctx->elem.w = F * pres->w;
    GET_F(3); LIMIT(F);
    ctx->elem.h = F * pres->h;

    commit_element(pres, ctx);
}

DEF_CMD(translate) {
    commit_element(pres, ctx);

    ctx->elem.kind = PRES_TRANSLATE;
    GET_F(1);
    ctx->elem.x = F * pres->w;
    GET_F(2);
    ctx->elem.y = F * pres->h;

    commit_element(pres, ctx);
}


#undef LIMIT
#undef DEF_CMD


static void do_command(pres_t *pres, build_ctx_t *ctx, array_t words) {
    char  *cmd;

#define CALL_CMD(name)            \
do {                              \
    ctx->cmd = #name;             \
    cmd_##name(pres, ctx, words); \
} while (0)

    cmd = *(char**)array_item(words, 0);


    if      (strcmp(cmd, "point")      == 0) { CALL_CMD(point);      }
    else if (strcmp(cmd, "speed")      == 0) { CALL_CMD(speed);      }
    else if (strcmp(cmd, "resolution") == 0) { CALL_CMD(resolution); }
    else if (strcmp(cmd, "begin")      == 0) { CALL_CMD(begin);      }
    else if (strcmp(cmd, "end")        == 0) { CALL_CMD(end);        }
    else if (strcmp(cmd, "use")        == 0) { CALL_CMD(use);        }
    else if (strcmp(cmd, "include")    == 0) { CALL_CMD(include);    }
    else if (strcmp(cmd, "font")       == 0) { CALL_CMD(font);       }
    else if (strcmp(cmd, "size")       == 0) { CALL_CMD(size);       }
    else if (strcmp(cmd, "bg")         == 0) { CALL_CMD(bg);         }
    else if (strcmp(cmd, "bgx")        == 0) { CALL_CMD(bgx);        }
    else if (strcmp(cmd, "fg")         == 0) { CALL_CMD(fg);         }
    else if (strcmp(cmd, "fgx")        == 0) { CALL_CMD(fgx);        }
    else if (strcmp(cmd, "margin")     == 0) { CALL_CMD(margin);     }
    else if (strcmp(cmd, "lmargin")    == 0) { CALL_CMD(lmargin);    }
    else if (strcmp(cmd, "rmargin")    == 0) { CALL_CMD(rmargin);    }
    else if (strcmp(cmd, "ljust")      == 0) { CALL_CMD(ljust);      }
    else if (strcmp(cmd, "cjust")      == 0) { CALL_CMD(cjust);      }
    else if (strcmp(cmd, "rjust")      == 0) { CALL_CMD(rjust);      }
    else if (strcmp(cmd, "vspace")     == 0) { CALL_CMD(vspace);     }
    else if (strcmp(cmd, "vfill")      == 0) { CALL_CMD(vfill);      }
    else if (strcmp(cmd, "bullet")     == 0) { CALL_CMD(bullet);     }
    else if (strcmp(cmd, "image")      == 0) { CALL_CMD(image);      }
    else if (strcmp(cmd, "translate")  == 0) { CALL_CMD(translate);  }
    else {
        ctx->cmd = cmd;
        BUILD_ERR("unknown command '%s'\n", cmd);
    }

#undef CALL_CMD
}

#undef GET_I
#undef GET_F
#undef GET_S

char space = ' ';

static void do_para(pres_t *pres, build_ctx_t *ctx, char *line, int line_len) {
    if (ctx->elem.kind != PRES_PARA
    &&  ctx->elem.kind != PRES_BULLET) {
        commit_element(pres, ctx);
        ctx->elem.kind = PRES_PARA;
        ctx->elem.text = array_make(char);
    } else {
        if (array_len(ctx->elem.text)) {
            array_push(ctx->elem.text, space);
        }
    }
    array_push_n(ctx->elem.text, line, line_len);
    array_zero_term(ctx->elem.text);
}

static void do_break(pres_t *pres, build_ctx_t *ctx) {
    if (ctx->elem.kind && ctx->elem.kind != PRES_BREAK) {
        commit_element(pres, ctx);
        ctx->elem.kind = PRES_BREAK;
    }
}

static void do_line(pres_t *pres, build_ctx_t *ctx, char *line) {
    int           line_len;
    array_t       words;
    char        **word;

    line_len = strlen(line);
    if (line_len == 0) {
        return;
    } else if (line_len == 1 && line[0] == '\n') {
        do_break(pres, ctx);
        return;
    }

    if (line[line_len - 1] == '\n') {
        line[line_len - 1]  = 0;
        line_len           -= 1;
    }

    if (line[0] == ':') {
        words = sh_split(line + 1);
        if (array_len(words)) {
            do_command(pres, ctx, words);
        }
        array_traverse(words, word) { free(*word); }
        array_free(words);
    } else {
        do_para(pres, ctx, line, line_len);
    }
}

static int do_file(pres_t *pres, build_ctx_t *ctx, const char *path) {
    FILE         *file;
    char          line[1024];
    char         *line_copy;
    int           line_len;
    array_t       words;
    char        **word;
    int           is_beg_or_end;
    int           save_line;
    const char   *save_path;

    file = fopen(path, "r");
    if (!file) {
        return 0;
    }

    save_line = ctx->line;
    ctx->line = 0;
    save_path = ctx->path;
    ctx->path = path;

    while (fgets(line, sizeof(line), file)) {
        is_beg_or_end  = 0;
        ctx->line      += 1;

        if (pres->collect_lines != NULL) {
            line_len = strlen(line);
            if (line_len != 0 && line[0] == ':') {
                words = sh_split(line + 1);
                if (array_len(words)) {
                    word          = array_item(words, 0);
                    is_beg_or_end = (  !strcmp(*word, "end")
                                    || !strcmp(*word, "begin"));
                }
                array_traverse(words, word) { free(*word); }
                array_free(words);
                if (is_beg_or_end) {
                    goto doline;
                }
            }

            line_copy = strdup(line);
            array_push(*pres->collect_lines, line_copy);
        } else {
doline:
            do_line(pres, ctx, line);
        }
    }

    commit_element(pres, ctx);

    fclose(file);

    ctx->line = save_line;
    ctx->path = save_path;

    return 1;
}

pres_t build_presentation(const char *path, SDL_Renderer *sdl_ren) {
    pres_t        pres;
    build_ctx_t   ctx;

    memset(&pres, 0, sizeof(pres));

    pres.tp            = tp_make(8);
    pthread_mutex_init(&pres.images_mutex, NULL);

    pres.sdl_ren       = sdl_ren;
    pres.elements      = array_make(pres_elem_t);
    pres.fonts         = array_make(char*);
    pres.macros        = tree_make_c(macro_name_t, array_t, strcmp);
    pres.collect_lines = NULL;
    pres.r             = pres.g = pres.b = 255;
    pres.speed         = 4.0;
    pres.w             = DEFAULT_RES_W;
    pres.h             = DEFAULT_RES_H;

    pres.bullet_strings[0] = "• ";
    pres.bullet_strings[1] = "› ";
    pres.bullet_strings[2] = "– ";

    pres.images = tree_make_c(image_path_t, sdl_texture_t, strcmp);

    memset(&ctx, 0, sizeof(ctx));
    ctx.font_id       = get_or_add_font_by_id(&pres, "fonts/luximr.ttf");
    ctx.font_size     = 16;
    ctx.r             = ctx.g = ctx.b = 255;
    ctx.justification = JUST_L;

    /* Add a point to the beginning of the presentation. */
    ctx.elem.kind = PRES_POINT;
    commit_element(&pres, &ctx);
    ctx.elem.kind = 0;

    if (!do_file(&pres, &ctx, path)) {
        ERR("could not open presentation file '%s'\n", path);
    }

    tp_wait(pres.tp);
    tp_stop(pres.tp, TP_GRACEFUL);
    tp_free(pres.tp);

    return pres;
}

void free_presentation(pres_t *pres) {
    image_map_it   iit;
    macro_map_it   mit;
    char         **lit;
    pres_elem_t   *eit;
    char         **fit;

    tree_traverse(pres->images, iit) {
        free(tree_it_key(iit));
        SDL_DestroyTexture(tree_it_val(iit));
    }
    tree_free(pres->images);

    tree_traverse(pres->macros, mit) {
        free(tree_it_key(mit));
        array_traverse(tree_it_val(mit), lit) {
            free(*lit);
        }
        array_free(tree_it_val(mit));
    }
    tree_free(pres->macros);

    array_traverse(pres->fonts, fit) { free(*fit); }
    array_free(pres->fonts);

    array_traverse(pres->elements, eit) {
        if (eit->kind == PRES_PARA
        ||  eit->kind == PRES_BULLET) {
            array_free(eit->text);
        }
    }
    array_free(pres->elements);

}

sdl_texture_t pres_get_image_texture(pres_t *pres, const char *image) {
    image_map_it it;

    it = tree_lookup(pres->images, (char*)image);

    if (tree_it_good(it)) {
        return tree_it_val(it);
    }

    return NULL;
}


static void clear_and_draw_bg(pres_t *pres) {
    SDL_Rect r;
    int      sum;

    sum = pres->r + pres->g + pres->g;
    if (sum > ((3 * 255) / 2)) {
        SDL_SetRenderDrawColor(pres->sdl_ren, 0, 0, 0, 255);
    } else {
        SDL_SetRenderDrawColor(pres->sdl_ren, 255, 255, 255, 255);
    }
    SDL_RenderClear(pres->sdl_ren);

    r.x = r.y = 0;
    r.w = pres->w;
    r.h = pres->h;

    SDL_SetRenderDrawColor(pres->sdl_ren,
                           pres->r,
                           pres->g,
                           pres->b,
                           255);

    SDL_RenderFillRect(pres->sdl_ren, &r);

    SDL_SetRenderDrawColor(pres->sdl_ren,
                           255 - pres->r,
                           255 - pres->g,
                           255 - pres->b,
                           255);
}

static array_t get_wrap_points(pres_t *pres, const unsigned char *bytes, int l_margin, int r_margin, array_t *line_widths) {
    int           len;
    array_t       wrap_points;
    int           total_width;
    int           line_width;
    int           i, j;
    int           last_space;
    font_entry_t *entry;
    char_code_t   code;
    int           n_bytes_i, n_bytes_j;
    int           space_width;
    font_cache_t *font;

    font = pres->cur_font;

    code         = get_char_code(" ", &n_bytes_i);
    entry        = get_glyph(font, code, pres->sdl_ren);
    space_width  = entry->pen_advance_x;

    len = strlen((const char *)bytes);

    wrap_points  = array_make(int);
    *line_widths = array_make(int);
    total_width  = l_margin;
    last_space   = -1;

    for (i = 0; i < len;) {
        code  = get_char_code(((const char *)bytes) + i, &n_bytes_i);
        entry = get_glyph(font, code, pres->sdl_ren);

        total_width += entry->pen_advance_x;

        if (total_width > pres->w - r_margin) {
            line_width  = total_width;
            total_width = l_margin;

            if (last_space != -1) {
                for (j = last_space + 1; j <= i;) {
                    code         = get_char_code(((const char *)bytes) + j, &n_bytes_j);
                    entry        = get_glyph(font, code, pres->sdl_ren);
                    total_width += entry->pen_advance_x;
                    j += n_bytes_j;
                }

                line_width -= total_width;
                line_width -= space_width;
                array_push(*line_widths, line_width);
                array_push(wrap_points, last_space);
                last_space = -1;
            }
        }

        if (isspace(bytes[i])) { last_space = i; }

        i += n_bytes_i;
    }

    if (total_width > pres->w - r_margin) {
        line_width  = total_width;
        total_width = l_margin;

        if (last_space != -1) {
            for (j = last_space + 1; j <= i; j += 1) {
                entry        = get_glyph(font, bytes[j], pres->sdl_ren);
                total_width += entry->pen_advance_x;
            }

            line_width -= total_width;
            array_push(*line_widths, line_width);
            array_push(wrap_points, last_space);
        }
    }

    line_width = total_width - l_margin;
    array_push(*line_widths, line_width);

    return wrap_points;
}

void draw_string(pres_t *pres, const char *str, int l_margin, int r_margin, int justification) {
    int            _x, _y;
    int            len;
    int            i;
    font_entry_t  *entry;
    SDL_Rect       srect,
                   drect;
    array_t        wrap_points;
    array_t        line_widths;
    int           *wrap_it;
    int            wrapped;
    unsigned char *bytes;
    char_code_t    code;
    int            n_bytes;
    int            line;
    font_cache_t  *font;

    font = pres->cur_font;
    if (!font) { return; }

    _x = pres->draw_x + l_margin;
    _y = pres->draw_y;

    (void)_y;

    pres->draw_x  = _x;
    pres->draw_y += font->line_height;

    if (!str) { return; }

    bytes = (unsigned char*)str;

    len = strlen(str);

    wrap_points = get_wrap_points(pres, bytes, l_margin, r_margin, &line_widths);
    line        = 0;

    switch (justification) {
        case JUST_L: break;
        case JUST_R:
            pres->draw_x += (pres->w - l_margin - r_margin) - *(int*)array_item(line_widths, line);
            break;
        case JUST_C:
            pres->draw_x += ((pres->w - l_margin - r_margin) - *(int*)array_item(line_widths, line)) / 2;
            break;
    }

    for (i = 0; i < len;) {
        wrapped = 0;

        code  = get_char_code(str + i, &n_bytes);
        entry = get_glyph(font, code, pres->sdl_ren);

        srect.x = entry->x;
        srect.y = entry->y;
        srect.w = entry->w;
        srect.h = entry->h;

        drect.x = pres->draw_x + entry->adjust_x;
        drect.y = pres->draw_y - entry->adjust_y;
        drect.w = entry->w;
        drect.h = entry->h;

        array_traverse(wrap_points, wrap_it) {
            if (*wrap_it == i) {
                line   += 1;
                pres->draw_y += 1.25 * font->line_height;
                pres->draw_x  = _x;

                switch (justification) {
                    case JUST_L: break;
                    case JUST_R:
                        pres->draw_x += (pres->w - l_margin - r_margin) - *(int*)array_item(line_widths, line);
                        break;
                    case JUST_C:
                        pres->draw_x += ((pres->w - l_margin - r_margin) - *(int*)array_item(line_widths, line)) / 2;
                        break;
                }

                wrapped = 1;
                break;
            }
        }

        SDL_RenderCopy(pres->sdl_ren, entry->texture, &srect, &drect);

        if (!wrapped) {
            pres->draw_x += entry->pen_advance_x;
        }
        pres->draw_y += entry->pen_advance_y;

        i += n_bytes;
    }

    array_free(line_widths);
    array_free(wrap_points);
}

static void draw_para(pres_t *pres, pres_elem_t *elem) {
    pres->cur_font = get_or_load_font(
                        pres_get_font_name_by_id(pres, elem->font_id),
                        elem->font_size, pres->sdl_ren);

    set_font_color(pres->cur_font, elem->r, elem->g, elem->b);

    draw_string(pres,
                array_data(elem->text),
                elem->l_margin, elem->r_margin, elem->justification);

    pres->is_translating = 0;
}

static void draw_bullet(pres_t *pres, pres_elem_t *elem) {
    int save_draw_x,
        save_draw_y;
    int new_l_margin;

    pres->cur_font = get_or_load_font(
                        pres_get_font_name_by_id(pres, elem->font_id),
                        elem->font_size, pres->sdl_ren);

    set_font_color(pres->cur_font, elem->r, elem->g, elem->b);

    save_draw_x = pres->draw_x;
    save_draw_y = pres->draw_y;

    new_l_margin =   elem->l_margin
                   + ((0.05 * (elem->level - 1)) * pres->w);

    draw_string(pres,
                pres->bullet_strings[elem->level - 1],
                new_l_margin, elem->r_margin, JUST_L);

    new_l_margin = pres->draw_x - save_draw_x;
    pres->draw_x = save_draw_x;
    pres->draw_y = save_draw_y;

    draw_string(pres,
                array_data(elem->text),
                new_l_margin, elem->r_margin, elem->justification);

    pres->is_translating = 0;
}

static void draw_break(pres_t *pres, pres_elem_t *elem) {
    if (pres->cur_font) {
        pres->draw_y += 1.25 * pres->cur_font->line_height;
    }

    pres->is_translating = 0;
}

static void draw_vspace(pres_t *pres, pres_elem_t *elem) {
    pres->draw_y         += elem->y;
    pres->is_translating  = 0;
}

static void draw_vfill(pres_t *pres, pres_elem_t *elem) {
    pres->draw_y         += pres->h - ((pres->draw_y - pres->view_y) % pres->h);
    pres->is_translating  = 0;
}

static void handle_point(pres_t *pres, pres_elem_t *elem) {
    pres->save_points[pres->n_points]  = -(pres->draw_y - pres->view_y);
    pres->n_points                    += 1;
}

static void draw_image(pres_t *pres, pres_elem_t *elem) {
    sdl_texture_t image_texture;
    SDL_Rect      drect;

    image_texture = pres_get_image_texture(pres, elem->image);

    drect.x = pres->draw_x;
    drect.y = pres->draw_y;
    drect.w = elem->w;
    drect.h = elem->h;

    SDL_RenderCopy(pres->sdl_ren, image_texture, NULL, &drect);

    pres->draw_y         += drect.h;
    pres->is_translating  = 0;
}

static void draw_translate(pres_t *pres, pres_elem_t *elem) {
    pres->is_translating  = 1;
    pres->draw_x         += elem->x;
    pres->draw_y         += elem->y;
}

static void do_animation(pres_t *pres) {
    if (pres->is_animating) {
        if (pres->view_y < pres->dst_view_y) {
            pres->view_y += pres->speed * FPS_CAP_MS;
            if (pres->view_y > pres->dst_view_y) {
                pres->view_y = pres->dst_view_y;
            }
        } else if (pres->view_y > pres->dst_view_y) {
            pres->view_y -= pres->speed * FPS_CAP_MS;
            if (pres->view_y < pres->dst_view_y) {
                pres->view_y = pres->dst_view_y;
            }
        }

        if (pres->view_y == pres->dst_view_y) {
            pres->is_animating = 0;
        }
    } else {
        if (pres->n_points != 0) {
            if (pres->point >= pres->n_points) {
                pres->point = pres->n_points - 1;
            }

            if (pres->view_y != pres->save_points[pres->point]) {
                pres->dst_view_y   = pres->save_points[pres->point];
                pres->is_animating = 1;
            }
        }
    }
}

void draw_presentation(pres_t *pres) {
    pres_elem_t *elem;

    clear_and_draw_bg(pres);

    pres->draw_x   = pres->view_x;
    pres->draw_y   = pres->view_y;
    pres->n_points = 0;

    array_traverse(pres->elements, elem) {
        switch (elem->kind) {
            case PRES_PARA:      draw_para(pres, elem);      break;
            case PRES_BULLET:    draw_bullet(pres, elem);    break;
            case PRES_BREAK:     draw_break(pres, elem);     break;
            case PRES_VSPACE:    draw_vspace(pres, elem);    break;
            case PRES_VFILL:     draw_vfill(pres, elem);     break;
            case PRES_IMAGE:     draw_image(pres, elem);     break;
            case PRES_TRANSLATE: draw_translate(pres, elem); break;
            case PRES_POINT:     handle_point(pres, elem);   break;
        }

        if (!pres->is_translating) { pres->draw_x = 0; }
    }

    do_animation(pres);
}

void pres_next_point(pres_t *pres) {
    if (!pres->is_animating) {
        pres->point += 1;
    }
}

void pres_prev_point(pres_t *pres) {
    if (!pres->is_animating) {
        if (pres->point > 0) {
            pres->point -= 1;
        }
    }
}

void pres_first_point(pres_t *pres) {
    if (!pres->is_animating) {
        pres->point = 0;
    }
    pres->view_y = pres->save_points[pres->point];
}

void pres_last_point(pres_t *pres) {
    if (!pres->is_animating) {
        pres->point = pres->n_points - 1;
    }
    pres->view_y = pres->save_points[pres->point];
}
