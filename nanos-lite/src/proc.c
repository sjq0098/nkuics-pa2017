#include "proc.h"

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC];
static int nr_proc = 0;
PCB *current = NULL;
PCB *fg_pcb = NULL;  /* 当前前台图形进程（pcb[0]=pal 或 pcb[2]=videotest） */

uintptr_t loader(_Protect *as, const char *filename);

void switch_fg(void) {
  if (fg_pcb == &pcb[0]) fg_pcb = &pcb[2];
  else                   fg_pcb = &pcb[0];
}

void load_prog(const char *filename) {
  int i = nr_proc ++;
  _protect(&pcb[i].as);

  uintptr_t entry = loader(&pcb[i].as, filename);

  _Area stack;
  stack.start = pcb[i].stack;
  stack.end = stack.start + sizeof(pcb[i].stack);

  pcb[i].tf = _umake(&pcb[i].as, stack, stack, (void *)entry, NULL, NULL);
  if (fg_pcb == NULL) fg_pcb = &pcb[i];  /* 第一个加载的程序作为初始前台 */
}

_RegSet* schedule(_RegSet *prev) {
  assert(nr_proc > 0);

  if (current != NULL) {
    current->tf = prev;
  }

  int next = (current == NULL) ? 0 : (current - pcb + 1) % nr_proc;
  /* 跳过非前台的图形进程（pcb[0]=pal，pcb[2]=videotest，只运行 fg_pcb 那个） */
  if (fg_pcb != NULL) {
    PCB *skip = (fg_pcb == &pcb[0]) ? &pcb[2] : &pcb[0];
    if (&pcb[next] == skip)
      next = (next + 1) % nr_proc;
  }
  current = &pcb[next];
  _switch(&current->as);
  return current->tf;
}
