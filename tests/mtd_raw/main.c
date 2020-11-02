/*
 * Copyright (c) 2020 ML!PA Consulting GmbH
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
 * @brief       Application for testing MTD implementations
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "od.h"
#include "mtd.h"
#include "shell.h"
#include "board.h"

#ifndef MTD_NUMOF
#ifdef MTD_0
#define MTD_NUMOF 1
#else
#define MTD_NUMOF 0
#endif
#endif

static mtd_dev_t *_get_mtd_dev(unsigned idx)
{
    switch (idx) {
#ifdef MTD_0
    case 0: return MTD_0;
#endif
#ifdef MTD_1
    case 1: return MTD_1;
#endif
#ifdef MTD_2
    case 2: return MTD_2;
#endif
#ifdef MTD_3
    case 3: return MTD_3;
#endif
    }

    return NULL;
}

static mtd_dev_t *_get_dev(int argc, char **argv, bool *oob)
{
    if (argc < 2) {
        printf("%s: please specify the MTD device\n", argv[0]);
        *oob = true;
        return NULL;
    }

    unsigned idx = atoi(argv[1]);

    if (idx > MTD_NUMOF) {
        printf("%s: invalid device: %s\n", argv[0], argv[1]);
        *oob = true;
        return NULL;
    }

    *oob = false;
    return _get_mtd_dev(idx);
}

static uint64_t _get_size(mtd_dev_t *dev)
{
    return dev->sector_count * dev->pages_per_sector * dev->page_size;
}

static int cmd_read(int argc, char **argv)
{
    bool oob;
    mtd_dev_t *dev = _get_dev(argc, argv, &oob);
    uint32_t addr, len;

    if (oob) {
        return -1;
    }

    if (argc < 4) {
        printf("usage: %s <dev> <addr> <len>\n", argv[0]);
        return -1;
    }

    addr = atoi(argv[2]);
    len  = atoi(argv[3]);

    void *buffer = malloc(len);
    if (buffer == NULL) {
        puts("out of memory");
        return -1;
    }

    int res = mtd_read(dev, buffer, addr, len);

    od_hex_dump_ext(buffer, len, 0, addr);

    free(buffer);

    if (res) {
        printf("error: %i\n", res);
    }

    return res;
}

static int cmd_read_page(int argc, char **argv)
{
    bool oob;
    mtd_dev_t *dev = _get_dev(argc, argv, &oob);
    uint32_t page, offset, len;

    if (oob) {
        return -1;
    }

    if (argc < 5) {
        printf("usage: %s <dev> <page> <offset> <len>\n", argv[0]);
        return -1;
    }

    page   = atoi(argv[2]);
    offset = atoi(argv[3]);
    len    = atoi(argv[4]);

    void *buffer = malloc(len);
    if (buffer == NULL) {
        puts("out of memory");
        return -1;
    }

    int res = mtd_read_page(dev, buffer, page, offset, len);

    od_hex_dump_ext(buffer, len, 0, page * dev->page_size + offset);

    free(buffer);

    if (res) {
        printf("error: %i\n", res);
    }

    return res;
}

static int cmd_write(int argc, char **argv)
{
    bool oob;
    mtd_dev_t *dev = _get_dev(argc, argv, &oob);
    uint32_t addr, len;

    if (oob) {
        return -1;
    }

    if (argc < 4) {
        printf("usage: %s <dev> <addr> <data>\n", argv[0]);
        return -1;
    }

    addr = atoi(argv[2]);
    len  = strlen(argv[3]);

    int res = mtd_write(dev, argv[3], addr, len);

    if (res) {
        printf("error: %i\n", res);
    }

    return res;
}

static int cmd_write_page(int argc, char **argv)
{
    bool oob;
    mtd_dev_t *dev = _get_dev(argc, argv, &oob);
    uint32_t page, offset, len;

    if (oob) {
        return -1;
    }

    if (argc < 5) {
        printf("usage: %s <dev> <page> <offset> <len>\n", argv[0]);
        return -1;
    }

    page   = atoi(argv[2]);
    offset = atoi(argv[3]);
    len    = strlen(argv[4]);

    int res = mtd_write_page(dev, argv[4], page, offset, len);

    if (res) {
        printf("error: %i\n", res);
    }

    return res;
}

static int cmd_erase(int argc, char **argv)
{
    bool oob;
    mtd_dev_t *dev = _get_dev(argc, argv, &oob);
    uint32_t addr;
    uint32_t len;

    if (oob) {
        return -1;
    }

    if (argc < 4) {
        printf("usage: %s <dev> <addr> <len>\n", argv[0]);
        return -1;
    }

    addr = atoi(argv[2]);
    len  = atoi(argv[3]);

    int res = mtd_erase(dev, addr, len);

    if (res) {
        printf("error: %i\n", res);
    }

    return res;
}

static int cmd_erase_sector(int argc, char **argv)
{
    bool oob;
    mtd_dev_t *dev = _get_dev(argc, argv, &oob);
    uint32_t sector, count = 1;

    if (oob) {
        return -1;
    }

    if (argc < 3) {
        printf("usage: %s <dev> <sector> [count]\n", argv[0]);
        return -1;
    }

    sector = atoi(argv[2]);

    if (argc > 3) {
        count = atoi(argv[3]);
    }

    int res = mtd_erase_sector(dev, sector, count);

    if (res) {
        printf("error: %i\n", res);
    }

    return res;
}

static void _print_info(mtd_dev_t *dev)
{
    printf("sectors: %lu\n", dev->sector_count);
    printf("pages per sector: %lu\n", dev->pages_per_sector);
    printf("page_size: %lu\n", dev->page_size);
    printf("total: %lu\n", (unsigned long)_get_size(dev));
}

static int cmd_info(int argc, char **argv)
{
    if (argc < 2) {
        for (int i = 0; i < MTD_NUMOF; ++i) {
            printf(" -=[ MTD_%u ]=-\n", i);
            _print_info(_get_mtd_dev(i));
        }
        return 0;
    }

    bool oob;
    mtd_dev_t *dev = _get_dev(argc, argv, &oob);

    if (oob) {
        return -1;
    }

    _print_info(dev);

    return 0;
}

static int cmd_power(int argc, char **argv)
{
    bool oob;
    mtd_dev_t *dev = _get_dev(argc, argv, &oob);
    enum mtd_power_state state;

    if (oob) {
        return -1;
    }

    if (argc < 3) {
        goto error;
    }

    if (strcmp(argv[2], "off") == 0) {
        state = MTD_POWER_DOWN;
    } else if (strcmp(argv[2], "on") == 0) {
        state = MTD_POWER_UP;
    } else {
        goto error;
    }

    mtd_power(dev, state);

    return 0;

error:
    printf("usage: %s <dev> <on|off>\n", argv[0]);
    return -1;
}

static bool mem_is_all_set(const uint8_t *buf, uint8_t c, size_t n)
{
    for (const uint8_t *end = buf + n; buf != end; ++buf) {
        if (*buf != c) {
            return false;
        }
    }

    return true;
}

static int cmd_test(int argc, char **argv)
{
    bool oob;
    mtd_dev_t *dev = _get_dev(argc, argv, &oob);
    uint32_t sector;

    if (oob) {
        return -1;
    }

    if (dev->sector_count < 2) {
        return -1;
    }

    if (argc > 2) {
        sector = atoi(argv[2]);
    } else {
        sector = dev->sector_count - 2;
    }

    uint8_t *buffer = malloc(dev->page_size);

    if (buffer == NULL) {
        puts("out of memory");
        return -1;
    }

    uint32_t page_0 = dev->pages_per_sector * sector;
    uint32_t page_1 = dev->pages_per_sector * (sector + 1);
    uint32_t page_size = dev->page_size;

    puts("[START]");

    /* write dummy data to sectors */
    memset(buffer, 0x23, dev->page_size);
    assert(mtd_write_page(dev, buffer, page_0, 0, page_size) == 0);
    assert(mtd_write_page(dev, buffer, page_1, 0, page_size) == 0);

    /* erase two sectors and check if they have been erase */
    assert(mtd_erase_sector(dev, sector, 2) == 0);
    assert(mtd_read_page(dev, buffer, page_0, 0, page_size) == 0);
    assert(mem_is_all_set(buffer, 0xFF, page_size));
    assert(mtd_read_page(dev, buffer, page_1, 0, page_size) == 0);
    assert(mem_is_all_set(buffer, 0xFF, page_size));

    /* write test data & read it back */
    const char test_str[] = "0123456789";
    uint32_t offset = 5;
    assert(mtd_write_page(dev, test_str, page_0, offset, sizeof(test_str)) == 0);
    assert(mtd_read_page(dev, buffer, page_0, offset, sizeof(test_str)) == 0);
    assert(memcmp(test_str, buffer, sizeof(test_str)) == 0);

    /* write across page boundary */
    offset = page_size - sizeof(test_str) / 2;
    assert(mtd_write_page(dev, test_str, page_0, offset, sizeof(test_str)) == 0);
    assert(mtd_read_page(dev, buffer, page_0, offset, sizeof(test_str)) == 0);
    assert(memcmp(test_str, buffer, sizeof(test_str)) == 0);

    /* write across sector boundary */
    offset = page_size - sizeof(test_str) / 2
           + (dev->pages_per_sector - 1) * page_size;
    assert(mtd_write_page(dev, test_str, page_0, offset, sizeof(test_str)) == 0);
    assert(mtd_read_page(dev, buffer, page_0, offset, sizeof(test_str)) == 0);
    assert(memcmp(test_str, buffer, sizeof(test_str)) == 0);

    puts("[SUCCESS]");

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "info", "Print properties of the MTD device", cmd_info },
    { "power", "Turn the MTD device on/off", cmd_power },
    { "read", "Read a region of memory on the MTD device", cmd_read },
    { "read_page", "Read a region of memory on the MTD device (pagewise addressing)", cmd_read_page },
    { "write", "Write a region of memory on the MTD device", cmd_write },
    { "write_page", "Write a region of memory on the MTD device (pagewise addressing)", cmd_write_page },
    { "erase", "Erase a region of memory on the MTD device", cmd_erase },
    { "erase_sector", "Erase a sector of memory on the MTD device", cmd_erase_sector },
    { "test", "Erase & write test data to the last two sectors", cmd_test },
    { NULL, NULL, NULL }
};

int main(void)
{
    puts("Manual MTD test");

    if (MTD_NUMOF == 0) {
        puts("no MTD device present on the board.");
    }

    for (int i = 0; i < MTD_NUMOF; ++i) {
        printf("init MTD_%u… ", i);

        mtd_dev_t *dev = _get_mtd_dev(i);
        int res = mtd_init(dev);
        if (res) {
            printf("error: %d\n", res);
            continue;
        }

        printf("OK (%lu kiB)\n", (unsigned long)(_get_size(dev) / 1024));
        mtd_power(dev, MTD_POWER_UP);
    }

    /* run the shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
