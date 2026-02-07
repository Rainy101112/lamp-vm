#include <SDL2/SDL.h>
#include "display.h"
#include "../../io.h"
#include "../../interrupt.h"
#include "../../vm.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;

int vga_display_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return -1;

    window = SDL_CreateWindow(
        "VM Display", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, FB_WIDTH, FB_HEIGHT, 0);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, FB_WIDTH, FB_HEIGHT);

    SDL_StartTextInput();
    return 0;
}

static void serial_rx_push(VM *vm, uint8_t c) {
    vm_shared_lock(vm);
    if (vm->io[SCREEN_ATTRIBUTE] & SERIAL_STATUS_RX_READY) {
        vm_shared_unlock(vm);
        return;
    }
    vm->io[KEYBOARD] = c;
    vm->io[SCREEN_ATTRIBUTE] |= SERIAL_STATUS_RX_READY;
    if ((vm->io[SCREEN_ATTRIBUTE] >> 8) & SERIAL_CTRL_RX_INT_ENABLE) {
        trigger_interrupt(vm, INT_SERIAL);
    }
    vm_shared_unlock(vm);
}

void display_update(VM *vm) {
    //printf("first 16 pixels:");
    //for (int i = 0; i < 16; i++) {
    //    printf(" %08x", ((uint32_t *)vm->fb)[i]);
    //}
    //printf("\n");

    //printf("flushing texture, first pixel = %08x\n", ((uint32_t *)vm->fb)[0]);
    SDL_UpdateTexture(texture, NULL, vm->fb, FB_WIDTH * FB_BPP);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    //printf("flushed\n");
}

void display_poll_events(VM *vm) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                vm->halted = 1;
                break;
            case SDL_TEXTINPUT: {
                const char *p = e.text.text;
                while (*p != '\0') {
                    serial_rx_push(vm, (uint8_t)*p);
                    p++;
                }
                break;
            }
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_RETURN) {
                    serial_rx_push(vm, (uint8_t)'\n');
                } else if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    serial_rx_push(vm, (uint8_t)0x08);
                } else if (e.key.keysym.sym == SDLK_TAB) {
                    serial_rx_push(vm, (uint8_t)'\t');
                }
                break;
            default:
                break;
        }
    }
}

void display_shutdown() {
    SDL_StopTextInput();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
