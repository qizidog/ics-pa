/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include "local-include/reg.h"

const char *regs[] = {
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void isa_reg_display(CPU_state* ref) {
  if (ref == NULL) ref = &cpu;
  else printf("Reference ");
  Assert(ref != NULL, "Reference to a Null pointer!");

  printf("Register information display as follows:\n");
  const char* pattern = "\t%-4s\t%-#12x";
  for (int i = 0; i < ARRLEN(regs); i++) {
    printf(pattern, regs[i], ref->gpr[check_reg_idx(i)]);
    if ((i & 1) == 1) printf("\n");
    else printf("|");
  }
  printf(pattern, "pc", ref->pc); printf("\n");

  // csr display
  printf(pattern, "mstatus", ref->mstatus); printf("\t");
  printf(pattern, "mtvec", ref->mtvec); printf("\n");
  printf(pattern, "mepc", ref->mepc); printf("\t");
  printf(pattern, "mcause", ref->mcause); printf("\n");
}

word_t isa_reg_str2val(const char *s, bool *success) {
  for (int i = 0; i < ARRLEN(regs); i++) {
    if (strcmp(s+1, regs[i]) == 0) {
      *success = true;
      return gpr(i);
    }
  }
  if (strcmp(s+1, "pc") == 0) {
    *success = true;
    return cpu.pc;
  }
  if (strcmp(s+1, "mstatus") == 0) {
    *success = true;
    return cpu.mstatus.val;
  }
  if (strcmp(s+1, "mtvec") == 0) {
    *success = true;
    return cpu.mtvec;
  }
  if (strcmp(s+1, "mepc") == 0) {
    *success = true;
    return cpu.mepc;
  }
  if (strcmp(s+1, "mcause") == 0) {
    *success = true;
    return cpu.mcause;
  }
  *success = false;
  printf("No register named `%s`.\n", s);
  return -1;
}
