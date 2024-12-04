#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int NDL_Init(uint32_t flags);
extern uint32_t NDL_GetTicks();

int main() {
  NDL_Init(0);
  uint32_t t1 = NDL_GetTicks();
  uint32_t t2;
  while (1) {
    t2 = NDL_GetTicks();
    if (t2 - t1 >= 500) {
      t1 = t2;
      printf("[Get-Time] milliseconds: %d\n", t1);
    }
  }
}
