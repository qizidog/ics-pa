#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <common.h>

struct timeval {
  long tv_sec;       /* seconds */
  long tv_usec; /* microseconds */
};
struct timezone {
  int tz_minuteswest; /* minutes west of Greenwich */
  int tz_dsttime;     /* type of DST correction */
};

#endif
