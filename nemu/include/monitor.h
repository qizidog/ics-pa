
#ifndef __MONITOR_H__
#define __MONITOR_H__

#include <common.h>
#include <cpu/decode.h>

bool check_watchpoints();

void invoke_itrace(char * buf);
void itrace_display();

void init_ftrace(char **elf_trace_file, uint32_t nr_elf);
void invoke_ftrace(Decode* s);

#endif
