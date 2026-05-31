#include "common.h"

/* Uncomment these macros to enable corresponding functionality. */
#define HAS_ASYE
#define HAS_PTE

void init_mm(void);
void init_ramdisk(void);
void init_device(void);
void init_irq(void);
void init_fs(void);
void load_prog(const char *);

int main() {
#ifdef HAS_PTE
  init_mm();
#endif

  Log("'Hello World!' from Nanos-lite");
  Log("Build time: %s, %s", __TIME__, __DATE__);

  init_ramdisk();

  init_device();

#ifdef HAS_ASYE
  Log("Initializing interrupt/exception handler...");
  init_irq();
#endif

  init_fs();

  load_prog("/bin/pal");        /* pcb[0]：前台图形程序（初始） */
  load_prog("/bin/hello");     /* pcb[1]：后台 console 程序 */
  load_prog("/bin/videotest"); /* pcb[2]：备用前台图形程序 */

  _trap();

  panic("Should not reach here");
}
