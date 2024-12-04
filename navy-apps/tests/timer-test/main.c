#include <assert.h>
#include <stdio.h>
#include <sys/time.h>

int main() {
  struct timeval tv;
  struct timezone tz;
  time_t s = tv.tv_sec;
  suseconds_t ms = tv.tv_usec;
  while (1) {
    assert(gettimeofday(&tv, &tz) == 0);
    if ((s == tv.tv_sec && tv.tv_usec - ms >= 500000) ||
        (s < tv.tv_sec &&
         tv.tv_usec - ms + (tv.tv_sec - s) * 1000000 >= 500000)) {
      s = tv.tv_sec;
      ms = tv.tv_usec;
      printf("[Get-Time] seconds: %ld, microseconds: %ld\n", s, ms);
    }
  }
}
