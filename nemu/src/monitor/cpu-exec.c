#include "nemu.h"
#include "monitor/monitor.h"
#include "monitor/watchpoint.h"
#include "cpu/jit.h"

/* The assembly code of instructions executed is only output to the screen
 * when the number of instructions executed is less than this value.
 * This is useful when you use the `si' command.
 * You can modify this value as you want.
 */
#define MAX_INSTR_TO_PRINT 10

int nemu_state = NEMU_STOP;

void exec_wrapper(bool);

/* Simulate how the CPU works. */
void cpu_exec(uint64_t n) {
  if (nemu_state == NEMU_END) {
    printf("Program execution has ended. To restart the program, exit NEMU and run again.\n");
    return;
  }
  nemu_state = NEMU_RUNNING;

  bool print_flag = n < MAX_INSTR_TO_PRINT;

#ifdef JIT
  /* JIT mode: same per-instruction semantics, but grouped into TBs by the
   * dispatcher (Stage 0 still interprets every instruction). */
  jit_exec(n, print_flag);
#else
  for (; n > 0; n --) {
    /* Execute one instruction, including instruction fetch,
     * instruction decode, and the actual execution. */
    exec_wrapper(print_flag);

#ifdef DEBUG
    if (scan_watchpoint()) {
      nemu_state = NEMU_STOP;
    }
#endif

#ifdef HAS_IOE
    extern void device_update();
    device_update();
#endif

    if (nemu_state != NEMU_RUNNING) { return; }
  }
#endif

  if (nemu_state == NEMU_RUNNING) { nemu_state = NEMU_STOP; }
}
