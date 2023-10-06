#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

// placeholder type
enum {
  PHT_C = 'c', PHT_S = 's', PHT_D = 'd',
  PHT_P = '%',
};

// placeholder modifier type
enum {
  PHM_N = '\0', PHM_L = 'l',
};


static char* itostr(char *str, int d) {
  int sz = 0;
  int dd = d;
  bool neg = false;
  bool ovf = false;
  int ovfid = 0;

  if (d < 0) {
    ++sz;
    neg = true;
    ovf = d == -d;  // negtive max
    d = ovf ? -(d + 1) : -d;
  }

  // count strlen
  dd = d;
  do {
    ++sz;
    dd /= 10;
  } while (dd != 0);
  /* char * str = (char *) malloc(sz + 1); */
  str[sz] = '\0';
  ovfid = sz - 1;

  // build string
  dd = d;
  do {
    str[--sz] = '0' + dd % 10;
    dd /= 10;
  } while (dd != 0);
  if (neg) str[--sz] = '-';
  if (ovf) {
    assert(str[ovfid] != '9');
    str[ovfid] = str[ovfid] + 1;
  }

  return str;
}

#define PARSE_FMT(cond_and, ...) while (*p != '\0' && (cond_and)) { \
  if (*p != '%') { \
    __parsed_char = *p++; \
    __VA_ARGS__; \
    ++ret; \
    continue; \
  } \
  switch (*++p) { \
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
      __parsed_int = strlen(__parsed_str); \
      for (int i = 0; i < __parsed_int; i++) { \
        __parsed_char = __parsed_str[i]; \
        __VA_ARGS__; \
        ++ret; \
        if (!(cond_and)) break; \
      } \
      break; \
    case PHT_D: \
      __parsed_int = va_arg(ap, int); \
      __parsed_str = buff; \
      itostr(__parsed_str, __parsed_int); \
      __parsed_int = strlen(__parsed_str); \
      for (int i = 0; i < __parsed_int; i++) { \
        __parsed_char = __parsed_str[i]; \
        __VA_ARGS__; \
        ++ret; \
        if (!(cond_and)) break; \
      } \
      break; \
    default: \
      assert(0); \
      break; \
  } \
  ++p; \
}

int printf(const char *fmt, ...) {
  // pointer to the next char to copy
  const char * p = fmt;
  int ret = 0;

  char buff[32];  // TODO: use malloc instead
  int __parsed_int = 0;
  char * __parsed_str = buff;
  char __parsed_char = '\0';

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

  char *d = out;  // pointer to '\0'
  PARSE_FMT(true, *d++ = __parsed_char );
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

  char *d = out;  // pointer to '\0'
  PARSE_FMT(ret < n - 1,  *d++ = __parsed_char );
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
