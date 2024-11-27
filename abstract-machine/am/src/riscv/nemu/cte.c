#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>

static Context* (*user_handler)(Event, Context*) = NULL;

Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    switch (c->mcause) {
      case -1: ev.event = EVENT_YIELD; c->mepc += 4; break;
      default:
        if (c->mcause >= 0 && c->mcause <= 19) {
          ev.event = EVENT_SYSCALL; c->mepc += 4; break;
        } else {
          ev.event = EVENT_ERROR; assert(0); break;
        }
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }

  return c;
}

extern void __am_asm_trap(void);  // 异常入口

bool cte_init(Context*(*handler)(Event, Context*)) {
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));

  // initialize mstatus
  int mstatus = 0x1800;
  asm volatile("csrw mstatus, %0" : : "r"(mstatus));

  // register event handler
  user_handler = handler;

  return true;
}

Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
  return NULL;
}

void yield() {
  asm volatile("li a7, -1; ecall");
}

bool ienabled() {
  return false;
}

void iset(bool enable) {
}
