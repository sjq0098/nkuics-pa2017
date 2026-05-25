#include "cpu/exec.h"
#include "memory/mmu.h"

void raise_intr(uint8_t NO, vaddr_t ret_addr) {
  vaddr_t desc_addr = cpu.idtr.base + NO * 8;
  assert(NO * 8 + 7 <= cpu.idtr.limit);

  uint32_t desc_low = vaddr_read(desc_addr, 4);
  uint32_t desc_high = vaddr_read(desc_addr + 4, 4);
  vaddr_t handler = (desc_low & 0xffff) | (desc_high & 0xffff0000);

  rtlreg_t val = cpu.eflags.val;
  rtl_push(&val);
  cpu.eflags.IF = 0;
  val = cpu.cs;
  rtl_push(&val);
  val = ret_addr;
  rtl_push(&val);

  cpu.cs = desc_low >> 16;
  decoding.jmp_eip = handler;
  decoding.is_jmp = 1;
}

void dev_raise_intr() {
  cpu.INTR = true;
}
