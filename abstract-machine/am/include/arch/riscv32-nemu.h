#ifndef ARCH_H__
#define ARCH_H__

struct Context {
  // fix the order of these members to match trap.S
  uintptr_t gpr[32];
  uintptr_t mcause, mstatus, mepc;
  void *pdir;
};

#define GPR1 gpr[17] // a7, syscall number
#define GPR2 gpr[10] // a0, arg1
#define GPR3 gpr[11] // a1, arg2
#define GPR4 gpr[12] // a2, arg3
#define GPRx gpr[10] // a0, ret

#endif
