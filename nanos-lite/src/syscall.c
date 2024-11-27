#include <common.h>
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
  c->GPRx=0;
}

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;

  switch (a[0]) {
    SYS_CASE(exit);
    SYS_CASE(yield);
    default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
