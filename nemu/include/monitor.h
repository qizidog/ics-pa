
#ifndef __MONITOR_H__
#define __MONITOR_H__

#include <common.h>

bool check_watchpoints();

void put_ringbuf(char * buf);
void ringbuf_display();

#endif
