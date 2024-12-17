#include "debug.h"
#include <fs.h>
#include <device.h>
#include <stdio.h>

typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
  size_t seek_offset;
} Finfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB};

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]  = {"stdin", 0, 0, invalid_read, invalid_write},
  [FD_STDOUT] = {"stdout", 0, 0, invalid_read, serial_write},
  [FD_STDERR] = {"stderr", 0, 0, invalid_read, serial_write},
#include "files.h"
  {"/dev/events", 0, 0, events_read, invalid_write},
};

int fs_open(const char *pathname, int flags, int mode) {
  // 为简化实现，允许对所有已存在的文件进行读写，忽略flags和mode
  // return the new file descriptor, or -1 if an error occurred
  for (int fd = 0; fd < LENGTH(file_table); fd++) {
    Finfo* f = &file_table[fd];
    if (strcmp(pathname, f->name) == 0) {
      if (f->read == NULL) f->read = ramdisk_read;
      if (f->write == NULL) f->write = ramdisk_write;
      return fd;
    }
  }
  return -1;
}

size_t fs_read(int fd, void *buf, size_t len) {
  // return number of objects read successfully
  Finfo* f = &file_table[fd];
  size_t sz = len <= f->size - f->seek_offset ? len : f->size - f->seek_offset;
  f->seek_offset += f->read(buf, f->disk_offset+f->seek_offset, sz);
  return sz;
}

size_t fs_write(int fd, const void *buf, size_t len) {
  Finfo* f = &file_table[fd];
  // assert(f->seek_offset + len <= f->size);
  int c = f->write(buf, f->disk_offset+f->seek_offset, len);
  assert(c != -1);
  f->seek_offset += c;
  return c;
}

size_t fs_lseek(int fd, size_t offset, int whence) {
  // Upon success, returns the resulting offset location as measured in bytes from the beginning of the file.
  // On error, the value -1 is returned
  Finfo* f = &file_table[fd];
  switch (whence) {
    case SEEK_SET: f->seek_offset = offset; break;
    case SEEK_CUR: f->seek_offset += offset; break;
    case SEEK_END: f->seek_offset = f->size + offset; break;
    default: panic("Error seek whence type %d.", whence); return -1;
  }
  return f->seek_offset;
}

int fs_close(int fd) {
  // return 0 on success
  return 0;
}

void init_fs() {
  // TODO: initialize the size of /dev/fb
}
