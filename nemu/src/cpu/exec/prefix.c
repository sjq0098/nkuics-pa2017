#include "cpu/exec.h"

make_EHelper(real);

make_EHelper(operand_size) {
  decoding.is_operand_size_16 = true;
  exec_real(eip);
  decoding.is_operand_size_16 = false;
}

make_EHelper(rep) {
  /* REP prefix: transparent for non-string instructions (e.g. ENDBR32 = f3 0f 1e fb) */
  exec_real(eip);
}
