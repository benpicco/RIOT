/*
 * Copyright (C) 2022 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 *
 * @author  Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */

#include <stdio.h>
#include <string.h>

#include "kernel_defines.h"
#include "lru_list.h"

static lru_list_entry_t *_find(lru_list_t *head, const void *needle,
                               lru_list_entry_t **free)
{
    for (lru_list_entry_t *entry = head->head; entry != NULL; entry = entry->next) {
        if (free) {
            *free = entry;
        }

        if (!entry->used) {
            return NULL;
        }

        if (head->is_equal(entry, needle)) {
            return entry;
        }
    }

    return NULL;
}

static void _promote(lru_list_t *head, lru_list_entry_t *entry)
{
    if (entry->prev == NULL) {
        /* already fist node */
        return;
    }

    entry->prev->next = entry->next;
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    entry->prev = NULL;

    entry->next = head->head;
    head->head->prev = entry;
    head->head = entry;
}

static void _remove(lru_list_t *head, lru_list_entry_t *entry)
{
    if (entry == head->head) {
        head->head = entry->next;
    }

    while (entry->next && entry->next->used) {
        lru_list_entry_t *prev = entry->prev;
        lru_list_entry_t *next = entry->next;

        if (prev) {
            prev->next = next;
        }

        entry->next = next->next;
        entry->prev = next;

        next->prev = prev;
        next->next = entry;
    }

    entry->used = false;
}

lru_list_entry_t * lru_list_insert(lru_list_t *head, const void *needle)
{
    lru_list_entry_t *free;
    lru_list_entry_t *entry = _find(head, needle, &free);

    if (entry == NULL) {
        entry = free;

        if (entry->used && head->remove) {
            head->remove(entry);
        }
    }

    entry->used = true;
    _promote(head, entry);

    return entry;
}

lru_list_entry_t * lru_list_find(lru_list_t *head, const void *needle)
{
    lru_list_entry_t *entry = _find(head, needle, NULL);

    if (entry != NULL) {
        _promote(head, entry);
    }

    return entry;
}

bool lru_list_remove(lru_list_t *head, const void *needle)
{
    lru_list_entry_t *entry = _find(head, needle, NULL);

    if (entry == NULL) {
        return false;
    }

    _remove(head, entry);
    if (head->remove) {
        head->remove(entry);
    }

    return true;
}

void lru_list_init(lru_list_t *head, void *buffer, size_t nmemb, size_t size,
                   lru_list_cb_is_equal_t is_equal, lru_list_cb_remove_t remove)
{
    uint8_t *ptr = buffer;
    head->head = buffer;
    head->is_equal = is_equal;
    head->remove = remove;

    memset(buffer, 0, size * nmemb);

    lru_list_entry_t *prev = head->head;
    for (unsigned i = 1; i < nmemb; ++i) {
        lru_list_entry_t *node = (void *)&ptr[i * size];
        prev->next = node;
        node->prev = prev;
        prev = node;
    }

    prev->next = NULL;
}
/** @} */
