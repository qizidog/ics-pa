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

#include "sdb.h"
#include <monitor.h>

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  uint32_t val;
  char *expr;
  struct watchpoint *next;
} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

bool new_wp(char * e) {
  if (free_ == NULL) {
    printf("No watchpoint is avaiable now, please release any watchpoint in use first.\n");
    return false;
  }

  bool success = false;
  int ret = expr(e, &success);
  if (!success) {
    printf("Not a valid watchpoint expression, watchpoint is not created.\n");
    return false;
  }

  WP * wp = free_;
  free_ = free_->next;
  wp->next = head;
  head = wp;

  wp->val = ret;
  int slen = strlen(e) + 1;
  wp->expr = (char*) malloc(slen * sizeof(char));
  strncpy(wp->expr, e, slen);

  return true;
}

void free_wp(uint32_t wp_id) {
  if (wp_id < 0 || wp_id > NR_WP-1) {
    printf("No watchpoint number `%u`.\n", wp_id);
    return;
  }

  WP *prev = NULL, *cur = head;
  while (cur != NULL) {
    if (cur->NO == wp_id) {
      if (prev == NULL) head = cur->next;
      else prev->next = cur->next;
      cur->next = free_;
      free_ = cur;
      free(cur->expr);
      return;
    }
    prev = cur;
    cur = cur->next;
  }
  printf("No watchpoint number `%u`.\n", wp_id);
}

// return true if hit watchpoint
bool check_watchpoints() {
  bool success = false;
  uint32_t val;
  WP *p = head;
  while (p != NULL) {
    val = expr(p->expr, &success);
    if (!success || p->val != val) {
      printf("Watchpoint %d hit.\n", p->NO);
      p->val = val;
      return true;
    }
    p = p->next;
  }
  return false;
}

void info_watchpoints() {
  WP *p = head;
  if (p == NULL) {
    printf("No breakpoints or watchpoints.\n");
    return;
  }
  printf("Num\tWhat\tValue\n");
  while (p != NULL) {
    printf("%d\t%s\t%u\n", p->NO, p->expr, p->val);
    p = p->next;
  }
}

