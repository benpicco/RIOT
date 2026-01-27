/*
 * Copyright 2019 Marian Buschsieweke
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_ws281x
 *
 * @{
 *
 * @file
 * @brief       Implementation of `ws281x_write()` for ATmega MCUs
 *
 * @author      Marian Buschsieweke <marian.buschsieweke@ovgu.de>
 *
 * @}
 */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "ws281x.h"
#include "ws281x_params.h"
#include "ws281x_constants.h"

#include <time.h>
#include <SDL2/SDL.h>

void ws281x_write_buffer(ws281x_t *dev, const void *buf, size_t size)
{
    (void) dev;
    (void) buf;
    (void) size;
}

void ws281x_end_transmission(ws281x_t *dev)
{
    SDL_UnlockTexture(dev->texture);
    SDL_RenderCopy(dev->renderer, dev->texture, NULL, NULL);
    SDL_LockTexture(dev->texture, NULL, &dev->pixels, &dev->pitch);
    SDL_RenderPresent(dev->renderer);
}

int ws281x_init_backend(ws281x_t *dev)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        return -ENODEV;
    }

    /* Quit SDL on exit */
//    atexit(SDL_Quit);
    puts("SDL init done.");

    SDL_Window *window = SDL_CreateWindow("ws281x", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 64, 64, 0);
    dev->renderer = SDL_CreateRenderer(window, -1, 0);
    dev->texture = SDL_CreateTexture(dev->renderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_STREAMING, 8, 8);
    SDL_LockTexture(dev->texture, NULL, &dev->pixels, &dev->pitch);

    return 0;
}
