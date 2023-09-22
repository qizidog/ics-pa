#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

void *memset(void *s, int c, size_t n) {
  for (unsigned char *p = s; n-- > 0; *p++ = c);
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  unsigned char *d = dst;
  const unsigned char *s = src;
  if (dst == src) {
  } else if (dst < src) {
    // Copy from the beginning to the end.
    for (; n-- > 0; *d++ = *s++);
  } else {
    // Copy from the end to the beginning.
    for (; n-- > 0; *(d+n) = *(s+n));
  }
  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  unsigned char *d;
  const unsigned char *s;
  for (d = out, s = in; n-- > 0; *d++ = *s++);
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = s1, *p2 = s2;
  for (size_t i = 0; i < n; i++) {
    if (*p1 != *p2) return *p1 - *p2;
    p1 ++; p2 ++;
  }
  return 0;
}

size_t strlen(const char *s) {
  size_t c = 0;
  for (const char *p = s; *p++ != '\0'; ++c);
  return c;
}

char *strcpy(char *dst, const char *src) {
  char *d = dst;
  do { *d++ = *src; } while (*src++ != '\0');
  return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
  size_t len = strlen(src) + 1;
  if (n <= len) {
    memcpy(dst, src, n);
  } else {
    memcpy(dst, src, len);
    memset(dst+len, '\0', n-len);
  }
  return dst;
}

char *strcat(char *dst, const char *src) {
  size_t len_d = strlen(dst);
  strcpy(dst + len_d, src);
  return dst;
}

char *strncat(char *dst, const char *src, size_t n) {
  size_t len_d = strlen(dst);
  size_t i;

  for (i = 0; i < n && src[i] != '\0'; i++)
    dst[len_d + i] = src[i];
  dst[len_d + i] = '\0';

  return dst;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  const char *p1 = s1, *p2 = s2;
  for (size_t i = 0; i < n; i++) {
    if (*p1 == '\0' || *p2 == '\0' || *p1 != *p2) return *p1 - *p2;
    p1 ++; p2 ++;
  }
  return 0;
}

int strcmp(const char *s1, const char *s2) {
  return strncmp(s1, s2, -1);
}

#endif
