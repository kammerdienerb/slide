#include "internal.h"
#include "font.h"
#include "presentation.h"


#define FPS_TO_MS(fps)   ((1000) * (1.0 / (float)(fps)))
#define FPS_CAP_MS       ((u64)(FPS_TO_MS(60)))


static int    draw_x, draw_y;
static int    view_x, view_y;
static u32    point;
static u32    n_points;
pres_t        pres;
const char   *pres_path;
SDL_Renderer *sdl_ren;

int  init_video(SDL_Window **sdl_win);
void fini_video(SDL_Window **sdl_win);

void draw_string(const char *str, int l_margin, int r_margin, int justification, font_cache_t *font);

void reload_pres(pres_t *pres, const char *path) {
    free_presentation(pres);
    *pres = build_presentation(path, sdl_ren);
}

static void handle_hup(int sig) {
    reload_pres(&pres, pres_path);
}

static void register_hup_handler(void) {
    struct sigaction sa;

    // Setup the sighub handler
    sa.sa_handler = &handle_hup;

    // Restart the system call, if at all possible
    sa.sa_flags = SA_RESTART;

    // Block every signal during the handler
    sigfillset(&sa.sa_mask);

    sigaction(SIGHUP, &sa, NULL);
}

static void clear_and_draw_bg(pres_t *pres) {
    SDL_Rect r;
    int      sum;

    sum = pres->r + pres->g + pres->g;
    if (sum > ((3 * 255) / 2)) {
        SDL_SetRenderDrawColor(sdl_ren, 0, 0, 0, 255);
    } else {
        SDL_SetRenderDrawColor(sdl_ren, 255, 255, 255, 255);
    }
    SDL_RenderClear(sdl_ren);

    r.x = r.y = 0;
    r.w = SCREEN_WIDTH;
    r.h = SCREEN_HEIGHT;

    SDL_SetRenderDrawColor(sdl_ren, pres->r, pres->g, pres->b, 255);
    SDL_RenderFillRect(sdl_ren, &r);

    SDL_SetRenderDrawColor(sdl_ren, 255 - pres->r, 255 - pres->g, 255 - pres->b, 255);
}

int main(int argc, char **argv) {
    SDL_Window     *sdl_win;
    font_cache_t   *font;
    pres_elem_t    *elem;
    SDL_Event       e;
    int             quit;
    u32             frame_start_ms, frame_elapsed_ms;
    u64             frame;
    const Uint8    *key_state;
    int             save_points[2048];
    int             dst_view_y;
    int             is_animating;
    int             save_draw_x, save_draw_y, new_l_margin;
    SDL_Rect        drect;
    SDL_Texture    *image_texture;
    int             translating;

    sdl_ren = NULL;

    if (argc != 2) {
        ERR("usage: %s [FILE]\n", argv[0]);
    }

    TIME_ON(init_video) {
        init_video(&sdl_win);
    } TIME_OFF(init_video);

    TIME_ON(init_font) {
        init_font();
    } TIME_OFF(init_font);

    TIME_ON(build_presentation) {
        pres_path = argv[1];
        pres = build_presentation(pres_path, sdl_ren);
    } TIME_OFF(build_presentation);


    register_hup_handler();

    quit  = 0;
    frame = 0;

    view_x = view_y = 0;

    is_animating = 0;
    translating  = 0;

    while (!quit) {
        frame_start_ms = SDL_GetTicks();

        clear_and_draw_bg(&pres);

        draw_x   = view_x;
        draw_y   = view_y;
        font     = NULL;
        n_points = 0;
        array_traverse(pres.elements, elem) {
            if (elem->kind == PRES_PARA) {
                font = get_or_load_font(pres_get_font_name_by_id(&pres, elem->font_id), elem->font_size, sdl_ren);
                set_font_color(font, elem->r, elem->g, elem->b);
                draw_string(array_data(elem->text), elem->l_margin, elem->r_margin, elem->justification, font);
                translating = 0;
            } else if (elem->kind == PRES_BULLET) {
                font = get_or_load_font(pres_get_font_name_by_id(&pres, elem->font_id), elem->font_size, sdl_ren);
                set_font_color(font, elem->r, elem->g, elem->b);

                save_draw_x = draw_x;
                save_draw_y = draw_y;

                new_l_margin = elem->l_margin + ((0.05 * (elem->level - 1)) * SCREEN_WIDTH);
                draw_string(pres.bullet_strings[elem->level - 1], new_l_margin, elem->r_margin, JUST_L, font);

                new_l_margin  = draw_x - save_draw_x;
                draw_x        = save_draw_x;
                draw_y        = save_draw_y;

                draw_string(array_data(elem->text), new_l_margin, elem->r_margin, elem->justification, font);
                translating = 0;
            } else if (elem->kind == PRES_BREAK) {
                if (font) {
                    draw_y += 1.25 * font->line_height;
                }
                translating = 0;
            } else if (elem->kind == PRES_VSPACE) {
                draw_y += elem->y;
                translating = 0;
            } else if (elem->kind == PRES_VFILL) {
                draw_y += SCREEN_HEIGHT - ((draw_y - view_y) % SCREEN_HEIGHT);
                translating = 0;
            } else if (elem->kind == PRES_POINT) {
                save_points[n_points] = -(draw_y - view_y);
                n_points += 1;
            } else if (elem->kind == PRES_IMAGE) {
                image_texture = pres_get_image_texture(&pres, elem->image);
                drect.x = draw_x;
                drect.y = draw_y;
                drect.w = elem->w;
                drect.h = elem->h;
                SDL_RenderCopy(sdl_ren, image_texture, NULL, &drect);
                draw_y += drect.h;
                translating = 0;
            } else if (elem->kind == PRES_TRANSLATE) {
                translating  = 1;
                draw_x      += elem->x;
                draw_y      += elem->y;
            }

            if (!translating) {
                draw_x = 0;
            }
        }

        if (is_animating) {
            if (view_y < dst_view_y) {
                view_y += pres.speed * FPS_CAP_MS;
                if (view_y > dst_view_y) {
                    view_y = dst_view_y;
                }
            } else if (view_y > dst_view_y) {
                view_y -= pres.speed * FPS_CAP_MS;
                if (view_y < dst_view_y) {
                    view_y = dst_view_y;
                }
            }

            if (view_y == dst_view_y) {
                is_animating = 0;
            }
        } else {
            if (n_points != 0) {
                if (point >= n_points) {
                    point = n_points - 1;
                }

                if (view_y != save_points[point]) {
                    dst_view_y   = save_points[point];
                    is_animating = 1;
                }
            }
        }

        SDL_RenderPresent(sdl_ren);

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            } if (e.type == SDL_KEYUP) {
                key_state = SDL_GetKeyboardState(NULL);
                if (   (key_state[SDL_SCANCODE_LCTRL]
                    ||  key_state[SDL_SCANCODE_RCTRL])
                &&  key_state[SDL_SCANCODE_R]) {
                    reload_pres(&pres, argv[1]);
                }
            } else if (e.type == SDL_KEYDOWN) {
                key_state = SDL_GetKeyboardState(NULL);
                if (key_state[SDL_SCANCODE_J]) {
                    if (!is_animating) {
                        point += 1;
                    }
                } else if (key_state[SDL_SCANCODE_K]) {
                    if (!is_animating) {
                        if (point > 0) {
                            point -= 1;
                        }
                    }
                }
            }
        }

        if (!quit) {
            frame_elapsed_ms = SDL_GetTicks() - frame_start_ms;

            if (frame_elapsed_ms < FPS_CAP_MS) {
                SDL_Delay(FPS_CAP_MS - frame_elapsed_ms);
            }

            frame += 1;
        }
    }

    fini_video(&sdl_win);

    return 0;
}

int init_video(SDL_Window **sdl_win) {

    TIME_ON(sdl_init_video) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
            ERR("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        }
    } TIME_OFF(sdl_init_video);


    TIME_ON(sdl_create_window) {
        *sdl_win = SDL_CreateWindow("slide",
                                    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                    SCREEN_WIDTH, SCREEN_HEIGHT,
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
        if (sdl_win == NULL) {
            ERR("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        }
    } TIME_OFF(sdl_create_window);


    TIME_ON(sdl_create_renderer) {
        sdl_ren = SDL_CreateRenderer(*sdl_win, -1, SDL_RENDERER_ACCELERATED);
    } TIME_OFF(sdl_create_renderer);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
    SDL_RenderSetLogicalSize(sdl_ren, SCREEN_WIDTH, SCREEN_HEIGHT);


/*     SDL_SetWindowOpacity(*sdl_win, 0.9); */

    return 1;
}

void fini_video(SDL_Window **sdl_win) {
    SDL_DestroyRenderer(sdl_ren);
    SDL_DestroyWindow(*sdl_win);
    SDL_Quit();
}

static array_t get_wrap_points(const unsigned char *bytes, font_cache_t *font, int l_margin, int r_margin, array_t *line_widths) {
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

    code         = get_char_code(" ", &n_bytes_i);
    entry        = get_glyph(font, code, sdl_ren);
    space_width  = entry->pen_advance_x;

    len = strlen((const char *)bytes);

    wrap_points  = array_make(int);
    *line_widths = array_make(int);
    total_width  = l_margin;
    last_space   = -1;

    for (i = 0; i < len;) {
        code  = get_char_code(((const char *)bytes) + i, &n_bytes_i);
        entry = get_glyph(font, code, sdl_ren);

        total_width += entry->pen_advance_x;

        if (total_width > SCREEN_WIDTH - r_margin) {
            line_width  = total_width;
            total_width = l_margin;

            if (last_space != -1) {
                for (j = last_space + 1; j <= i;) {
                    code         = get_char_code(((const char *)bytes) + j, &n_bytes_j);
                    entry        = get_glyph(font, code, sdl_ren);
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

    if (total_width > SCREEN_WIDTH - r_margin) {
        line_width  = total_width;
        total_width = l_margin;

        if (last_space != -1) {
            for (j = last_space + 1; j <= i; j += 1) {
                entry        = get_glyph(font, bytes[j], sdl_ren);
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

void draw_string(const char *str, int l_margin, int r_margin, int justification, font_cache_t *font) {
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

    _x = draw_x + l_margin;
    _y = draw_y;

    (void)_y;


    draw_x  = _x;
    draw_y += font->line_height;

    if (!str) { return; }

    bytes = (unsigned char*)str;

    len = strlen(str);

    wrap_points = get_wrap_points(bytes, font, l_margin, r_margin, &line_widths);
    line        = 0;

    switch (justification) {
        case JUST_L: break;
        case JUST_R:
            draw_x += (SCREEN_WIDTH - l_margin - r_margin) - *(int*)array_item(line_widths, line);
            break;
        case JUST_C:
            draw_x += ((SCREEN_WIDTH - l_margin - r_margin) - *(int*)array_item(line_widths, line)) / 2;
            break;
    }

    for (i = 0; i < len;) {
        wrapped = 0;

        code  = get_char_code(str + i, &n_bytes);
        entry = get_glyph(font, code, sdl_ren);

        srect.x = entry->x;
        srect.y = entry->y;
        srect.w = entry->w;
        srect.h = entry->h;

        drect.x = draw_x + entry->adjust_x;
        drect.y = draw_y - entry->adjust_y;
        drect.w = entry->w;
        drect.h = entry->h;

        array_traverse(wrap_points, wrap_it) {
            if (*wrap_it == i) {
                line   += 1;
                draw_y += 1.25 * font->line_height;
                draw_x  = _x;

                switch (justification) {
                    case JUST_L: break;
                    case JUST_R:
                        draw_x += (SCREEN_WIDTH - l_margin - r_margin) - *(int*)array_item(line_widths, line);
                        break;
                    case JUST_C:
                        draw_x += ((SCREEN_WIDTH - l_margin - r_margin) - *(int*)array_item(line_widths, line)) / 2;
                        break;
                }

                wrapped = 1;
                break;
            }
        }

        SDL_RenderCopy(sdl_ren, entry->texture, &srect, &drect);

        if (!wrapped) {
            draw_x += entry->pen_advance_x;
        }
        draw_y += entry->pen_advance_y;

        i += n_bytes;
    }

    array_free(line_widths);
    array_free(wrap_points);
}
