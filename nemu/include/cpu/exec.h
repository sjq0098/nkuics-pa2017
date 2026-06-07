#ifndef __CPU_EXEC_H__
#define __CPU_EXEC_H__

#include "nemu.h"

#define make_EHelper(name) void concat(exec_, name) (vaddr_t *eip)
typedef void (*EHelper) (vaddr_t *);

#include "cpu/decode.h"

#ifdef JIT
/* Stage 1 instruction-fetch redirection: while a translation block is being
 * replayed, jit_code points at its cached guest bytes, so opcode/operand bytes
 * are read from host memory instead of paying vaddr_read + page-translate on
 * every execution.  Out-of-range fetches (e.g. an interrupt redirected eip out
 * of the block) transparently fall back to vaddr_read. */
extern const uint8_t *jit_code;
extern vaddr_t        jit_code_base;
extern uint32_t       jit_code_size;
#endif

static inline uint32_t instr_fetch(vaddr_t *eip, int len) {
  uint32_t instr;
#ifdef JIT
  if (jit_code != NULL && *eip >= jit_code_base &&
      (*eip - jit_code_base) + (uint32_t)len <= jit_code_size) {
    instr = 0;
    memcpy(&instr, jit_code + (*eip - jit_code_base), len);
  } else
#endif
  instr = vaddr_read(*eip, len);
#ifdef DEBUG
  uint8_t *p_instr = (void *)&instr;
  int i;
  for (i = 0; i < len; i ++) {
    decoding.p += sprintf(decoding.p, "%02x ", p_instr[i]);
  }
#endif
  (*eip) += len;
  return instr;
}

void rtl_setcc(rtlreg_t*, uint8_t);

static inline const char* get_cc_name(int subcode) {
  static const char *cc_name[] = {
    "o", "no", "b", "nb",
    "e", "ne", "be", "nbe",
    "s", "ns", "p", "np",
    "l", "nl", "le", "nle"
  };
  return cc_name[subcode];
}

#ifdef DEBUG
#define print_asm(...) Assert(snprintf(decoding.assembly, 80, __VA_ARGS__) < 80, "buffer overflow!")
#else
#define print_asm(...)
#endif

#define suffix_char(width) ((width) == 4 ? 'l' : ((width) == 1 ? 'b' : ((width) == 2 ? 'w' : '?')))

#define print_asm_template1(instr) \
  print_asm(str(instr) "%c %s", suffix_char(id_dest->width), id_dest->str)

#define print_asm_template2(instr) \
  print_asm(str(instr) "%c %s,%s", suffix_char(id_dest->width), id_src->str, id_dest->str)

#define print_asm_template3(instr) \
  print_asm(str(instr) "%c %s,%s,%s", suffix_char(id_dest->width), id_src->str, id_src2->str, id_dest->str)

#endif
