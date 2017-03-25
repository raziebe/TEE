#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include "linux_kernfile.h"

struct file* file_open(const char* path, int flags, int rights)
{
    struct file* filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if(IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    return filp;
}

void file_close(struct file* file)
{
    filp_close(file, NULL);
}

size_t file_read(struct file* file, unsigned long long offset, unsigned char* data, size_t size)
{
    mm_segment_t oldfs;
    size_t ret;
    loff_t oset;

    oldfs = get_fs();
    set_fs(get_ds());

    oset = offset;
    ret = vfs_read(file, data, size, &oset);

    set_fs(oldfs);
    return ret;
}

size_t file_write(struct file* file, unsigned char* data, size_t size)
{
    mm_segment_t oldfs;
    size_t ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, data, size, &file->f_pos);

    set_fs(oldfs);
    return ret;
}

size_t file_write_foffset(struct file* file, unsigned char* data, size_t size)
{
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, data, size, &file->f_pos);

    set_fs(oldfs);
    return ret;
}
