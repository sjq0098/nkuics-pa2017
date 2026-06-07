#include "cpu/jit.h"

#ifdef JIT

#include "nemu.h"
#include "cpu/decode.h"
#include "monitor/monitor.h"
#include "memory/mmu.h"

/* ---- interpreter hooks reused by the JIT dispatcher ---- */
void exec_wrapper(bool);
#ifdef HAS_IOE
void device_update();
#endif

/* ======================= TB cache (hash table + pool) ======================= */

#define TB_HASH_BITS 16
#define TB_HASH_SIZE (1u << TB_HASH_BITS)
#define TB_POOL_SIZE TB_HASH_SIZE

static TB *tb_hash[TB_HASH_SIZE];
static TB  tb_pool[TB_POOL_SIZE];
static uint32_t tb_pool_used;

static inline uint32_t tb_hash_func(vaddr_t eip) {
  return (eip ^ (eip >> 13)) & (TB_HASH_SIZE - 1);
}

void tb_flush_all(void) {
  memset(tb_hash, 0, sizeof(tb_hash));
  tb_pool_used = 0;
}

static TB *tb_find(vaddr_t eip) {
  for (TB *tb = tb_hash[tb_hash_func(eip)]; tb != NULL; tb = tb->hash_next) {
    if (tb->guest_eip == eip) return tb;
  }
  return NULL;
}

static TB *tb_new(vaddr_t eip) {
  /* simplest reclaim policy: when the pool is exhausted, flush everything */
  if (tb_pool_used >= TB_POOL_SIZE) tb_flush_all();

  TB *tb = &tb_pool[tb_pool_used ++];
  tb->guest_eip = eip;
  tb->n_insn = 0;
  tb->guest_size = 0;

  uint32_t h = tb_hash_func(eip);
  tb->hash_next = tb_hash[h];
  tb_hash[h] = tb;
  return tb;
}

/* ======================= basic-block boundary ======================= */

/* Whether the instruction just executed ends a basic block.  Decided purely
 * from the decoded opcode (static), so it is independent of branch outcome and
 * of asynchronous interrupts -- both required for a stable, cacheable block
 * length.  `decoding` still holds the last instruction's opcode here. */
static inline bool is_terminator(void) {
  uint32_t op = decoding.opcode;
  if (op >= 0x70 && op <= 0x7f)   return true;  // jcc rel8
  if (op >= 0x180 && op <= 0x18f) return true;  // jcc rel32 (0f 8x)
  switch (op) {
    case 0xe8:                                  // call rel
    case 0xe9: case 0xeb:                       // jmp rel
    case 0xc3:                                  // ret
    case 0xcd:                                  // int imm8
    case 0xcf:                                  // iret
    case 0xd6:                                  // nemu_trap (hlt)
      return true;
    case 0xff:                                  // gp5: /2 call /3 callf /4 jmp /5 jmpf
      return decoding.ext_opcode >= 2 && decoding.ext_opcode <= 5;
    default:
      return false;
  }
}

/* ======================= dispatcher ======================= */

/* One interpreter step, identical to the body of the original cpu_exec loop. */
static inline void exec_one(bool print_flag) {
  exec_wrapper(print_flag);
#ifdef DEBUG
  if (scan_watchpoint()) { nemu_state = NEMU_STOP; }
#endif
#ifdef HAS_IOE
  device_update();
#endif
}

void jit_exec(uint64_t n, bool print_flag) {
  while (n > 0 && nemu_state == NEMU_RUNNING) {
    TB *tb = tb_find(cpu.eip);
    if (tb == NULL) tb = tb_new(cpu.eip);

    uint32_t limit = tb->n_insn;   // 0 == length not discovered yet
    uint32_t i = 0;

    while (n > 0) {
      exec_one(print_flag);
      n --; i ++;
      if (nemu_state != NEMU_RUNNING) return;

      if (limit != 0) {
        /* cache hit: block length already known */
        if (i >= limit) break;
      } else {
        /* discovering the block on first encounter */
        if (is_terminator())   { tb->n_insn = i; break; }
        if (i >= MAX_BB_INSN)  { tb->n_insn = i; break; }
        if ((cpu.eip & ~PAGE_MASK) != (tb->guest_eip & ~PAGE_MASK)) {
          tb->n_insn = i; break;   // page boundary: truncate the block
        }
      }
    }
  }
}

#endif
