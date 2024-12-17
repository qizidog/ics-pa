#include <device.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

size_t serial_write(const void *buf, size_t offset, size_t len) {
  size_t n = 0;
  const char * str = buf;
  for (; n < len; n++) putch(*str++);
  return n;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  AM_INPUT_KEYBRD_T k = io_read(AM_INPUT_KEYBRD);
  // printf("keydown: %d, keycode: %d\n", k.keydown, k.keycode);
  if (k.keycode == AM_KEY_NONE) { ((char *) buf)[0] = '\0'; return 0; }
  return snprintf((char *)buf, len, "%s %s\n", k.keydown ? "kd" : "ku", keyname[k.keycode]);
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  return 0;
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  return 0;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
