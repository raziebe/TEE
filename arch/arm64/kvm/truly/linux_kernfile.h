#pragma once

struct file;

struct file* file_open (const char* path, int flags, int rights);
void file_close(struct file* file);
size_t file_read(struct file* file, unsigned long long offset, unsigned char* data, size_t size);
size_t file_write(struct file* file, unsigned char* data, size_t size);
size_t file_write_foffset(struct file* file, unsigned char* data, size_t size);