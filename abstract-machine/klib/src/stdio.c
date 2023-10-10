#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

// format specifiers
enum FS {
  PHT_C = 'c', PHT_S = 's', PHT_D = 'd',
  PHT_X = 'x', PHT_F = 'f', PHT_U = 'u',
  PHT_O = 'o',
  PHT_P = '%',
};

// conversion specifications
typedef struct {
  enum FS fs;
  bool left;  // left-justified
  bool sign;  // always prepend sign signature
  bool space;  // use space for sign padding
  bool lead0;  // leading zero
  bool alt;  // alternative form
  uint32_t width;  // total width
  uint32_t prec;  // precision
  uint32_t cslen;  // cs length
} ConvSpec;


static bool parse_cs(const char* p, ConvSpec* cs) {
  assert(*p == '%');
  const char *ptr = p;
  cs->sign = false;
  cs->space = false;
  cs->left = false;
  cs->lead0 = false;
  cs->width = 0;
  cs->prec = 4;
  cs->cslen = 1;

  // state 1 (optinal)
  while (*ptr != '\0') {
    switch (*++ptr){
      case '-': cs->left = true; break;
      case '+': cs->sign = true; break;
      case ' ': cs->space = true; break;
      case '#': cs->alt = true; break;
      case '0': cs->lead0 = true; break;
      default: goto s2;
    }
    cs->cslen++;
  }

s2:
  // state 2 (optinal)
  if (*ptr >= '0' && *ptr <= '9') {
    cs->width = atoi(ptr);
    for ( ; *ptr >= '0' && *ptr <= '9'; ++ptr ) cs->cslen++;
  }

  // state 3 (optinal)
  if (*ptr == '.') {
    cs->prec = atoi(++ptr);
    for ( ; *ptr >= '0' && *ptr <= '9'; ++ptr ) cs->cslen++;
  }

  // final state (necessary)
  switch (*ptr) {
    case PHT_C: case PHT_S: case PHT_D:
    case PHT_X: case PHT_F: case PHT_U:
    case PHT_O: case PHT_P:
      cs->fs = *ptr; cs->cslen++; break;
    default:
      return false;
  }

  if (cs->sign) cs->space = false;
  if (cs->left) cs->lead0 = false;

  return true;
}

static const char CH_NUM[] = "0123456789abcdef";

// n_sign: treat d as signed number
// p_sign: always put +/- precede number
static char* itostr(char *str, int d, bool n_sign, bool p_sign, bool p_space, unsigned int base, const char* alt) {
  int sz = 0;
  unsigned int dd;
  int ovfid = 0;
  bool ovf = false;
  bool neg = (base == 10) && n_sign && (d < 0);
  assert(base == 8 || base == 10 || base == 16);

  p_sign = (base == 10) && (p_sign || neg);
  p_space = (base == 10) && n_sign && (d >= 0) && !p_sign && p_space;
  if (p_sign || p_space) ++sz;
  if (neg) {
    ovf = d == -d;  // negtive max
    d = ovf ? -(d + 1) : -d;
  }

  // count strlen
  dd = d;
  do {
    ++sz;
    dd /= base;
  } while (dd != 0);
  sz += strlen(alt);
  /* char * str = (char *) malloc(sz + 1); */
  str[sz] = '\0';
  ovfid = sz - 1;

  // build string
  dd = d;
  do {
    str[--sz] = CH_NUM[dd % base];
    dd /= base;
  } while (dd != 0);

  if (p_sign) str[--sz] = neg ? '-' : '+';
  else if (p_space) str[--sz] = ' ';
  for (int i = 0; i < strlen(alt); i++) str[i] = alt[i];

  if (ovf) {
    assert(str[ovfid] != CH_NUM[base-1]);
    str[ovfid] = str[ovfid] + 1;
  }

  return str;
}

#define PUT_STR(s, cs, cond, ...) do { \
  __parsed_tmp = strlen((s)); \
  if (__parsed_tmp >= cs.width) { \
    for (int i = 0; i < __parsed_tmp; i++) { \
      __parsed_char = s[i]; \
      __VA_ARGS__; \
      ++ret; \
      if (!(cond)) break; \
    } \
  } else if (cs.left) { \
    for (int i = 0; i < __parsed_tmp; i++) { \
      __parsed_char = s[i]; \
      __VA_ARGS__; \
      ++ret; \
      if (!(cond)) break; \
    } \
    if (!(cond)) break; \
    for (int i = cs.width - __parsed_tmp; i > 0; i--) { \
      __parsed_char = ' '; \
      __VA_ARGS__; \
      ++ret; \
      if (!(cond)) break; \
    } \
  } else { \
    for (int i = cs.width - __parsed_tmp; i > 0; i--) { \
      __parsed_char = cs.lead0 ? '0' : ' '; \
      __VA_ARGS__; \
      ++ret; \
      if (!(cond)) break; \
    } \
    if (!(cond)) break; \
    for (int i = 0; i < __parsed_tmp; i++) { \
      __parsed_char = s[i]; \
      __VA_ARGS__; \
      ++ret; \
      if (!(cond)) break; \
    } \
  }\
} while(0)

#define PUT_NUM(num, base, n_sign, p_sign, p_space, alt, cond, ...) do { \
  __parsed_str = buff; \
  itostr(__parsed_str, (num), (n_sign), (p_sign), (p_space), (base), (alt)); \
  PUT_STR(__parsed_str, cs, (cond), __VA_ARGS__); \
} while(0)

#define PARSE_FMT(cond_and, ...) while (*p != '\0' && (cond_and)) { \
  if (*p != '%' || !parse_cs(p, &cs)) { \
    __parsed_char = *p++; \
    __VA_ARGS__; \
    ++ret; \
    continue; \
  } \
  switch (cs.fs) { \
    case PHT_P: \
      __parsed_char = '%'; \
      __VA_ARGS__; \
      ++ret; \
      break; \
    case PHT_C: \
      __parsed_int = va_arg(ap, int); \
      __parsed_char = (char)__parsed_int; \
      __VA_ARGS__; \
      ++ret; \
      break; \
    case PHT_S: \
      __parsed_str = va_arg(ap, char *); \
      PUT_STR(__parsed_str, cs, (cond_and), __VA_ARGS__); \
      break; \
    case PHT_O: \
      __parsed_int = va_arg(ap, int); \
      PUT_NUM(__parsed_int, 8, false, false, cs.space, "", (cond_and), __VA_ARGS__); \
      break; \
    case PHT_X: \
      __parsed_int = va_arg(ap, int); \
      PUT_NUM(__parsed_int, 16, false, false, cs.space, "0x", (cond_and), __VA_ARGS__); \
      break; \
    case PHT_U: \
      __parsed_int = va_arg(ap, int); \
      PUT_NUM(__parsed_int, 10, false, cs.sign, cs.space, "", (cond_and), __VA_ARGS__); \
      break; \
    case PHT_D: \
      __parsed_int = va_arg(ap, int); \
      PUT_NUM(__parsed_int, 10, true, cs.sign, cs.space, "", (cond_and), __VA_ARGS__); \
      break; \
    default: \
      putstr("Type not support yet."); \
      assert(0); \
      break; \
  } \
  p += cs.cslen; \
}

int printf(const char *fmt, ...) {
  // pointer to the next char to copy
  const char * p = fmt;
  int ret = 0;

  char buff[32];  // TODO: use malloc instead
  int __parsed_int = 0;
  char * __parsed_str = buff;
  char __parsed_char = '\0';
  int __parsed_tmp = 0;
  ConvSpec cs;

  va_list ap;
  va_start(ap, fmt);
  PARSE_FMT(true, putch(__parsed_char));
  va_end(ap);

  return ret;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  // pointer to the next char to copy
  const char * p = fmt;
  int ret = 0;

  char buff[32];  // TODO: use malloc instead
  int __parsed_int = 0;
  char * __parsed_str = buff;
  char __parsed_char = '\0';
  int __parsed_tmp = 0;
  ConvSpec cs;

  char *d = out;  // pointer to '\0'
  PARSE_FMT(true, *d++ = __parsed_char);
  out[ret] = '\0';

  return ret;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);

  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  // pointer to the next char to copy
  const char * p = fmt;
  int ret = 0;

  char buff[32];  // TODO: use malloc instead
  int __parsed_int = 0;
  char * __parsed_str = buff;
  char __parsed_char = '\0';
  int __parsed_tmp = 0;
  ConvSpec cs;

  char *d = out;  // pointer to '\0'
  PARSE_FMT(ret < n - 1, *d++ = __parsed_char);
  out[ret] = '\0';

  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);

  return ret;
}

#undef PARSE_FMT

#endif
