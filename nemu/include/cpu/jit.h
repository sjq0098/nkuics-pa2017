#ifndef __CPU_JIT_H__
#define __CPU_JIT_H__

#include "common.h"

#ifdef JIT

/* Upper bound on guest instructions per translation block (basic block). */
#define MAX_BB_INSN 32

/* Translation Block: a basic block keyed by its starting guest eip.
 *
 * Stage 0 only records the block extent (n_insn) so the dispatcher can group
 * the interpreter into blocks and exercise the cache/hash/flush pipeline with
 * ZERO change in execution semantics.  Stage 1 will add a predecoded
 * instruction array here so cache hits can skip instruction fetch and decode. */
typedef struct TB {
  vaddr_t  guest_eip;    // key: basic-block start
  uint32_t n_insn;       // # of guest instructions (0 == not discovered yet)
  uint32_t guest_size;   // bytes covered by the block (used by Stage 1 / flush)
  struct TB *hash_next;  // chaining within a hash bucket
  /* --- Stage 1 (decode cache) fields will be added here --- */
} TB;

/* JIT dispatcher main loop: runs up to n guest instructions, grouping them into
 * TBs.  Drop-in replacement for the interpreter loop body of cpu_exec(). */
void jit_exec(uint64_t n, bool print_flag);

/* Invalidate the whole TB cache (call when guest code may have changed, e.g.
 * nanos-lite loads a new program -- needed once Stage 1 caches decode results). */
void tb_flush_all(void);

#endif

#endif
