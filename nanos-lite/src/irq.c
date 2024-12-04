#include <common.h>

void do_syscall(Context *c);

#ifndef CONFIG_ETRACE
# define CONFIG_ETRACE 0
#endif
#define INVOKE_ETRACE(event) if(CONFIG_ETRACE) Log("[ETRACE] " #event)

static Context* do_event(Event e, Context* c) {
  switch (e.event) {
    case EVENT_YIELD: INVOKE_ETRACE(EVENT_YIELD); break;
    case EVENT_SYSCALL: INVOKE_ETRACE(EVENT_SYSCALL); do_syscall(c); break;
    default: panic("Unhandled event ID = %d", e.event);
  }

  return c;
}

void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}
