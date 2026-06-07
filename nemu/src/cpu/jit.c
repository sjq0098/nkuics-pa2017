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

/* ---- instruction-fetch redirection (read by instr_fetch() in cpu/exec.h) ---- */
const uint8_t *jit_code      = NULL;   // cached bytes of the block being replayed
vaddr_t        jit_code_base = 0;      // guest eip the cached bytes start at
uint32_t       jit_code_size = 0;      // # of cached bytes

/* ======================= TB cache (hash table + pools) ======================= */

#define TB_HASH_BITS 16
#define TB_HASH_SIZE (1u << TB_HASH_BITS)
#define TB_POOL_SIZE TB_HASH_SIZE
#define TB_CODE_POOL_SIZE (8u << 20)   // 8 MB of cached guest code bytes

static TB *tb_hash[TB_HASH_SIZE];
static TB  tb_pool[TB_POOL_SIZE];
static uint32_t tb_pool_used;
static uint8_t  tb_code_pool[TB_CODE_POOL_SIZE];
static uint32_t tb_code_used;

static inline uint32_t tb_hash_func(vaddr_t eip) {
  return (eip ^ (eip >> 13)) & (TB_HASH_SIZE - 1);
}

/* Invalidate the whole cache.  Called when guest code at a cached eip may have
 * changed -- here that means an address-space switch (CR3 write, see system.c),
 * which is how nanos-lite brings a freshly loaded program online (§7.2). */
void tb_flush_all(void) {
  memset(tb_hash, 0, sizeof(tb_hash));
  tb_pool_used = 0;
  tb_code_used = 0;
}

static TB *tb_find(vaddr_t eip) {
  for (TB *tb = tb_hash[tb_hash_func(eip)]; tb != NULL; tb = tb->hash_next) {
    if (tb->guest_eip == eip) return tb;
  }
  return NULL;
}

static TB *tb_new(vaddr_t eip) {
  if (tb_pool_used >= TB_POOL_SIZE) tb_flush_all();   // simplest reclaim policy

  TB *tb = &tb_pool[tb_pool_used ++];
  tb->guest_eip = eip;
  tb->n_insn = 0;
  tb->guest_size = 0;
  tb->code = NULL;

  uint32_t h = tb_hash_func(eip);
  tb->hash_next = tb_hash[h];
  tb_hash[h] = tb;
  return tb;
}

/* Snapshot the block's guest code bytes into the code pool so later runs can
 * fetch them without page translation.  The bytes were just executed, hence
 * mapped and readable; reading them again is a one-time cost per TB. */
static void tb_capture_code(TB *tb) {
  uint32_t sz = tb->guest_size;
  if (sz == 0 || tb_code_used + sz > TB_CODE_POOL_SIZE) return;  // leave un-cached

  uint8_t *buf = &tb_code_pool[tb_code_used];
  tb_code_used += sz;
  for (uint32_t i = 0; i < sz; i ++) {
    buf[i] = vaddr_read(tb->guest_eip + i, 1);
  }
  tb->code = buf;
}

/* ======================= basic-block boundary ======================= */

/* Whether the instruction just executed ends a basic block, decided from the
 * decoded opcode (static -> stable, cacheable block length).  The dispatcher
 * additionally ends a block when control flow diverged unexpectedly (e.g. an
 * asynchronous interrupt), so this need not be exhaustive to stay correct. */
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

    if (tb->code != NULL) {
      /* ---------- cache hit: replay, fetching code bytes from the cache ---------- */
      jit_code      = tb->code;
      jit_code_base = tb->guest_eip;
      jit_code_size = tb->guest_size;

      uint32_t i = 0;
      while (i < tb->n_insn && n > 0) {
        exec_one(print_flag);
        n --; i ++;
        if (nemu_state != NEMU_RUNNING) break;
      }

      jit_code = NULL;   // back to normal fetch outside the block
    } else {
      /* ------ cache miss: interpret to discover the block, then snapshot it ------ */
      uint32_t i = 0;
      bool ended = false;
      while (n > 0) {
        exec_one(print_flag);
        n --; i ++;
        if (nemu_state != NEMU_RUNNING) return;

        /* End the block on a static terminator, on unexpected control-flow
         * divergence (interrupt/exception: cpu.eip != next sequential eip),
         * on the length cap, or at a page boundary. */
        if (is_terminator() || cpu.eip != decoding.seq_eip ||
            i >= MAX_BB_INSN ||
            (cpu.eip & ~PAGE_MASK) != (tb->guest_eip & ~PAGE_MASK)) {
          tb->n_insn = i;
          tb->guest_size = decoding.seq_eip - tb->guest_eip;  // contiguous block bytes
          ended = true;
          break;
        }
      }
      if (ended) tb_capture_code(tb);
    }
  }
}

#endif
