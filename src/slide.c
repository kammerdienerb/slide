#include "internal.h"
#include "font.h"
#include "presentation.h"
#include "pdf.h"

typedef struct {
    int         renderer; /* 0 = hardware, 1 = software */
    const char *path;
    int         to_pdf;
    const char *to_pdf_name;
    float       pdf_quality;
} options_t;

options_t options;

static void get_env_options(void) {
    char *val;

    if ((val = getenv("SLIDE_RENDERER"))) {
        if (strcmp(val, "hw") == 0) {
            options.renderer = 0;
        } else if (strcmp(val, "sw") == 0) {
            options.renderer = 1;
        } else {
            printf("slide: invalid value for 'SLIDE_RENDERER'\n");
        }
    }
}

static char *usage =
"usage: slide [options] FILE\n"
"\n"
"options:\n"
"\n"
"--renderer\n"
"    Select the rendering method:\n"
"        'hw': (default) use harware rendering.\n"
"        'sw': use a software renderer.\n"
"--to-pdf[=NAME]\n"
"    Output to a PDF file instead of presenting.\n"
"    The PDF will be created as NAME if it is provided.\n"
"    Otherwise, basename(FILE).pdf will be used.\n"
"--pdf-quality=FLOAT\n"
"    Export the PDF at FLOAT quality where FLOAT is in the\n"
"    range [0.0, 1.0]. Default value is 1.0 (full quality).\n"
"--help\n"
"    Show this information.\n"
"\n"
;

static void err_usage(void)   { ERR("%s", usage);    }
static void print_usage(void) { printf("%s", usage); }

static void parse_options(int argc, char **argv) {
    int   i;
    char  buff[1024];
    char  path_cpy[1024];
    char *ext_point;

    options.pdf_quality = 1.0;

    for (i = 1; i < argc; i += 1) {
        if (strncmp(argv[i], "--renderer=", 11) == 0) {
            if (strcmp(argv[i] + 11, "hw") == 0) {
                options.renderer = 0;
            } else if (strcmp(argv[i] + 11, "sw") == 0) {
                options.renderer = 1;
            } else {
                err_usage();
            }
        } else if (strncmp(argv[i], "--to-pdf=", 9) == 0) {
            options.to_pdf_name = argv[i] + 9;
            if (strlen(options.to_pdf_name) == 0) {
                err_usage();
            }
            options.to_pdf = 1;
        } else if (strcmp(argv[i], "--to-pdf") == 0) {
            options.to_pdf = 1;
        } else if (strncmp(argv[i], "--pdf-quality=", 14) == 0) {
            if (!sscanf(argv[i] + 14, "%f", &options.pdf_quality)) {
                err_usage();
            }
            if (options.pdf_quality < 0.0 || options.pdf_quality > 1.0) {
                err_usage();
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            exit(0);
        } else {
            if (options.path) {
                err_usage();
            }
            options.path = argv[i];
        }
    }

    if (!options.to_pdf_name && options.path) {
        buff[0] = path_cpy[0] = 0;
        strcat(path_cpy, options.path);
        ext_point = strrchr(path_cpy, '.');
        if (ext_point) {
            *ext_point = 0;
        }
        sprintf(buff, "%s.pdf", basename(path_cpy));
        options.to_pdf_name = strdup(buff);
    }
}

pres_t        pres;
const char   *pres_path;
SDL_Renderer *sdl_ren;
SDL_Window   *sdl_win;
SDL_Texture  *sdl_tex;
int           reloading;
int           show_grid;
u32           start_ms;

void do_pdf_export(void);
void do_present(void);
void handle_input(int *quit, int *reloading, int *show_grid, int *winch);

int  init_video(void);
void fini_video(void);

void draw_simple_string_at(int x, int y, const char *str, font_cache_t *font);
void draw_time(float time);

static void update_window_resolution(pres_t *pres) {
    SDL_RenderSetLogicalSize(sdl_ren, pres->w, pres->h);
}

void reload_pres(pres_t *pres, const char *path) {
    free_presentation(pres);
    *pres = build_presentation(path, sdl_ren);
    update_window_resolution(pres);
    printf("reloaded '%s'\n", path);
}

static void handle_hup(int sig) {
    reloading = 1;
}

static void register_hup_handler(void) {
    struct sigaction sa;

    // Setup the sighup handler
    sa.sa_handler = &handle_hup;

    // Restart the system call, if at all possible
    sa.sa_flags = SA_RESTART;

    // Block every signal during the handler
    sigfillset(&sa.sa_mask);

    sigaction(SIGHUP, &sa, NULL);
}

int main(int argc, char **argv) {
    sdl_ren = NULL;

    memset(&options, 0, sizeof(options));
    get_env_options();
    parse_options(argc, argv);
    pres_path = options.path;

    if (!pres_path) { err_usage(); }

    start_ms = SDL_GetTicks();

    printf("pid = %d\n", getpid());

    TIME_ON(init_video) {
        init_video();
    } TIME_OFF(init_video);

    TIME_ON(init_font) {
        init_font();
    } TIME_OFF(init_font);

    TIME_ON(build_presentation) {
        pres = build_presentation(pres_path, sdl_ren);
        update_window_resolution(&pres);
        SDL_SetWindowSize(sdl_win, pres.w, pres.h);
    } TIME_OFF(build_presentation);

    if (options.to_pdf) {
        do_pdf_export();
    } else {
        register_hup_handler();
        do_present();
    }

    fini_video();

    return 0;
}

void do_pdf_export(void) {
    SDL_ShowWindow(sdl_win);

    export_to_pdf(sdl_ren, &pres, options.to_pdf_name, options.pdf_quality);
}

static void draw_grid(void) {
    int      unit;
    int      i, x, y;
    SDL_Rect r;

#define N_GRID_SQUARES (32)

    SDL_SetRenderDrawBlendMode(sdl_ren, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(pres.sdl_ren,
                           (~pres.r) & 0xFF,
                           (~pres.g) & 0xFF,
                           (~pres.b) & 0xFF,
                           64);

    unit = MAX(pres.w, pres.h) / N_GRID_SQUARES;

    r.w = 4;
    for (i = 1; i < pres.w / unit; i += 1) {
        x = i * unit;

        r.x = x;
        r.y = 0;
        r.h = pres.h;

        SDL_RenderFillRect(sdl_ren, &r);
    }

    r.h = 4;
    for (i = 1; i < pres.h / unit; i += 1) {
        y = i * unit;

        r.x = 0;
        r.y = y;
        r.w = pres.w;

        SDL_RenderFillRect(sdl_ren, &r);
    }
}

void do_present(void) {
    int             quit;
    u32             frame_start_ms, frame_elapsed_ms;
    u64             frame;
    float           last_frame_time;
    int             save_point;
    int             should_draw;
    int             sleep_ms;
    int             winch;
    int             was_animating;

    quit          = 0;
    frame         = 0;
    save_point    = 0;
    winch         = 0;
    was_animating = 0;

    while (!quit) {
        frame_start_ms = SDL_GetTicks();

        if (reloading) {
            save_point = pres.point;
            reload_pres(&pres, pres_path);
        }

        handle_input(&quit, &reloading, &show_grid, &winch);

        should_draw   =    (frame <= DISPLAY_DELAY_FRAMES + 1)
                        || was_animating
                        || pres.is_animating
                        || pres.movement_started
                        || reloading
                        || winch
                        || (frame % NON_ANIM_DRAW_INTERVAL == 0);
        winch         =    0;

        was_animating = pres.is_animating;

        if (should_draw) {
            draw_presentation(&pres);
        }

        update_presentation(&pres);

        if (reloading) {
            pres_restore_point(&pres, save_point);
        }

        if (frame == DISPLAY_DELAY_FRAMES) {
            printf("time to first draw: %ums\n", SDL_GetTicks() - start_ms);
            SDL_ShowWindow(sdl_win);
        }

        if (reloading) {
            pres_clear_and_draw_bg(&pres);
        }

        reloading = 0;

        (void)last_frame_time;
/*         draw_time(last_frame_time); */

        if (should_draw) {
            if (show_grid) {
                draw_grid();
            }

            SDL_RenderPresent(sdl_ren);
            SDL_Delay(0);
        }

        frame_elapsed_ms  = SDL_GetTicks() - frame_start_ms;
        frame            += 1;

        last_frame_time   = (float)frame_elapsed_ms;

        if (!should_draw && !was_animating) {
            sleep_ms = FPS_CAP_MS - frame_elapsed_ms;
            if (sleep_ms > 0) {
                SDL_Delay(sleep_ms);
            }
        }
    }

}

void handle_input(int *quit, int *reloading, int *show_grid, int *winch) {
    SDL_Event    e;
    const Uint8 *key_state;

    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            *quit = 1;
        } else if (e.type == SDL_KEYUP) {
            key_state = SDL_GetKeyboardState(NULL);

            if (!key_state[SDL_SCANCODE_LSHIFT]
            &&  !key_state[SDL_SCANCODE_RSHIFT]) {
                SDL_ShowCursor(SDL_DISABLE);
            }
        } else if (e.type == SDL_KEYDOWN) {
            key_state = SDL_GetKeyboardState(NULL);

            if (key_state[SDL_SCANCODE_LSHIFT]
            ||  key_state[SDL_SCANCODE_RSHIFT]) {
                SDL_ShowCursor(SDL_ENABLE);
            }

            if (!*reloading
            &&     (key_state[SDL_SCANCODE_LCTRL]
                ||  key_state[SDL_SCANCODE_RCTRL])
            &&  key_state[SDL_SCANCODE_R]) {
                *reloading = 1;
            } else if ((key_state[SDL_SCANCODE_LCTRL] || key_state[SDL_SCANCODE_RCTRL])
            && key_state[SDL_SCANCODE_L]) {
                *show_grid = !*show_grid;
            } else if (key_state[SDL_SCANCODE_Q]) {
                *quit = 1;
            } else if (!pres.is_animating && !pres.movement_started) {
                if (key_state[SDL_SCANCODE_J]) {
                    pres_next_point(&pres);
                } else if (key_state[SDL_SCANCODE_K]) {
                    pres_prev_point(&pres);
                } else if (key_state[SDL_SCANCODE_G]) {
                    if (key_state[SDL_SCANCODE_LSHIFT]
                    ||  key_state[SDL_SCANCODE_RSHIFT]) {
                        pres_last_point(&pres);
                    } else {
                        pres_first_point(&pres);
                    }
                }
            }
        } else if (e.type == SDL_WINDOWEVENT) {
            *winch = 1;
        }
    }
}

int init_video(void) {
    int render_flags;

    render_flags = SDL_RENDERER_ACCELERATED;

    if (options.renderer == 0) {
        render_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    } else if (options.renderer == 1) {
        render_flags = SDL_RENDERER_SOFTWARE;
    }

    if (options.to_pdf) {
        render_flags &= ~(SDL_RENDERER_ACCELERATED);
        render_flags &= ~(SDL_RENDERER_PRESENTVSYNC);
        render_flags |= SDL_RENDERER_SOFTWARE;
        render_flags |= SDL_RENDERER_TARGETTEXTURE;
    }


    TIME_ON(sdl_init_video) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
            ERR("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        }
    } TIME_OFF(sdl_init_video);

    TIME_ON(sdl_create_window) {
        sdl_win = SDL_CreateWindow("slide",
                                    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                    DEFAULT_RES_W, DEFAULT_RES_H,
                                    SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
        if (sdl_win == NULL) {
            ERR("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        }
    } TIME_OFF(sdl_create_window);

    SDL_ShowCursor(SDL_DISABLE);
/*     SDL_SetWindowOpacity(sdl_win, 0.9); */

    TIME_ON(sdl_create_renderer) {
        sdl_ren = SDL_CreateRenderer(sdl_win, -1, render_flags);
    } TIME_OFF(sdl_create_renderer);

    /*
     * If exporting to PDF, we will create the texture later since
     * we'll need to know the resolution of the presentation first.
     */

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
    SDL_RenderSetLogicalSize(sdl_ren, DEFAULT_RES_W, DEFAULT_RES_H);

    return 1;
}

void fini_video(void) {
    SDL_DestroyRenderer(sdl_ren);
    SDL_DestroyWindow(sdl_win);
    SDL_Quit();
}


void draw_simple_string_at(int x, int y, const char *str, font_cache_t *font) {
    int            len;
    int            i;
    font_entry_t  *entry;
    SDL_Rect       srect,
                   drect;
    char_code_t    code;
    int            n_bytes;

    if (!str) { return; }

    len = strlen(str);

    for (i = 0; i < len;) {
        code  = get_char_code(str + i, &n_bytes);
        entry = get_glyph(font, code, sdl_ren);

        srect.x = entry->x;
        srect.y = entry->y;
        srect.w = entry->w;
        srect.h = entry->h;

        drect.x = x + entry->adjust_x;
        drect.y = y - entry->adjust_y;
        drect.w = entry->w;
        drect.h = entry->h;

        SDL_RenderCopy(sdl_ren, entry->texture, &srect, &drect);

        x += entry->pen_advance_x;
        y += entry->pen_advance_y;

        i += n_bytes;
    }
}

void draw_time(float time) {
    char          buff[32];
    font_cache_t *font;
    int           size;

    size = 20;

    sprintf(buff, "%.1fms", time);

    font = get_or_load_font("fonts/luximr.ttf", size, sdl_ren);
    set_font_color(font, 255, 0, 255);
    draw_simple_string_at(0, pres.h - size, buff, font);
}
