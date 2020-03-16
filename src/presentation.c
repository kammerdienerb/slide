#include "presentation.h"
#include "parse.h"
#include "font.h"

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
    char        *cmd;
    pres_elem_t  elem;
    u32          font_id;
    u32          font_size;
    u32          r, g, b;
    u32          l_margin, r_margin;
} build_ctx_t;

static void commit_element(pres_t *pres, build_ctx_t *ctx) {
    if (!ctx->elem.kind) {
        return;
    }

    ctx->elem.font_id   = ctx->font_id;
    ctx->elem.font_size = ctx->font_size;
    ctx->elem.r         = ctx->r;
    ctx->elem.g         = ctx->g;
    ctx->elem.b         = ctx->b;
    ctx->elem.l_margin  = ctx->l_margin;
    ctx->elem.r_margin  = ctx->r_margin;

    array_push(pres->elements, ctx->elem);

    memset(&ctx->elem, 0, sizeof(ctx->elem));
}

static int   I;
static float F;
static char *S;

#define GET_I(idx)                                                         \
do {                                                                       \
    if (array_len(words) <= idx                                            \
    ||  !sscanf(*(char**)array_item(words, idx), "%d", &I)) {              \
        ERR("line %d in command '%s': expected integer for argument %d\n", \
             ctx->line, ctx->cmd, idx);                                    \
    }                                                                      \
} while (0)

#define GET_F(idx)                                                       \
do {                                                                     \
    if (array_len(words) <= idx                                          \
    ||  !sscanf(*(char**)array_item(words, idx), "%f", &F)) {            \
        ERR("line %d in command '%s': expected float for argument %d\n", \
             ctx->line, ctx->cmd, idx);                                  \
    }                                                                    \
} while (0)

#define GET_S(idx)                                                        \
do {                                                                      \
    if (array_len(words) <= idx) {                                        \
        ERR("line %d in command '%s': expected string for argument %d\n", \
             ctx->line, ctx->cmd, idx);                                   \
    }                                                                     \
    S = *(char**)array_item(words, idx);                                  \
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
        ERR("line %d in command '%s': nothing to end\n",
            ctx->line, ctx->cmd);
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
        ERR("line %d in command '%s': '%s' has not been defined\n",
            ctx->line, ctx->cmd, S);
    }

    array_traverse(tree_it_val(it), line) {
        do_line(pres, ctx, *line);
    }
}

DEF_CMD(font) {
    commit_element(pres, ctx);
    GET_S(1);
    ctx->font_id = get_or_add_font_by_id(pres, S);
    GET_I(2);
    ctx->font_size = I;
}

DEF_CMD(bg) {
    GET_F(1); LIMIT(F); pres->r = (u32)(F * 255);
    GET_F(2); LIMIT(F); pres->g = (u32)(F * 255);
    GET_F(3); LIMIT(F); pres->b = (u32)(F * 255);
}

DEF_CMD(fg) {
    commit_element(pres, ctx);

    GET_F(1); LIMIT(F); ctx->r = (u32)(F * 255);
    GET_F(2); LIMIT(F); ctx->g = (u32)(F * 255);
    GET_F(3); LIMIT(F); ctx->b = (u32)(F * 255);
}

DEF_CMD(lmargin) {
    commit_element(pres, ctx);

    GET_F(1); LIMIT(F);
    ctx->l_margin = F * SCREEN_WIDTH;
}

DEF_CMD(rmargin) {
    commit_element(pres, ctx);

    GET_F(1); LIMIT(F);
    ctx->r_margin = F * SCREEN_WIDTH;
}

DEF_CMD(margin) {
    commit_element(pres, ctx);

    GET_F(1); LIMIT(F);
    ctx->l_margin = ctx->r_margin = F * SCREEN_WIDTH;
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


#undef LIMIT
#undef DEF_CMD


static void do_command(pres_t *pres, build_ctx_t *ctx, array_t words) {
    char  *cmd;
    int    i;
    float  f;
    char  *s;

#define CALL_CMD(name)            \
do {                              \
    ctx->cmd = #name;             \
    cmd_##name(pres, ctx, words); \
} while (0)

    if (ctx->elem.kind == PRES_PARA) {
        commit_element(pres, ctx);
    }

    cmd = *(char**)array_item(words, 0);


    if      (strcmp(cmd, "point")   == 0) { CALL_CMD(point);   }
    else if (strcmp(cmd, "speed")   == 0) { CALL_CMD(speed);   }
    else if (strcmp(cmd, "begin")   == 0) { CALL_CMD(begin);   }
    else if (strcmp(cmd, "end")     == 0) { CALL_CMD(end);     }
    else if (strcmp(cmd, "use")     == 0) { CALL_CMD(use);     }
    else if (strcmp(cmd, "font")    == 0) { CALL_CMD(font);    }
    else if (strcmp(cmd, "bg")      == 0) { CALL_CMD(bg);      }
    else if (strcmp(cmd, "fg")      == 0) { CALL_CMD(fg);      }
    else if (strcmp(cmd, "margin")  == 0) { CALL_CMD(margin);  }
    else if (strcmp(cmd, "lmargin") == 0) { CALL_CMD(lmargin); }
    else if (strcmp(cmd, "rmargin") == 0) { CALL_CMD(rmargin); }
    else if (strcmp(cmd, "vspace")  == 0) { CALL_CMD(vspace);  }
    else if (strcmp(cmd, "vfill")   == 0) { CALL_CMD(vfill);   }
    else {
        ERR("line %d: unknown command '%s'\n", ctx->line, cmd);
    }

#undef CALL_CMD
}

#undef GET_I
#undef GET_F
#undef GET_S

static char space = ' ';

static void do_para(pres_t *pres, build_ctx_t *ctx, char *line, int line_len) {
    if (ctx->elem.kind != PRES_PARA) {
        commit_element(pres, ctx);
        ctx->elem.kind = PRES_PARA;
        ctx->elem.text = array_make(char);
    } else {
        array_push(ctx->elem.text, space);
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

pres_t build_presentation(const char *path) {
    pres_t        pres;
    build_ctx_t   ctx;
    FILE         *file;
    char          line[1024];
    char         *line_copy;
    int           line_len;
    array_t       words;
    char        **word;
    int           is_end;

    file = fopen(path, "r");
    if (!file) {
        ERR("could not open presentation file '%s'\n", path);
    }

    pres.elements      = array_make(pres_elem_t);
    pres.fonts         = array_make(char*);
    pres.macros        = tree_make_c(macro_name_t, array_t, strcmp);
    pres.collect_lines = NULL;
    pres.r             = pres.g = pres.b = 255;
    pres.speed         = 4.0;

    memset(&ctx, 0, sizeof(ctx));
    ctx.font_id   = get_or_add_font_by_id(&pres, "luximr.ttf");
    ctx.font_size = 16;
    ctx.r         = ctx.g = ctx.b = 255;

    /* Add a point to the beginning of the presentation. */
    ctx.elem.kind = PRES_POINT;
    commit_element(&pres, &ctx);
    ctx.elem.kind = 0;

    while (fgets(line, sizeof(line), file)) {
        is_end    = 0;
        ctx.line += 1;

        if (pres.collect_lines != NULL) {
            line_len = strlen(line);
            if (line_len != 0 && line[0] == ':') {
                words = sh_split(line + 1);
                if (array_len(words)) {
                    word = array_item(words, 0);
                    is_end = !strcmp(*word, "end");
                }
                array_traverse(words, word) { free(*word); }
                array_free(words);
                if (is_end) {
                    goto doline;
                }
            }

            line_copy = strdup(line);
            array_push(*pres.collect_lines, line_copy);
        } else {
doline:
            do_line(&pres, &ctx, line);
        }
    }

    commit_element(&pres, &ctx);

    fclose(file);

    return pres;
}

void free_presentation(pres_t *pres) {
    pres_elem_t  *eit;
    char        **fit;

    tree_free(pres->macros);

    array_traverse(pres->fonts, fit) { free(*fit); }
    array_free(pres->fonts);

    array_traverse(pres->elements, eit) {
        if (eit->kind == PRES_PARA) {
            array_free(eit->text);
        }
    }
    array_free(pres->elements);

}
