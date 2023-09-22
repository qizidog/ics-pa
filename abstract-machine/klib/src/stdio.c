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

static int itostr(char *s, int d) {
  int ct = 0;
  int i = 0;

  if (d < 0) {
    ++ct;
    s[i++] = '-';
    d = -d;  // negative max integer?
  }

  char *p = s + i;
  do {
    s[i++] = d % 10 + 48;
    d /= 10;
    ++ct;
  } while (d != 0);
  s[i] = '\0';

  // swap number order
  char *q = s + i - 1;
  char t;
  while (p < q) {
    t = *p;
    *p++ = *q;
    *q-- = t;
  }

  return ct;
}

// parse the placeholder pointed to by p,
// concat parsed string to *d with next param in args,
// and update d to '\0', p to next char to parse.
#define parse_ph(d, p, args) do { \
  assert(*(p) == '%'); \
  ++(p); \
  int parsed_int_val; \
  char* parsed_str_val; \
  switch (*(p)) { \
    case PHT_C: \
      parsed_int_val = (char) va_arg(args, int); \
      (d)[0] = parsed_int_val; \
      (d)[1] = '\0'; \
      ++(d); \
      break; \
    case PHT_S: \
      parsed_str_val = va_arg(args, char*); \
      strcat((d), parsed_str_val); \
      (d) += strlen(parsed_str_val); \
      break; \
    case PHT_D: \
      parsed_int_val = (char) va_arg(args, int); \
      (d) += itostr((d), parsed_int_val); \
      break; \
    case PHT_P: \
      (d)[0] = '%'; \
      (d)[1] = '\0'; \
      ++(d); \
      break; \
    default: assert(0); \
  } \
  ++(p); \
} while(0)

#define parse_nph(d, p, n, args) do { \
  assert(*(p) == '%'); \
  assert((n) > 0); \
  ++(p); \
  int parsed_int_val; \
  char* parsed_str_val; \
  switch (*(p)++) { \
    case PHT_C: \
      parsed_int_val = (char) va_arg(args, int); \
      (d)[0] = parsed_int_val; \
      (d)[1] = '\0'; \
      ++(d); \
      break; \
    case PHT_S: \
      parsed_str_val = va_arg(args, char*); \
      strncat((d), parsed_str_val, (n) - 1); \
      if (strlen(parsed_str_val) >= (n) - 1) { \
        d += (n); \
        (p) = (d); /* set *p to '\0' */ \
      } else { \
        d += (strlen(parsed_str_val)); \
      } \
      break; \
    case PHT_D: \
      parsed_int_val = (char) va_arg(args, int); \
      { /* check usable space */ \
        int ct = 0, t = parsed_int_val; \
        if (t < 0) { ++ct; t = -t; } \
        do { \
          t /= 10; \
          ++ct; \
        } while (t != 0); \
        if (ct > n) { \
          (p) = (d); /* set *p to '\0' */ \
          break; \
        } \
      } \
      (d) += itostr((d), parsed_int_val); \
      break; \
    case PHT_P: \
      (d)[0] = '%'; \
      (d)[1] = '\0'; \
      ++(d); \
      break; \
    default: assert(0); \
  } \
} while(0)

int printf(const char *fmt, ...) {
  panic("Not implemented");
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  *out = '\0';
  char *d = out;  // pointer to '\0'
  // p1 pointer to the first char to copy, p2 pointer to the next char to copy
  const char *p1 = fmt, *p2 = fmt;

  while (*p2 != '\0') {
    if (*p2 == '%') {
      strncat(d, p1, p2 - p1);
      d += p2 - p1;
      parse_ph(d, p2, ap);  // p2 and d are updated
      p1 = p2;
    } else {
      ++p2;
    }
  }
  strncat(d, p1, p2 - p1);
  d += p2 - p1;

  return d - out;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);

  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  *out = '\0';
  char *d = out;  // pointer to '\0'
  // p pointer to the next char to copy
  const char *p = fmt;

  while (*p != '\0' && d - out < n - 1) {
    if (*p == '%') {
      parse_nph(d, p, n, ap);
    } else {
      *d++ = *p++;
    }
  }
  *d = '\0';

  return d - out;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);

  return ret;
}

#undef parse_ph

#endif
