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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "memory/paddr.h"
#include "memory/vaddr.h"
#include "sdb.h"

static int is_batch_mode = false;
static int is_test_expr = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_si(char *args) {
  char* arg;
  if (args == NULL || (arg = strtok(NULL, " ")) == NULL) {
    cpu_exec(1);
    return 0;
  }

  int64_t nstep;
  int ret = sscanf(arg, "%ld", &nstep);
  if (ret != 1 || (arg = strtok(NULL, " ")) != NULL) {
    printf("A syntax error in expression, near `%s`.\n", arg);
    return 1;
  }

  cpu_exec(nstep > 0 ? nstep : 0);
  return 0;
}

static int cmd_info(char *args) {
  char *scmd = strtok(NULL, " ");

  if (scmd == NULL) {
    printf("List of info subcommands:\n");
    printf("\tinfo r --  List of integer registers and their contents, for selected stack frame.\n");
    printf("\tinfo w --  Status of specified watchpoints (all watchpoints if no argument).\n");
  } else if (!strcmp(scmd, "r")) {
    isa_reg_display();
  } else if (!strcmp(scmd, "w")) {
    // TODO: info watch
    printf("Watch\n");
  } else {
    printf("Undefined info command: \"%s\".\n", scmd);
  }

  return 0;
}

static int cmd_x(char *args) {
  int32_t arg1;
  uint32_t arg2;
  char *token;

  if ((token = strtok(NULL, " ")) == NULL || 
      sscanf(token, "%d", &arg1) != 1) {
    printf("A syntax error in expression, near `%s`.\n", token);
    return 1;
  }
  if ((token = strtok(NULL, " ")) == NULL ||
      sscanf(token, "0x%x", &arg2) != 1 ||
      (token = strtok(NULL, " ")) != NULL) {
    printf("A syntax error in expression, near `%s`, please prefix memory address with '0x'.\n", token);
    return 1;
  }

  const int step = 4;
  uint32_t pp, p_end;
  if (arg1 < 0) {
    pp = arg2 + step * arg1;
    p_end = arg2;
  } else {
    pp = arg2;
    p_end = arg2 + step * arg1;
  }
  // out of bound detection
  if (pp < PMEM_LEFT || pp >= PMEM_RIGHT+1) {
    printf("address = " FMT_PADDR " is out of bound of pmem [" FMT_PADDR ", " FMT_PADDR "]\n",
      pp, PMEM_LEFT, PMEM_RIGHT);
    return 1;
  } else if (p_end > PMEM_RIGHT+1 || p_end < PMEM_LEFT) {
    printf("address = " FMT_PADDR " is out of bound of pmem [" FMT_PADDR ", " FMT_PADDR "]\n",
      p_end, PMEM_LEFT, PMEM_RIGHT);
    return 1;
  }

  while (pp < p_end) {
    printf(ANSI_FMT(FMT_PADDR, ANSI_FG_BLUE) ": ", pp);

    for (int i = 0; i < 4 && pp < p_end; i++, pp+=step) {
      uint32_t m = vaddr_read(pp, step);
      printf("\t" FMT_PADDR, m);
    }
    printf("\n");
  }
  return 0;
}

static int cmd_p(char *args) {
  if (args == NULL) return 1;

  bool success;
  uint32_t r;
  if (strncmp(args, "/", 1) != 0) {
    r = expr(args, &success);
    if (success) printf("%d\n", r);
    return !success;
  }

  char *fmt = strtok(NULL, " ");
  if (strcmp(fmt, "/x")==0) {
    r = expr(args + strlen(fmt) + 1, &success);
    if (success) printf("%#x\n", r);
  } else {
    success = false;
    printf("Error format parameter `%s`.\n", fmt);
  }

  return !success;
}

static int cmd_w(char *args) {
  return 0;
}

static int cmd_d(char *args) {
  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Step [N] instruction", cmd_si },
  { "info", "Info subcmd ('r' for register, 'w' for watchpoint)", cmd_info },
  { "x", "Scan memory and print", cmd_x },
  { "p", "Calculate the value of expressions", cmd_p },
  { "w", "Set watchpoint", cmd_w },
  { "d", "Unset watchpoint", cmd_d },
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_set_test_expr() {
  is_test_expr = true;
}

static void test_expr() {
  char *nemu_home = getenv("NEMU_HOME");
  if (nemu_home == NULL) {
    printf("ERROR: `$NEMU_HOME` is not set!\n");
    return;
  }
  Assert(strlen(nemu_home)<480, "too long value of $NEMU_HOME: %lu\n", strlen(nemu_home));
  char f_input[512];
  strcat(strcat(f_input, nemu_home), "/tools/gen-expr/build/input");

  FILE *fp = fopen(f_input, "r");
  if (fp == NULL) {
    printf("Failed to open file: %s.\n", f_input);
    return;
  }

  bool success;
  char buf[65536 + 128];

  while (fgets(buf, sizeof(buf), fp) != NULL) {
    char *e;
    uint32_t rt_real;

    sscanf(e = (strtok(buf, " ")), "%u", &rt_real);
    e = buf + strlen(e) + 1;

    if (e[strlen(e)-1] == '\n') {
      e[strlen(e)-1] = '\0';  // 使用strcspn函数找到第一个'\n'的位置，然后将其替换为字符串结束符'\0'
    }

    uint32_t rt = expr(e, &success);

    if (!success || rt != rt_real) {
      printf(
        ANSI_FMT("Expression evaluate failed.\n"
                 "get\treal\texpression\n" "%u\t%u\t%s",
                 ANSI_FG_RED)"\n",
        rt, rt_real, e
      );
      assert(0);
    }
  }

  printf("All test cases have passed, Congratuation!\n");
  nemu_state.state = NEMU_END;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  } else if (is_test_expr) {
    test_expr();
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
