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
#include "memory/vaddr.h"
#include "memory/paddr.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  // Operators are listed top to bottom, in descending precedence.
  TK_LBKT = '(', TK_RBKT = ')',
  TK_NOTYPE = 256, TK_NUM, TK_REG,
  TK_SINC = 310, TK_SDEC,
  TK_PINC = 320, TK_PDEC, TK_POS, TK_NEG, TK_LNOT, TK_NOT, TK_DEREF, TK_REF,
  TK_MUL = 330, TK_DIV, TK_MOD,
  TK_ADD = 340, TK_SUB,
  TK_LSHIFT = 350, TK_RSHIFT,
  TK_LT = 360, TK_LTE, TK_GT, TK_GTE,
  TK_EQ = 370, TK_NEQ,
  TK_AND = 380,
  TK_XOR = 390,
  TK_OR = 400,
  TK_LAND = 410,
  TK_LOR = 420,
  TK_COMMA = 430,

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  // regular expression formats
  {" +", TK_NOTYPE},                  // spaces
  {"(0[xX][0-9a-fA-F]+|0[bB][01]+|[0-9]+)[uU]?", TK_NUM},  // number
  {"\\$[0-9a-z]+", TK_REG},           // register
  {"\\(", TK_LBKT},                   // left bracket
  {"\\)", TK_RBKT},                   // right bracket
  {"\\*", TK_MUL},                    // multiplication
  {"\\/", TK_DIV},                    // division
  {"%", TK_MOD},                      // remainder
  {"\\+", TK_ADD},                    // addition
  {"\\-", TK_SUB},                    // subtraction
  {"==", TK_EQ},                      // equal
  {"\\!=", TK_NEQ},                   // not equal
  {"&&", TK_LAND},                    // logical AND
  {"\\|\\|", TK_LOR},                 // logical OR
  {"<=", TK_LTE},                     // less than or equal to
  {"<", TK_LT},                       // less than
  {">=", TK_GTE},                     // greater than of equal to
  {">", TK_GT},                       // greater than
  {"&", TK_AND},                      // bitewise AND
  {"\\^", TK_XOR},                    // bitewise XOR
  {"\\|", TK_OR},                     // bitewise OR
  {"\\!", TK_LNOT},                   // logical NOT
  {"\\~", TK_NOT},                    // bitewise NOT
  {"\\,", TK_COMMA},                  // comma
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

/* #define TOKEN_SIZE 1024  // use 1024 for test */
#define TOKEN_SIZE 32
static Token tokens[TOKEN_SIZE] __attribute__((used)) = {}; // tokens recognized
static int nr_token __attribute__((used)) = 0; // number of token recognized

static int cmp_prece(uint32_t, uint32_t);

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

        switch (rules[i].token_type) {
        case TK_NOTYPE:
          break;
        case TK_NUM:
        case TK_REG:
          Assert(substr_len < 32, "substr_len: %d\n", substr_len);
          strncpy(t.str, substr_start, substr_len);
          t.str[substr_len] = '\0';
          // no break
        default:
          t.type = rules[i].token_type;
          Assert(nr_token < TOKEN_SIZE, "nr_token overflow, expression: %s\n", e);
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

  for (int i=0; i<nr_token-1; i++) {
    uint32_t tk_type = tokens[i].type;

    if (tk_type == TK_NUM && tokens[i+1].type == TK_NUM) {
      printf("Error Expression!\n");
      return false;
    }

    if (tk_type == TK_SUB && (i==0 || cmp_prece(tokens[i-1].type, TK_NEG) <= 0)) {
      tokens[i].type = TK_NEG;
      Log("change tokens[%d] from `TK_SUB` to `TK_NEG`.", i);
      continue;
    }

    if (tk_type == TK_ADD && (i==0 || cmp_prece(tokens[i-1].type, TK_POS) <= 0)) {
      tokens[i].type = TK_POS;
      Log("change tokens[%d] from `TK_ADD` to `TK_POS`.", i);
      continue;
    }

    if (tk_type == TK_MUL && (i==0 || cmp_prece(tokens[i-1].type, TK_DEREF) <= 0)) {
      tokens[i].type = TK_DEREF;
      Log("change tokens[%d] from `TK_MUL` to `TK_DEREF`.", i);
      continue;
    }

  }

  return true;
}

typedef struct {
  uint32_t elems[TOKEN_SIZE];  // TODO: size too large
  int top;
  bool err;
} Stack;


static void push_stack(Stack *s, uint32_t e) {
  if (s->top == ARRLEN(s->elems) - 1) {
    Log("Stack overflow!\n");
    s->err = true;
    assert(0);
  }
  s->elems[++s->top] = e;
}

static uint32_t pop_stack(Stack *s) {
  if (s->top < 0) {
    Log("Stack underflow!\n");
    s->err = true;
    return -1;
  }
  return s->elems[s->top--];
}

static uint32_t top_stack(Stack *s) {
  if (s->top < 0) {
    Log("Empty Stack!\n");
    s->err = true;
    return -1;
  }
  return s->elems[s->top];
}

static bool err_eval = false;
static uint32_t err_tk = -1;

// ret >/=/< 0 if precedence of op1 >/=/< precedence of op2
static int cmp_prece(uint32_t op1, uint32_t op2) {
  return (op2 / 10 * 10) - (op1 / 10 * 10);
}

static bool is_unary(uint32_t op) {
  switch (op) {
    case TK_SINC: case TK_SDEC:
    case TK_PINC: case TK_PDEC:
    case TK_POS: case TK_NEG:
    case TK_LNOT: case TK_NOT:
    case TK_DEREF: case TK_REF:
      return true;
    default: return false;
  }
}

static bool is_r_associ(uint32_t op) {
  switch (op) {
    case TK_PINC: case TK_PDEC:
    case TK_POS: case TK_NEG:
    case TK_LNOT: case TK_NOT:
    case TK_DEREF: case TK_REF:
      return true;
    default: return false;
  }
}

static uint32_t calc(uint32_t op, uint32_t oped1, uint32_t oped2) {
  switch (op) {
    case TK_ADD: return oped1 + oped2;
    case TK_SUB: return oped1 - oped2;
    case TK_MUL: return oped1 * oped2;
    case TK_DIV:
      if (oped2==0) {
        err_eval = true;
        printf("Divided by 0!\n");
        return -1;
      }
      return oped1 / oped2;
    case TK_MOD:
      if (oped2==0) {
        err_eval = true;
        printf("Modular by 0!\n");
        return -1;
      }
      return oped1 % oped2;
    case TK_LAND: return oped1 && oped2;
    case TK_LOR: return oped1 || oped2;
    case TK_LT: return oped1 < oped2;
    case TK_LTE: return oped1 <= oped2;
    case TK_GT: return oped1 > oped2;
    case TK_GTE: return oped1 >= oped2;
    case TK_EQ: return oped1 == oped2;
    case TK_NEQ: return oped1 != oped2;
    case TK_AND: return oped1 & oped2;
    case TK_XOR: return oped1 ^ oped2;
    case TK_OR: return oped1 | oped2;
    case TK_COMMA: return oped2;
    default: err_eval = true; assert(0);
  }
}

static uint32_t calc_unary(uint32_t op, uint32_t oped) {
  switch (op) {
    case TK_POS: return oped;
    case TK_NEG: return -oped;
    case TK_LNOT: return !oped;
    case TK_NOT: return ~oped;
    case TK_DEREF:
      if (oped < PMEM_LEFT || oped >= PMEM_RIGHT+1) {
        printf("address = " FMT_PADDR " is out of bound of pmem [" FMT_PADDR ", " FMT_PADDR "]\n",
               oped, PMEM_LEFT, PMEM_RIGHT);
        err_eval = true;
        return -1;
      }
      return vaddr_read(oped, 4);
    /* case TK_REF: return &oped; */
    default: err_eval = true; assert(0);
  }
}

static uint32_t eval() {
  Stack num_stack = {.top = -1};
  Stack op_stack = {.top = -1};
  uint32_t op, oped1, oped2;

  for (int i = 0; i < nr_token; i++) {
    Token tk = tokens[i];
    if (tk.type == TK_NUM) {
      int num = 0;
      if (strncmp(tk.str, "0x", 2) == 0 || strncmp(tk.str, "0X", 2) == 0) {
        // hexadecimal
        sscanf(tk.str, "%x", &num);
      } else if (strncmp(tk.str, "0b", 2) == 0 || strncmp(tk.str, "0B", 2) == 0) {
        // binary
        for (int i=2; i<strlen(tk.str); i++) {
          num <<= 1;
          num += *(tk.str+i)-'0';
        }
      } else {  // decimal
        sscanf(tk.str, "%d", &num);
      }
      push_stack(&num_stack, num);
      assert(!num_stack.err);
      continue;
    }

    if (tk.type == TK_REG) {
      bool success = false;
      int rval = isa_reg_str2val(tk.str, &success);
      if (!success) goto err_handler;
      push_stack(&num_stack, rval);
      continue;
    }

    if (tk.type == TK_RBKT) {
      // do pop until op=='('
      while ((op = pop_stack(&op_stack)) != TK_LBKT) {
        if (is_unary(op)) {
          oped1 = pop_stack(&num_stack);
          push_stack(&num_stack, calc_unary(op, oped1));
        } else {
          oped2 = pop_stack(&num_stack);
          oped1 = pop_stack(&num_stack);
          push_stack(&num_stack, calc(op, oped1, oped2));
        }
        if (num_stack.err || op_stack.err) goto err_handler;
      }
    } else if (op_stack.top < 0 || top_stack(&op_stack) == TK_LBKT || cmp_prece(tk.type, top_stack(&op_stack)) > 0
      || (cmp_prece(tk.type, top_stack(&op_stack)) == 0 && is_r_associ(tk.type))) {
      // current operator has higher precedence than previous operator
      // or has same precedence but current operator is right associative.
      push_stack(&op_stack, tk.type);
      assert(!op_stack.err);
      continue;
    } else {
      // current operator has lower or same precedence than previous operator
      // and current operator is left associative.
      while (op_stack.top >= 0 && top_stack(&op_stack) != TK_LBKT && cmp_prece(tk.type, top_stack(&op_stack)) <= 0) {
        op = pop_stack(&op_stack);
        if (is_unary(op)) {
          oped1 = pop_stack(&num_stack);
          push_stack(&num_stack, calc_unary(op, oped1));
        } else {
          oped2 = pop_stack(&num_stack);
          oped1 = pop_stack(&num_stack);
          push_stack(&num_stack, calc(op, oped1, oped2));
        }
        if (num_stack.err || op_stack.err) goto err_handler;
      }
      push_stack(&op_stack, tk.type);
    }
    assert(!op_stack.err && !num_stack.err);
  }

  while (op_stack.top >= 0) {
    op = pop_stack(&op_stack);
    if (op == TK_LBKT) {
      goto err_handler;
    }
    if (is_unary(op)) {
      oped1 = pop_stack(&num_stack);
      push_stack(&num_stack, calc_unary(op, oped1));
    } else {
      oped2 = pop_stack(&num_stack);
      oped1 = pop_stack(&num_stack);
      push_stack(&num_stack, calc(op, oped1, oped2));
    }
    if (num_stack.err || op_stack.err) goto err_handler;
  }
  return top_stack(&num_stack);

err_handler:
  err_eval = true;
  printf("Syntax error in expression.\n");
  return -1;
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
  uint32_t rt = eval();

  *success = !err_eval;
  if (err_eval) {
    return -1;
  }

  return rt;
}
