#include <common.h>

#define NR_RB 10
#define SZ_BUF 128

static char ringbuf[NR_RB][SZ_BUF];
static int head = 0;

void put_ringbuf(char * buf) {
  strncpy(ringbuf[head], buf, SZ_BUF);
  head = (head + 1) % NR_RB;
}

void ringbuf_display() {
  printf("Instruction ring buffer (record size %d) display as follows:\n", NR_RB);
  for (int i = 0; i < NR_RB; i++) {
    if (ringbuf[i][0] != '\0') {
      if (i == (head + NR_RB - 1) % NR_RB) printf("-->");
      printf("\t%s\n", ringbuf[i]);
    }
  }
}
