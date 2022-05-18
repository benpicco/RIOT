/*
 * Copyright (C) 2022 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup net_gnrc_pktbuf_heap
 * @{
 *
 * @file
 *
 * @author  Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>

#include "mutex.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/nettype.h"
#include "net/gnrc/pkt.h"

#include "pktbuf_internal.h"

#define ENABLE_DEBUG 0
#include "debug.h"

enum {
    TYPE_NODE,
    TYPE_LEAF,
};

typedef struct _node _node_t;

/* The static buffer needs to be aligned to word size, so that its start
 * address can be casted to `_unused_t *` safely. Just allocating an array of
 * (word sized) uintptr_t is a trivial way to do this */
static uintptr_t _pktbuf_buf[CONFIG_GNRC_PKTBUF_SIZE / sizeof(uintptr_t)];

struct _node {
    union {
        struct {
            uint8_t *start;
            size_t len;
        } leaf;
        struct {
            uint8_t *start;
            size_t len;
            _node_t *l;
            _node_t *r;
        } node;
    } data;
    uint8_t type;
} _head = {
    .data = {
        .leaf = {
            .start = (void *)_pktbuf_buf,
            .len = sizeof(_pktbuf_buf),
        },
    },
    .type = TYPE_LEAF,
};

static void *_alloc(_node_t *node, size_t len)
{
    if (node == NULL) {
        return NULL;
    }

    if (node->type == TYPE_LEAF) {
        if (node->data.leaf.len < len) {
            return NULL;
        }

        /* spot found */
        node->data.leaf.len -= len;
        /* TODO: handle spot now being empty */
        return node->data.leaf.start + node->data.leaf.len;
    }

    if (node->type == TYPE_NODE) {
        void *mem = _alloc(node->data.node.l, len);
        if (mem == NULL) {
            mem = _alloc(node->data.node.r, len);
        }
        return mem;
    }

    assert(0);
    return NULL;
}

static _node_t *_search_at_end(_node_t *node, _node_t **parent, uint8_t *ptr, size_t len)
{
    if (node == NULL || node->type == TYPE_NODE) {
        return node;
    }

    _node_t *p_l = node;
    _node_t *l = _search_at_end(node->data.node.l, &p_l, ptr, len);

    _node_t *p_r = node;
    _node_t *r = _search_at_end(node->data.node.r, &p_r, ptr, len);

    if (l == NULL) {
        *parent = p_r;
        return r;
    }

    if (r == NULL) {
        *parent = p_l;
        return l;
    }

    assert(r->type == TYPE_NODE);
    assert(l->type == TYPE_NODE);

    if (r->data.leaf.start + r->data.leaf.len <= ptr) {
        *parent = p_r;
        return r;
    }

    if (l->data.leaf.start + l->data.leaf.len <= ptr) {
        *parent = p_l;
        return l;
    }

    return NULL;
}

static _node_t *_search_at_start(_node_t *node, _node_t **parent, uint8_t *ptr, size_t len)
{
    if (node == NULL || node->type == TYPE_NODE) {
        return node;
    }

    _node_t *p_l = node;
    _node_t *l = _search_at_start(node->data.node.l, &p_l, ptr, len);

    _node_t *p_r = node;
    _node_t *r = _search_at_start(node->data.node.r, &p_r, ptr, len);

    if (l == NULL) {
        *parent = p_r;
        return r;
    }

    if (r == NULL) {
        *parent = p_l;
        return l;
    }

    assert(r->type == TYPE_NODE);
    assert(l->type == TYPE_NODE);

    if (ptr + len <= l->data.leaf.start) {
        *parent = p_l;
        return l;
    }

    if (ptr + len <= r->data.leaf.start) {
        *parent = p_r;
        return r;
    }

    return NULL;
}

static void _free_chunk(uint8_t *ptr, size_t len)
{
    size_t diff_start = SIZE_MAX;
    size_t diff_end = SIZE_MAX;

    _node_t *p_s = NULL;
    _node_t *s = _search_at_start(&_head, &p_s, ptr, len);

    _node_t *p_e = NULL;
    _node_t *e = _search_at_end(&_head, &p_e, ptr, len);

    if (s) {
        diff_start = s->data.leaf.start - (ptr + len);
    }

    if (e) {
        diff_end = ptr - (e->data.leaf.start + e->data.leaf.len);
    }

    if (diff_start == 0 && diff_end == 0) {
        /* TODO: merge chunks */
    }

    if (diff_start == 0) {
        s->data.leaf.start = ptr;
        s->data.leaf.len += len;
        return;
    }

    if (diff_end == 0) {
        e->data.leaf.len += len;
        return;
    }

    if (diff_start < diff_end) {
    } else {
    }
}

void gnrc_pktbuf_init(void)
{
}

gnrc_pktsnip_t *gnrc_pktbuf_add(gnrc_pktsnip_t *next, const void *data, size_t size,
                                gnrc_nettype_t type)
{
}

gnrc_pktsnip_t *gnrc_pktbuf_mark(gnrc_pktsnip_t *pkt, size_t size, gnrc_nettype_t type)
{
    return marked_snip;
}

int gnrc_pktbuf_realloc_data(gnrc_pktsnip_t *pkt, size_t size)
{
    return 0;
}

void gnrc_pktbuf_hold(gnrc_pktsnip_t *pkt, unsigned int num)
{
    mutex_lock(&gnrc_pktbuf_mutex);
    while (pkt) {
        pkt->users += num;
        pkt = pkt->next;
    }
    mutex_unlock(&gnrc_pktbuf_mutex);
}

gnrc_pktsnip_t *gnrc_pktbuf_start_write(gnrc_pktsnip_t *pkt)
{
    /* TODO: copy if users > 1 */

    return pkt;
}

void gnrc_pktbuf_stats(void)
{
}
#endif

#ifdef TEST_SUITES
bool gnrc_pktbuf_is_empty(void)
{
}

bool gnrc_pktbuf_is_sane(void)
{
}
#endif

void gnrc_pktbuf_free_internal(void *data, size_t size)
{
}

/** @} */
