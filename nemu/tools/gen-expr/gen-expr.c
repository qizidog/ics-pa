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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// this should be enough
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";
#define BUF_SIZE 65536
#define ALEN(arr) (sizeof(arr) / sizeof(arr[0]))

static char buf[BUF_SIZE] = {};
static char code_buf[BUF_SIZE + 128] = {}; // a little larger than `buf`

static const char *ops[] = {"+", "-", "*", "/"};
static int buf_ovf = 0;

static uint32_t choose(uint32_t n) { return rand() % n; }

static char *gen(const char *c) {
  char *p = buf;
  if (buf_ovf)
    return p + strlen(p);  // pointer to '\0'
  if (strlen(p) + strlen(c) >= BUF_SIZE) {
    /* printf("Buffer Overflow\n"); */
    buf_ovf = 1;
    return p + strlen(p);  // pointer to '\0'
  }
  strcat(p, c);
  return p + (strlen(p) - strlen(c));
}

static char *gen_num() {
  char num[10];
  int k = 0;
  // random 0-100
  for (int i = 1 + choose(ALEN(num) - 2); i > 0; i--, k++) {
    if (k == 0)
      num[k] = '1' + choose(9);
    else
      num[k] = '0' + choose(10);
  }
  num[k++] = 'u';
  num[k] = '\0';
  return gen(num);
}

static char *gen_rand_op() { return gen(ops[choose(ALEN(ops))]); }

static void gen_space() {
  // random 1-10 spaces
  for (int i = choose(10) + 1; i > 0; i--) {
    // 20% probability to generate a space
    if (choose(5) == 0)
      gen(" ");
  }
}

static void gen_rand_expr() {
  switch (choose(3)) {
  case 0:
    gen_space(); gen_num(); gen_space(); break;
  case 1:
    gen("("); gen_rand_expr(); gen(")"); break;
  default:
    gen_rand_expr();
    char *p = gen_rand_op();
    if (*p == '/') {
      char *c1;
      do {
        c1 = p;
        *(p + 1) = '\0';
        gen_rand_expr();
        while (*c1 == ' ') c1++;
      } while (*c1 == '0');
    } else {
      gen_rand_expr();
    }
    break;
  }
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i++) {
    buf_ovf = 0;
    buf[0] = '\0';
    gen_rand_expr();
    if (buf_ovf)
      continue;

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc -Werror /tmp/.code.c -o /tmp/.expr 2>/dev/null");
    if (ret != 0) {
      /* i--; */
      continue;
    }

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
