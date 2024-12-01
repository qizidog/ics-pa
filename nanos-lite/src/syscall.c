#include <common.h>
#include <fs.h>
#include "syscall.h"

#ifndef CONFIG_STRACE
# define CONFIG_STRACE 0
#endif
#define INVOKE_STRACE(c) if (CONFIG_STRACE) Log("[STRACE] syscall " #c " with NO = %d", c)
#define SYS_CASE(sc) case SYS_##sc: INVOKE_STRACE(SYS_##sc); sys_##sc(c); break

static void sys_exit(Context *c){
  halt(c->GPR2);
}

static void sys_yield(Context *c){
  yield();
  c->GPRx = 0;
}

static void sys_open(Context *c){
  const char *path = (char *)c->GPR2;
  int flags = c->GPR3;
  int mode = c->GPR4;
  c->GPRx = fs_open(path, flags, mode);
}

static void sys_read(Context *c){
  int fd = c->GPR2;
  void *buf = (char *)c->GPR3;
  size_t count = c->GPR4;
  c->GPRx = fs_read(fd, buf, count);
}

static void sys_write(Context *c){
  int fd = c->GPR2;
  char * buf = (char*) c->GPR3;
  size_t count = c->GPR4;
  c->GPRx = fs_write(fd, buf, count);
}

static void sys_close(Context *c){
  int fd = c->GPR2;
  c->GPRx = fs_close(fd);
}

static void sys_lseek(Context *c){
  int fd = c->GPR2;
  int offset = c->GPR3;
  int whence = c->GPR4;
  c->GPRx = fs_lseek(fd, offset, whence);
}

static void sys_brk(Context *c){
  // intptr_t incr = c->GPR2;
  c->GPRx = 0;
}

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;

  switch (a[0]) {
    SYS_CASE(exit);
    SYS_CASE(yield);
    SYS_CASE(open);
    SYS_CASE(read);
    SYS_CASE(write);
    SYS_CASE(close);
    SYS_CASE(lseek);
    SYS_CASE(brk);
    default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
