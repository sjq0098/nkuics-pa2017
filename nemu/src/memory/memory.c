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

paddr_t page_translate(vaddr_t addr, bool is_write) {
  if (cpu.cr0.paging == 0) {
    return addr;
  }

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

  return (pte.val & ~PAGE_MASK) | offset;
}

uint32_t vaddr_read(vaddr_t addr, int len) {
  if ((addr & PAGE_MASK) + len > PAGE_SIZE) {
    assert(0);
  }
  return paddr_read(page_translate(addr, false), len);
}

void vaddr_write(vaddr_t addr, int len, uint32_t data) {
  if ((addr & PAGE_MASK) + len > PAGE_SIZE) {
    assert(0);
  }
  paddr_write(page_translate(addr, true), len, data);
}
