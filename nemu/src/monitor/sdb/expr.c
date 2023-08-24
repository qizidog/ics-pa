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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256,
  TK_EQ,
  TK_NUM,
  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  // regular expression formats

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE}, // spaces
  {"\\+", '+'},      // plus
  {"\\-", '-'},      // minus
  {"\\*", '*'},      // multiply
  {"\\/", '/'},      // divide
  {"\\(", '('},      // left bracket
  {"\\)", ')'},      // right bracket
  {"==", TK_EQ},     // equal
  {"[0-9]+", TK_NUM},// number
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {}; // compiled regular expressions

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {}; // tokens recognized
static int nr_token __attribute__((used)) = 0; // number of token recognized

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        Token t = {.str = ""};
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
        case TK_NOTYPE:
          break;
        case TK_NUM:
          Assert(substr_len < 32, "substr_len: %d\n", substr_len);
          strncpy(t.str, substr_start, substr_len);
          t.str[substr_len] = '\0';
          // no break
        default:
          t.type = rules[i].token_type;
          Assert(nr_token < 32, "nr_token: %d", nr_token);
          tokens[nr_token++] = t;
          break;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

static bool err_eval = false;
static uint32_t err_tk = -1;

static bool check_parentheses(uint32_t l, uint32_t r) {
  int sz_s = r - l + 1;
  /* if ((sz_s = r - l + 1) == 2 || tokens[l].type != '(' || tokens[r].type != ')') return false; */
  bool quick = tokens[l].type == '(' && tokens[r].type == ')';

  int nr_p = 0;  // parentheses number in stack
  int pstack[sz_s];  // parentheses stack
  for (int i = l; i <= r; i++) {
    if (tokens[i].type == '(') {
      pstack[nr_p++] = i;
    } else if (tokens[i].type == ')') {
      nr_p--;
      if (nr_p < 0) {
        err_tk = i;
        err_eval = true;
        return false;
      }
    }
  }
  if ((err_eval = (nr_p!=0))) err_tk = pstack[nr_p-1];
  return nr_p == 0 && pstack[0] == l && quick;
}

static uint32_t eval(uint32_t l, uint32_t r) {
  if (err_eval) goto err;

  uint32_t ret;
  /* assert(l <= r); */
  if (l == r) {
    if (tokens[l].type != TK_NUM || sscanf(tokens[l].str, "%u", &ret) != 1) {
      err_tk = l < nr_token ? l : nr_token-1;
      goto err;
    }
    return ret;
  } else if (check_parentheses(l, r)) {
    if (err_eval) goto err;
    return eval(l + 1, r - 1);
  } else {
    int nr_p = 0;
    uint32_t op = r;
    for (int i = l; i <= r; i++) {
      int tt = tokens[i].type;
      if (tt == TK_NUM) {
        // pass
      } else if (tt == '(') {
        nr_p++;
      } else if (tt == ')') {
        nr_p--;
      } else if (nr_p != 0) {
        // pass
      } else if ((tt == '*' || tt == '/') && (op==l || (tokens[op].type != '+' && tokens[op].type != '-'))) {
        op = i;
      } else if ((tt == '+' || tt == '-') && (i==l || (tokens[i-1].type==TK_NUM || tokens[i-1].type==')'))) {
        op = i;
      }
    }
    if (op == r || tokens[op].type >= 256) {
      err_tk = op;
      goto err;
    } else if (op == l) {
      uint32_t val = eval(op+1, r);
      switch (tokens[op].type) {
        case '+': return val;
        case '-': return -val;
        default:
          err_tk = op;
          goto err;
      }
    }

    uint32_t val1 = eval(l, op - 1);
    uint32_t val2 = eval(op + 1, r);
    if (err_eval) goto err;

    switch (tokens[op].type ) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/':
        if (val2 == 0) {
          err_tk = op;
          goto err;
        }
        return val1 / val2;
      default: assert(0);
    }
  }

err:
  err_eval = true;
  return err_tk;
}

// calculate expression and return result.
// if not a valid expression, detect probable syntax error and return -1.
word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return -1;
  }

  err_eval = false;
  err_tk = -1;
  uint32_t rt = eval(0, nr_token - 1);

  *success = !err_eval;
  if (err_eval) {
    printf("Syntax error in expression, near \"");
    if (tokens[err_tk].type < 256) {
      printf("%c\".\n", tokens[err_tk].type);
    } else {
      printf("%s\".\n", tokens[err_tk].str);
    }
    return -1;
  }

  return rt;
}
