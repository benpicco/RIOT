/*
 * Copyright (C) 2022 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    sys_lru_list    Least Recently Used list
 * @ingroup     sys
 * @brief       A list sorted by least recently used
 * @{
 *
 * @file
 * @brief   Least Recently Used list
 *
 * @author  Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */

#ifndef LRU_LIST_H
#define LRU_LIST_H

#include <stdbool.h>

#include "kernel_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 *
 * @param[in] head
 * @param[in] array
 */
#define LRU_LIST_INIT(head, array, is_equal, remove) lru_list_init(head, array,         \
                                                                   ARRAY_SIZE(array),   \
                                                                   sizeof(array[0]),    \
                                                                   is_equal, remove)

typedef struct _lru_list_entry {
    struct _lru_list_entry *next;
    struct _lru_list_entry *prev;
    bool used;
} lru_list_entry_t;

typedef struct {
    lru_list_entry_t *head;
    bool (*is_equal)(lru_list_entry_t *node, const void *needle);
    void (*remove)(lru_list_entry_t *node);
} lru_list_t;

typedef bool (*lru_list_cb_is_equal_t)(lru_list_entry_t *node, const void *needle);
typedef void (*lru_list_cb_remove_t)(lru_list_entry_t *node);

void lru_list_init(lru_list_t *head, void *buffer, size_t nmemb, size_t size,
                   lru_list_cb_is_equal_t is_equal, lru_list_cb_remove_t remove);

lru_list_entry_t * lru_list_insert(lru_list_t *head, const void *needle);

lru_list_entry_t * lru_list_find(lru_list_t *head, const void *needle);

bool lru_list_remove(lru_list_t *head, const void *needle);

#ifdef __cplusplus
}
#endif

#endif /* LRU_LIST_H */
/** @} */
