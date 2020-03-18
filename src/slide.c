#include "internal.h"
#include "font.h"
#include "presentation.h"

pres_t        pres;
const char   *pres_path;
SDL_Renderer *sdl_ren;
SDL_Window   *sdl_win;
int           reloading;

int  init_video(void);
void fini_video(void);


static void update_window_resolution(pres_t *pres) {
    SDL_RenderSetLogicalSize(sdl_ren, pres->w, pres->h);
}

void reload_pres(pres_t *pres, const char *path) {
    free_presentation(pres);
    *pres = build_presentation(path, sdl_ren);
    update_window_resolution(pres);
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
    SDL_Event       e;
    int             quit;
    u32             frame_start_ms, frame_elapsed_ms;
    u64             frame;
    const Uint8    *key_state;
    int             save_point;

    sdl_ren = NULL;

    if (argc != 2) {
        ERR("usage: %s [FILE]\n", argv[0]);
    }

    TIME_ON(init_video) {
        init_video();
    } TIME_OFF(init_video);

    TIME_ON(init_font) {
        init_font();
    } TIME_OFF(init_font);

    TIME_ON(build_presentation) {
        pres_path = argv[1];
        pres = build_presentation(pres_path, sdl_ren);
        update_window_resolution(&pres);
        SDL_SetWindowSize(sdl_win, pres.w, pres.h);
    } TIME_OFF(build_presentation);

    register_hup_handler();

    quit       = 0;
    frame      = 0;
    save_point = 0;

    while (!quit) {
        frame_start_ms = SDL_GetTicks();

        if (reloading) {
            save_point = pres.point;
            reload_pres(&pres, pres_path);
        }

        draw_presentation(&pres);
        update_presentation(&pres);

        if (reloading) {
            pres_restore_point(&pres, save_point);
        }

        if (frame == DISPLAY_DELAY_FRAMES) {
            SDL_ShowWindow(sdl_win);
        }

        if (reloading) {
            pres_clear_and_draw_bg(&pres);
        }

        reloading = 0;

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
                if (!pres.is_animating) {
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

    fini_video();

    return 0;
}

int init_video(void) {

    TIME_ON(sdl_init_video) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
            ERR("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        }
    } TIME_OFF(sdl_init_video);


    TIME_ON(sdl_create_window) {
        sdl_win = SDL_CreateWindow("slide",
                                    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                    DEFAULT_RES_W, DEFAULT_RES_H,
                                    SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
        if (sdl_win == NULL) {
            ERR("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        }
    } TIME_OFF(sdl_create_window);


    TIME_ON(sdl_create_renderer) {
        sdl_ren = SDL_CreateRenderer(sdl_win, -1, SDL_RENDERER_ACCELERATED);
    } TIME_OFF(sdl_create_renderer);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
    SDL_RenderSetLogicalSize(sdl_ren, DEFAULT_RES_W, DEFAULT_RES_H);


/*     SDL_SetWindowOpacity(sdl_win, 0.9); */

    return 1;
}

void fini_video(void) {
    SDL_DestroyRenderer(sdl_ren);
    SDL_DestroyWindow(sdl_win);
    SDL_Quit();
}
