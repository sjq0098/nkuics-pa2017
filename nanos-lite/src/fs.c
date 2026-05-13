#include "fs.h"

void ramdisk_read(void *buf, off_t offset, size_t len);
void ramdisk_write(const void *buf, off_t offset, size_t len);
size_t events_read(void *buf, size_t len);
void dispinfo_read(void *buf, off_t offset, size_t len);
size_t dispinfo_size(void);
void fb_write(const void *buf, off_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  off_t disk_offset;
  off_t open_offset;
} Finfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB, FD_EVENTS, FD_DISPINFO, FD_NORMAL};

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
  {"stdin (note that this is not the actual stdin)", 0, 0},
  {"stdout (note that this is not the actual stdout)", 0, 0},
  {"stderr (note that this is not the actual stderr)", 0, 0},
  [FD_FB] = {"/dev/fb", 0, 0},
  [FD_EVENTS] = {"/dev/events", 0, 0},
  [FD_DISPINFO] = {"/proc/dispinfo", 128, 0},
#include "files.h"
};

#define NR_FILES (sizeof(file_table) / sizeof(file_table[0]))

void init_fs() {
  file_table[FD_FB].size = _screen.width * _screen.height * sizeof(uint32_t);
  file_table[FD_DISPINFO].size = dispinfo_size();
}

int fs_open(const char *pathname, int flags, int mode) {
  (void)flags;
  (void)mode;

  for (int i = 0; i < NR_FILES; i ++) {
    if (strcmp(pathname, file_table[i].name) == 0) {
      file_table[i].open_offset = 0;
      return i;
    }
  }

  panic("Cannot open file: %s", pathname);
  return -1;
}

ssize_t fs_read(int fd, void *buf, size_t len) {
  assert(fd >= 0 && fd < NR_FILES);

  if (fd == FD_STDIN || fd == FD_STDOUT || fd == FD_STDERR)
    return 0;

  if (fd == FD_EVENTS)
  {
    static int event_read_log = 0;
    ssize_t ret = events_read(buf, len);
    if (event_read_log < 16) {
      unsigned char *p = buf;
      Log("fs_read /dev/events len=%u ret=%d bytes=%02x %02x %02x %02x %02x %02x %02x %02x",
          len, ret, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
      event_read_log ++;
    }
    return ret;
  }

  Finfo *file = &file_table[fd];

  if (fd == FD_DISPINFO) {
    size_t remain = file->size - file->open_offset;
    size_t read_len = len < remain ? len : remain;
    dispinfo_read(buf, file->open_offset, read_len);
    file->open_offset += read_len;
    return read_len;
  }

  assert(fd >= FD_NORMAL);
  size_t remain = file->size - file->open_offset;
  size_t read_len = len < remain ? len : remain;
  ramdisk_read(buf, file->disk_offset + file->open_offset, read_len);
  file->open_offset += read_len;
  return read_len;
}

ssize_t fs_write(int fd, const void *buf, size_t len) {
  assert(fd >= 0 && fd < NR_FILES);

  if (fd == FD_STDOUT || fd == FD_STDERR) {
    const char *p = buf;
    for (size_t i = 0; i < len; i ++) _putc(p[i]);
    return len;
  }

  if (fd == FD_FB) {
    fb_write(buf, file_table[fd].open_offset, len);
    file_table[fd].open_offset += len;
    return len;
  }

  assert(fd >= FD_NORMAL);
  Finfo *file = &file_table[fd];
  size_t remain = file->size - file->open_offset;
  size_t write_len = len < remain ? len : remain;
  ramdisk_write(buf, file->disk_offset + file->open_offset, write_len);
  file->open_offset += write_len;
  return write_len;
}

off_t fs_lseek(int fd, off_t offset, int whence) {
  assert(fd >= 0 && fd < NR_FILES);

  Finfo *file = &file_table[fd];
  off_t base = 0;
  switch (whence) {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = file->open_offset; break;
    case SEEK_END: base = file->size; break;
    default: panic("Invalid whence = %d", whence);
  }

  off_t new_offset = base + offset;
  assert(new_offset >= 0 && new_offset <= (off_t)file->size);
  file->open_offset = new_offset;
  return new_offset;
}

int fs_close(int fd) {
  assert(fd >= 0 && fd < NR_FILES);
  return 0;
}

size_t fs_filesz(int fd) {
  assert(fd >= 0 && fd < NR_FILES);
  return file_table[fd].size;
}
