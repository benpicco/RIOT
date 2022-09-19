/*
 * Copyright (C) 2022 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       lru_list test application
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "lru_list.h"
#include "test_utils/expect.h"

typedef struct {
    lru_list_entry_t node;
    char name[32];
} test_entry_t;

static void _print_list(lru_list_t *head)
{
    puts("START LIST");

    for (lru_list_entry_t *entry = head->head; entry != NULL; entry = entry->next) {
        test_entry_t *node = container_of(entry, test_entry_t, node);
        if (node->node.used) {
            printf("%s\n", node->name);
        } else {
            puts("[empty]");
        }
    }

    puts("END LIST");
}

static bool _is_equal(lru_list_entry_t *a, const void *b)
{
    test_entry_t *node = container_of(a, test_entry_t, node);

    return strcmp(node->name, b) == 0;
}

static void _remove(lru_list_entry_t *a)
{
    test_entry_t *node = container_of(a, test_entry_t, node);
    printf("dropping '%s'\n", node->name);
}

static void test_entry_insert(lru_list_t *head, const char *name)
{
    lru_list_entry_t *entry = lru_list_insert(head, name);
    test_entry_t *node = container_of(entry, test_entry_t, node);

    strncpy(node->name, name, sizeof(node->name));
    printf("insert '%s'\n", node->name);
}

int main(void)
{
    lru_list_t lru_list;
    test_entry_t nodes[8];

    LRU_LIST_INIT(&lru_list, nodes, _is_equal, _remove);

    puts("[Test insert]");
    test_entry_insert(&lru_list, "A");
    test_entry_insert(&lru_list, "B");
    test_entry_insert(&lru_list, "C");
    test_entry_insert(&lru_list, "D");
    test_entry_insert(&lru_list, "E");
    test_entry_insert(&lru_list, "F");
    test_entry_insert(&lru_list, "G");
    test_entry_insert(&lru_list, "H");
    test_entry_insert(&lru_list, "I");
    test_entry_insert(&lru_list, "J");

    _print_list(&lru_list);

    puts("[Test remove]");
    expect(!lru_list_remove(&lru_list, "A"));
    expect(lru_list_remove(&lru_list, "J"));
    expect(lru_list_remove(&lru_list, "F"));
    expect(lru_list_remove(&lru_list, "C"));

    _print_list(&lru_list);

    puts("[Test use]");
    expect(lru_list_find(&lru_list, "A") == NULL);
    expect(lru_list_find(&lru_list, "I"));
    expect(lru_list_find(&lru_list, "D"));
    expect(lru_list_find(&lru_list, "G"));

    _print_list(&lru_list);
}
