#include <common.h>
#include "syscall.h"

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
    case SYS_exit : Log("syscall - SYS_EXIT"); sys_exit(c); break;
    case SYS_yield : Log("syscall - SYS_YIELD"); sys_yield(c); break;
    default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
