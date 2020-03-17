#include "presentation.h"
#include "font.h"

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
    ctx->l_margin = F * SCREEN_WIDTH;
}

DEF_CMD(rmargin) {
    GET_F(1); LIMIT(F);
    ctx->r_margin = F * SCREEN_WIDTH;
}

DEF_CMD(margin) {
    GET_F(1); LIMIT(F);
    ctx->l_margin = ctx->r_margin = F * SCREEN_WIDTH;
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
    ctx->elem.y = F * SCREEN_HEIGHT;

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

static image_path_t ensure_image(pres_t *pres, build_ctx_t *ctx, const char *path) {
    image_map_it   it;
    sdl_texture_t  texture;
    int            want_fmt, orig_fmt;
    int            w, h, depth, pitch, pixel_format;
    unsigned char *image_data;
    SDL_Surface   *surface;

    it = tree_lookup(pres->images, (char*)path);

    if (tree_it_good(it)) {
        return tree_it_key(it);
    }

    want_fmt   = STBI_rgb_alpha;
    image_data = stbi_load(path, &w, &h, &orig_fmt, want_fmt);
    if (image_data == NULL) {
        BUILD_ERR("loading image '%s' failed\n    %s\n", path, stbi_failure_reason());
    }

    depth        = 32;
    pitch        = 4 * w;
    pixel_format = SDL_PIXELFORMAT_RGBA32;

    surface = SDL_CreateRGBSurfaceWithFormatFrom((void*)image_data, w, h,
                                                 depth, pitch, pixel_format);
    texture = SDL_CreateTextureFromSurface(pres->sdl_ren, surface);

    SDL_FreeSurface(surface);
    stbi_image_free(image_data);

    it = tree_insert(pres->images, strdup(path), texture);

    return tree_it_key(it);
}

DEF_CMD(image) {
    commit_element(pres, ctx);

    GET_S(1);
    ctx->elem.kind  = PRES_IMAGE;
    ctx->elem.image = ensure_image(pres, ctx, S);

    GET_F(2); LIMIT(F);
    ctx->elem.w = F * SCREEN_WIDTH;
    GET_F(3); LIMIT(F);
    ctx->elem.h = F * SCREEN_HEIGHT;

    commit_element(pres, ctx);
}

DEF_CMD(translate) {
    commit_element(pres, ctx);

    ctx->elem.kind = PRES_TRANSLATE;
    GET_F(1);
    ctx->elem.x = F * SCREEN_WIDTH;
    GET_F(2);
    ctx->elem.y = F * SCREEN_HEIGHT;

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


    if      (strcmp(cmd, "point")     == 0) { CALL_CMD(point);     }
    else if (strcmp(cmd, "speed")     == 0) { CALL_CMD(speed);     }
    else if (strcmp(cmd, "begin")     == 0) { CALL_CMD(begin);     }
    else if (strcmp(cmd, "end")       == 0) { CALL_CMD(end);       }
    else if (strcmp(cmd, "use")       == 0) { CALL_CMD(use);       }
    else if (strcmp(cmd, "include")   == 0) { CALL_CMD(include);   }
    else if (strcmp(cmd, "font")      == 0) { CALL_CMD(font);      }
    else if (strcmp(cmd, "size")      == 0) { CALL_CMD(size);      }
    else if (strcmp(cmd, "bg")        == 0) { CALL_CMD(bg);        }
    else if (strcmp(cmd, "bgx")       == 0) { CALL_CMD(bgx);       }
    else if (strcmp(cmd, "fg")        == 0) { CALL_CMD(fg);        }
    else if (strcmp(cmd, "fgx")       == 0) { CALL_CMD(fgx);       }
    else if (strcmp(cmd, "margin")    == 0) { CALL_CMD(margin);    }
    else if (strcmp(cmd, "lmargin")   == 0) { CALL_CMD(lmargin);   }
    else if (strcmp(cmd, "rmargin")   == 0) { CALL_CMD(rmargin);   }
    else if (strcmp(cmd, "ljust")     == 0) { CALL_CMD(ljust);     }
    else if (strcmp(cmd, "cjust")     == 0) { CALL_CMD(cjust);     }
    else if (strcmp(cmd, "rjust")     == 0) { CALL_CMD(rjust);     }
    else if (strcmp(cmd, "vspace")    == 0) { CALL_CMD(vspace);    }
    else if (strcmp(cmd, "vfill")     == 0) { CALL_CMD(vfill);     }
    else if (strcmp(cmd, "bullet")    == 0) { CALL_CMD(bullet);    }
    else if (strcmp(cmd, "image")     == 0) { CALL_CMD(image);     }
    else if (strcmp(cmd, "translate") == 0) { CALL_CMD(translate); }
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

    pres.sdl_ren       = sdl_ren;
    pres.elements      = array_make(pres_elem_t);
    pres.fonts         = array_make(char*);
    pres.macros        = tree_make_c(macro_name_t, array_t, strcmp);
    pres.collect_lines = NULL;
    pres.r             = pres.g = pres.b = 255;
    pres.speed         = 4.0;

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
