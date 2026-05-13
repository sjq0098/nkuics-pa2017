#include "common.h"

#define NAME(key) \
  [_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [_KEY_NONE] = "NONE",
  _KEYS(NAME)
};

size_t events_read(void *buf, size_t len) {
  static char evbuf[64];
  static int evlen = 0, evpos = 0;

  if (evpos >= evlen) {
    int key = _read_key();
    if (key != _KEY_NONE) {
      int down = key & 0x8000;
      key &= ~0x8000;
      evlen = snprintf(evbuf, sizeof(evbuf), "%s %s\n",
                       down ? "kd" : "ku", keyname[key]);
    } else {
      evlen = snprintf(evbuf, sizeof(evbuf), "t %u\n", (unsigned)_uptime());
    }
    evpos = 0;
  }

  int avail = evlen - evpos;
  int ret = avail < (int)len ? avail : (int)len;
  memcpy(buf, evbuf + evpos, ret);
  evpos += ret;
  return ret;
}

static char dispinfo[128] __attribute__((used));

void dispinfo_read(void *buf, off_t offset, size_t len) {
  memcpy(buf, dispinfo + offset, len);
}

size_t dispinfo_size(void) {
  return strlen(dispinfo);
}

void fb_write(const void *buf, off_t offset, size_t len) {
  int start_pixel = offset / sizeof(uint32_t);
  int x = start_pixel % _screen.width;
  int y = start_pixel / _screen.width;
  const uint32_t *pixels = buf;
  int n = len / sizeof(uint32_t);

  if (x == 0) {
    int rows = n / _screen.width;
    int rem  = n % _screen.width;
    if (rows > 0) {
      _draw_rect(pixels, 0, y, _screen.width, rows);
      pixels += rows * _screen.width;
      y += rows;
      n = rem;
    }
  }
  while (n > 0) {
    int cols = _screen.width - x;
    if (cols > n) cols = n;
    _draw_rect(pixels, x, y, cols, 1);
    pixels += cols;
    n -= cols;
    x = 0;
    y++;
  }
}

void init_device() {
  _ioe_init();
  snprintf(dispinfo, sizeof(dispinfo), "WIDTH:%d\nHEIGHT:%d\n",
           _screen.width, _screen.height);
}
