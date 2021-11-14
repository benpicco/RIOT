/*
 * Copyright (C) 2021 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    sys_chunk_buffer    chunked Ringbuffer
 * @ingroup     sys
 * @brief       Implementation of a Ringbuffer to store chunks of data
 * @{
 *
 * @file
 * @brief   Chunked Ringbuffer
 *
 * @author  Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */

#ifndef CHUNKED_RINGBUFFER_H
#define CHUNKED_RINGBUFFER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CHUNK_NUM_MAX
#define CHUNK_NUM_MAX   (3)
#endif

typedef struct {
    uint8_t *buffer;
    uint8_t *buffer_end;
    uint8_t *cur;
    uint8_t *cur_start;
    uint8_t *protect;

    uint8_t *chunk_start[CHUNK_NUM_MAX];
    uint16_t chunk_len[CHUNK_NUM_MAX];
    uint8_t chunk_cur;
} chunk_ringbuf_t;

typedef void (*crb_byte_callback_t)(void *ctx, uint8_t byte);

static inline bool crb_start_chunk(chunk_ringbuf_t *rb)
{
    /* pointing to the start of the first chunk */
    if (rb->cur == rb->protect) {
        return false;
    }

    rb->cur_start = rb->cur;

    if (rb->protect == NULL) {
        rb->protect = rb->cur_start;
    }

    return true;
}

static inline bool crb_add_byte(chunk_ringbuf_t *rb, uint8_t b)
{
    /* if this is the first chunk, protect will be at start */
    if (rb->cur == rb->protect &&
        rb->cur != rb->cur_start) {
        return false;
    }

    *rb->cur = b;

    /* handle wrap around */
    if (rb->cur == rb->buffer_end) {
        rb->cur = rb->buffer;
    } else {
        ++rb->cur;
    }

    return true;
}

bool crb_add_bytes(chunk_ringbuf_t *rb, const void *data, size_t len);

bool crb_add_chunk(chunk_ringbuf_t *rb, const void *data, size_t len);

bool crb_end_chunk(chunk_ringbuf_t *rb, bool valid);

bool crb_get_chunk_size(chunk_ringbuf_t *rb, size_t *len);

bool crb_peek_bytes(chunk_ringbuf_t *rb, void *dst, size_t offset, size_t len);

bool crb_consume_chunk(chunk_ringbuf_t *rb, void *dst, size_t len);

bool crb_chunk_foreach(chunk_ringbuf_t *rb, crb_byte_callback_t func, void *ctx);

void crb_init(chunk_ringbuf_t *rb, void *buffer, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CHUNKED_RINGBUFFER_H */
/** @} */
