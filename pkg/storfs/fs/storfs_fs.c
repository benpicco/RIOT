/*
 * Copyright (C) 2022
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_storfs
 * @{
 *
 * @file
 * @brief       STORfs integration with vfs
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include "storfs.h"
#include "fs/storfs_fs.h"

#define ENABLE_DEBUG 0
#include "debug.h"

static storfs_err_t _errno_to_storfs(int err)
{
    switch (err) {
    case 0:
        return STORFS_OK;
    default:
        return STORFS_ERROR;
    }
}

static int _storfs_to_errno(storfs_err_t err)
{
    switch (err) {
    case STORFS_OK:
        return 0;
    case STORFS_ERROR:
        return -EINVAL;
    case STORFS_WRITE_FAILED:
    case STORFS_READ_FAILED:
        return -EIO;
    case STORFS_MEMORY_DISCREPENCY:
        return -ENOMEM;
    case STORFS_CRC_ERR:
        return -EBADMSG;
    }

    return -1;
}

static storfs_err_t storfs_read(const struct storfs *storfsInst, storfs_page_t page,
                                storfs_byte_t byte, uint8_t *buffer, storfs_size_t size)
{
    mtd_dev_t *mtd = storfsInst->memInst;
    page *= mtd->pages_per_sector;
    return _errno_to_storfs(mtd_read_page(mtd, buffer, page, byte, size));
}

static storfs_err_t storfs_write(const struct storfs *storfsInst, storfs_page_t page,
                                 storfs_byte_t byte, uint8_t *buffer, storfs_size_t size)
{
    mtd_dev_t *mtd = storfsInst->memInst;
    page *= mtd->pages_per_sector;
    return _errno_to_storfs(mtd_write_page(mtd, buffer, page, byte, size));
}

static storfs_err_t storfs_erase(const struct storfs *storfsInst, storfs_page_t page)
{
    mtd_dev_t *mtd = storfsInst->memInst;
    return _errno_to_storfs(mtd_erase_sector(mtd, page, 1));
}

static storfs_err_t storfs_sync(const struct storfs *storfsInst)
{
    (void)storfsInst;
    return STORFS_OK;
}

static inline STORFS_FILE * _get_file(vfs_file_t *f)
{
    /* The buffer in `private_data` is part of a union that also contains a
     * pointer, so the alignment is fine. Adding an intermediate cast to
     * uintptr_t to silence -Wcast-align
     */
    return (STORFS_FILE *)(uintptr_t)f->private_data.buffer;
}

static void _init(storfs_t *fs, mtd_dev_t *mtd)
{
    fs->read  = storfs_read;
    fs->write = storfs_write;
    fs->erase = storfs_erase;
    fs->sync  = storfs_sync;
    fs->memInst = mtd;
    fs->firstByteLoc = 0;
    fs->firstPageLoc = 0;
    fs->pageSize  = mtd->page_size * mtd->pages_per_sector;
    fs->pageCount = mtd->sector_count;
}

static int _format(vfs_mount_t *mountp)
{
    storfs_desc_t *fs = mountp->private_data;

    _init(&fs->fs, fs->dev);

    /* TODO */

    return 0;
}

static int _mount(vfs_mount_t *mountp)
{
    storfs_desc_t *fs = mountp->private_data;

    _init(&fs->fs, fs->dev);

    return _storfs_to_errno(storfs_mount(&fs->fs, (char *)mountp->mount_point));
}

static int _unlink(vfs_mount_t *mountp, const char *name)
{
    storfs_desc_t *fs = mountp->private_data;

    return _storfs_to_errno(storfs_rm(&fs->fs, (char *)name, NULL));
}

static int _mkdir(vfs_mount_t *mountp, const char *name, mode_t mode)
{
    (void)mode;
    storfs_desc_t *fs = mountp->private_data;

    return _storfs_to_errno(storfs_mkdir(&fs->fs, (char *)name));
}

static int _open(vfs_file_t *filp, const char *name, int flags, mode_t mode,
                 const char *abs_path)
{
    storfs_desc_t *fs = filp->mp->private_data;
    STORFS_FILE *stream = _get_file(filp);

    const char *_mode;

    return _storfs_to_errno(storfs_fopen(&fs->fs, (char *)name, _mode, stream));
}

static const vfs_file_system_ops_t storfs_fs_ops = {
    .format = _format,
    .mount = _mount,
    .unlink = _unlink,
    .mkdir = _mkdir,
};

static const vfs_file_ops_t storfs_file_ops = {
    .open = _open,
    .close = _close,
    .read = _read,
    .write = _write,
    .lseek = _lseek,
    .fsync = _fsync,
};

static const vfs_dir_ops_t storfs_dir_ops = {
    .opendir = _opendir,
    .readdir = _readdir,
    .closedir = _closedir,
};

const vfs_file_system_t storfs2_file_system = {
    .fs_op = &storfs_fs_ops,
    .f_op = &storfs_file_ops,
    .d_op = &storfs_dir_ops,
};
