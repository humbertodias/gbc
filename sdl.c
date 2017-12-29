#include <assert.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "gui.h"

static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioDeviceID audio_dev;

static int lcd_width, lcd_height;

/* Called by SDL when it needs more samples. */
void audio_callback(void *userdata, uint8_t *stream, int len) {
    uint8_t *sndbuf = userdata;
    memcpy(stream, sndbuf, len);
}

int gui_audio_init(int sample_rate, int channels, size_t sndbuf_size,
        uint8_t *sndbuf) {
    SDL_AudioSpec want, have;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        printf("SDL: failed to initialize sound: %s\n", SDL_GetError());
        return 1;
    }

    SDL_memset(&want, 0, sizeof(want));
    want.freq = sample_rate;
    want.format = AUDIO_U8;
    want.channels = channels;
    want.samples = sndbuf_size * channels;
    want.callback = audio_callback;
    want.userdata = sndbuf;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!audio_dev) {
        printf("SDL: failed to open sound device: %s\n", SDL_GetError());
        return 1;
    }
    SDL_PauseAudioDevice(audio_dev, 0);
    return 0;
}


int gui_lcd_init(int width, int height, int zoom, char *wintitle) {
    SDL_Window *window;

    if (SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL failed to initialize video: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(wintitle,
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            width * zoom, height * zoom, 0);
    if (!window){
        printf("SDL could not create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!window){
        printf("SDL could not create renderer: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture){
        printf("SDL could not create screen texture: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    lcd_width = width;
    lcd_height = height;


    return 0;
}


void gui_lcd_render_frame(char use_colors, uint16_t *pixbuf) {
    uint32_t *pixels = NULL;
    int pitch;
    if (SDL_LockTexture(texture, NULL, (void*)&pixels, &pitch)) {
        printf("SDL could not lock screen texture: %s\n", SDL_GetError());
        exit(1);
    }

    if (use_colors) {
        /* The colors stored in pixbuf are two byte each, 5 bits per rgb
         * component: -bbbbbgg gggrrrrr. We need to extract these, scale these
         * values from 0-1f to 0-ff and put them in RGBA format. For the scaling
         * we'd have to multiply by 0xff/0x1f, which is 8.23, approx 8, which is
         * a shift by 3. */
        for (int y = 0; y < lcd_height; y++)
            for (int x = 0; x < lcd_width; x++) {
                int idx = x + y * lcd_width;
                uint16_t rawcol = pixbuf[idx];
                uint32_t r = ((rawcol >>  0) & 0x1f) << 3;
                uint32_t g = ((rawcol >>  5) & 0x1f) << 3;
                uint32_t b = ((rawcol >> 10) & 0x1f) << 3;
                uint32_t col = (r << 24) | (g << 16) | (b << 8) | 0xff;
                pixels[idx] = col;
            }
    } else {
        /* The colors stored in pixbuf already went through the palette
         * translation, but are still 2 bit monochrome. */
        uint32_t palette[] = { 0xffffffff, 0xaaaaaaaa, 0x66666666, 0x11111111 };
        for (int y = 0; y < lcd_height; y++)
            for (int x = 0; x < lcd_width; x++) {
                int idx = x + y * lcd_width;
                pixels[idx] = palette[pixbuf[idx]];
            }
    }

    SDL_UnlockTexture(texture);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    SDL_Delay(1000/120);
}

int gui_input_poll(struct player_input *input) {
    input->special_quit = 0;
    input->special_savestate = 0;
    input->special_dbgbreak = 0;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
            case SDLK_q:
                input->special_quit = 1;
                break;

            case SDLK_b:         input->special_dbgbreak = 1; break;
            case SDLK_s:         input->special_savestate = 1; break;

            case SDLK_RETURN:    input->button_start = 1; break;
            case SDLK_BACKSPACE: input->button_select = 1; break;
            case SDLK_x:         input->button_b = 1; break;
            case SDLK_z:         input->button_a = 1; break;
            case SDLK_DOWN:      input->button_down = 1; break;
            case SDLK_UP:        input->button_up = 1; break;
            case SDLK_LEFT:      input->button_left = 1; break;
            case SDLK_RIGHT:     input->button_right = 1; break;
            }
            break;

        case SDL_KEYUP:
            switch (event.key.keysym.sym) {
            case SDLK_RETURN:    input->button_start = 0; break;
            case SDLK_BACKSPACE: input->button_select = 0; break;
            case SDLK_x:         input->button_b = 0; break;
            case SDLK_z:         input->button_a = 0; break;
            case SDLK_DOWN:      input->button_down = 0; break;
            case SDLK_UP:        input->button_up = 0; break;
            case SDLK_LEFT:      input->button_left = 0; break;
            case SDLK_RIGHT:     input->button_right = 0; break;
            }
            break;

        case SDL_QUIT:
            input->special_quit = 1;
        }
    }
    return 1;
}
