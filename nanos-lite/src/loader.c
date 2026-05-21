#include "fs.h"
#include "memory.h"

#define DEFAULT_ENTRY ((void *)0x8048000)

uintptr_t loader(_Protect *as, const char *filename) {
  assert(as != NULL);

  int fd = fs_open(filename, 0, 0);
  size_t size = fs_filesz(fd);
  uintptr_t va = (uintptr_t)DEFAULT_ENTRY;
  size_t offset = 0;

  while (offset < size) {
    void *pa = new_page();
    _map(as, (void *)va, pa);
    memset(pa, 0, PGSIZE);

    size_t read_len = size - offset < PGSIZE ? size - offset : PGSIZE;
    fs_lseek(fd, offset, SEEK_SET);
    ssize_t nread = fs_read(fd, pa, read_len);
    assert((size_t)nread == read_len);

    va += PGSIZE;
    offset += read_len;
  }

  fs_close(fd);

  return (uintptr_t)DEFAULT_ENTRY;
}
