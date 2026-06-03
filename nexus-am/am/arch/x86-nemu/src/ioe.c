#include <am.h>
#include <x86.h>

#define RTC_PORT 0x48
#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_STATUS_HASKEY_MASK 0x1
#define VGA_SYNC_PORT 0x100
#define FB_ADDR 0x40000
#define SCREEN_W 400
#define SCREEN_H 300

static unsigned long boot_time;

void _ioe_init() {
  boot_time = inl(RTC_PORT);
}

unsigned long _uptime() {
  return inl(RTC_PORT) - boot_time;
}

static volatile uint32_t *const fb = (volatile uint32_t *)FB_ADDR;

_Screen _screen = {
  .width  = SCREEN_W,
  .height = SCREEN_H,
};

void _draw_rect(const uint32_t *pixels, int x, int y, int w, int h) {
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      fb[(y + j) * _screen.width + (x + i)] = pixels[j * w + i];
    }
  }
}

void _draw_sync() {
  outl(VGA_SYNC_PORT, 1);
}

int _read_key() {
  if ((inb(KBD_STATUS_PORT) & KBD_STATUS_HASKEY_MASK) == 0) {
    return _KEY_NONE;
  }
  return inl(KBD_DATA_PORT);
}
