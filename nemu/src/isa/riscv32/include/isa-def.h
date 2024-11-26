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

#ifndef __ISA_RISCV32_H__
#define __ISA_RISCV32_H__

#include <common.h>

typedef struct {
  word_t gpr[32];
  vaddr_t pc;
  union {
    struct {
      word_t UIE: 1, SIE: 1, WPRI_0: 1, MIE: 1;
      word_t UPIE: 1, SPIE: 1, WPRI: 1, MPIE: 1;
      word_t SPP: 1, WPRI_1_2: 2, MPP: 2, FS: 2;
      word_t XS: 2, MPRV: 1, SUM: 1, MXR: 1;
      word_t TVM: 1, TW: 1, TSR: 1, WPRI_3_10: 8, SD: 1;
    } bit;
    word_t val;
  } mstatus;  // 0x300
  word_t mtvec;  // 0x305
  word_t mepc;  // 0x341
  word_t mcause;  // 0x342
} riscv32_CPU_state;

// riscv32 csr
enum {
  MSTATUS = 0x300,
  MTVEC = 0x305,
  MEPC = 0x341, MCAUSE = 0x342
};

// decode
typedef struct {
  union {
    uint32_t val;
  } inst;
} riscv32_ISADecodeInfo;

#define isa_mmu_check(vaddr, len, type) (MMU_DIRECT)

#endif
