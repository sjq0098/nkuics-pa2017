#include "nemu.h"

#ifdef HAS_IOE
#include "device/mmio.h"
#endif

#define PMEM_SIZE (128 * 1024 * 1024)

#define pmem_rw(addr, type) *(type *)({\
    Assert(addr < PMEM_SIZE, "physical address(0x%08x) is out of bound", addr); \
    guest_to_host(addr); \
    })

uint8_t pmem[PMEM_SIZE];

/* Memory accessing interfaces */

uint32_t paddr_read(paddr_t addr, int len) {
#ifdef HAS_IOE
  int map_no = is_mmio(addr);
  if (map_no != -1) {
    return mmio_read(addr, len, map_no);
  }
#endif
  return pmem_rw(addr, uint32_t) & (~0u >> ((4 - len) << 3));
}

void paddr_write(paddr_t addr, int len, uint32_t data) {
#ifdef HAS_IOE
  int map_no = is_mmio(addr);
  if (map_no != -1) {
    mmio_write(addr, len, data, map_no);
    return;
  }
#endif
  memcpy(guest_to_host(addr), &data, len);
}

#ifdef JIT
/* Software TLB (part of the JIT fast-path engine): cache vpage -> ppage so a
 * guest memory access skips the two-level page walk (4 paddr ops + is_mmio each)
 * that otherwise dominates run time once paging is on.  Flushed on CR3 writes
 * (address-space switch, see exec/system.c); within one address space nanos-lite
 * only ever *adds* mappings, so cached entries never go stale. */
#define TLB_SIZE (1u << 12)
typedef struct { uint32_t tag; paddr_t ppbase; bool valid; } TLBEntry;
static TLBEntry tlb[TLB_SIZE];

void tlb_flush(void) {
  for (uint32_t i = 0; i < TLB_SIZE; i ++) tlb[i].valid = false;
}
#endif

paddr_t page_translate(vaddr_t addr, bool is_write) {
  if (cpu.cr0.paging == 0) {
    return addr;
  }

#ifdef JIT
  uint32_t vpn = addr >> 12;
  uint32_t tlb_idx = vpn & (TLB_SIZE - 1);
  if (tlb[tlb_idx].valid && tlb[tlb_idx].tag == vpn) {
    return tlb[tlb_idx].ppbase | (addr & PAGE_MASK);
  }
#endif

  uint32_t pdir_idx = addr >> 22;
  uint32_t ptab_idx = (addr >> 12) & 0x3ff;
  uint32_t offset = addr & PAGE_MASK;

  paddr_t pdir_base = cpu.cr3.val & ~PAGE_MASK;
  paddr_t pde_addr = pdir_base + pdir_idx * 4;
  PDE pde;
  pde.val = paddr_read(pde_addr, 4);
  Assert(pde.present, "PDE not present, vaddr = 0x%08x, pdir_idx = %u", addr, pdir_idx);
  pde.accessed = 1;
  paddr_write(pde_addr, 4, pde.val);

  paddr_t ptab_base = pde.val & ~PAGE_MASK;
  paddr_t pte_addr = ptab_base + ptab_idx * 4;
  PTE pte;
  pte.val = paddr_read(pte_addr, 4);
  Assert(pte.present, "PTE not present, vaddr = 0x%08x, ptab_idx = %u", addr, ptab_idx);
  pte.accessed = 1;
  if (is_write) {
    pte.dirty = 1;
  }
  paddr_write(pte_addr, 4, pte.val);

  paddr_t ppbase = pte.val & ~PAGE_MASK;
#ifdef JIT
  tlb[tlb_idx].tag = vpn;
  tlb[tlb_idx].ppbase = ppbase;
  tlb[tlb_idx].valid = true;
#endif
  return ppbase | offset;
}

uint32_t vaddr_read(vaddr_t addr, int len) {
  int first_len = PAGE_SIZE - (addr & PAGE_MASK);
  if (first_len >= len) {
    return paddr_read(page_translate(addr, false), len);
  }

  int second_len = len - first_len;
  uint32_t data = paddr_read(page_translate(addr, false), first_len);
  uint32_t rest = paddr_read(page_translate(addr + first_len, false), second_len);
  return data | (rest << (first_len << 3));
}

void vaddr_write(vaddr_t addr, int len, uint32_t data) {
  int first_len = PAGE_SIZE - (addr & PAGE_MASK);
  if (first_len >= len) {
    paddr_write(page_translate(addr, true), len, data);
    return;
  }

  int second_len = len - first_len;
  paddr_write(page_translate(addr, true), first_len, data);
  paddr_write(page_translate(addr + first_len, true), second_len, data >> (first_len << 3));
}
