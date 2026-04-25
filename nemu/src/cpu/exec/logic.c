#include "cpu/exec.h"

make_EHelper(test) {
  rtl_and(&t0, &id_dest->val, &id_src->val);
  rtl_li(&t1, 0);
  rtl_set_CF(&t1);
  rtl_set_OF(&t1);
  rtl_update_ZFSF(&t0, id_dest->width);

  print_asm_template2(test);
}

make_EHelper(and) {
  rtl_and(&t0, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t0);

  rtl_li(&t1, 0);
  rtl_set_CF(&t1);
  rtl_set_OF(&t1);
  rtl_update_ZFSF(&t0, id_dest->width);

  print_asm_template2(and);
}

make_EHelper(xor) {
  rtl_xor(&t0, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t0);

  rtl_li(&t1, 0);
  rtl_set_CF(&t1);
  rtl_set_OF(&t1);
  rtl_update_ZFSF(&t0, id_dest->width);

  print_asm_template2(xor);
}

make_EHelper(or) {
  rtl_or(&t0, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t0);

  rtl_li(&t1, 0);
  rtl_set_CF(&t1);
  rtl_set_OF(&t1);
  rtl_update_ZFSF(&t0, id_dest->width);

  print_asm_template2(or);
}

make_EHelper(rol) {
  rtl_andi(&t1, &id_src->val, 0x1f);
  int bits = id_dest->width * 8;
  int count = t1 % bits;
  if (count == 0) {
    rtl_mv(&t0, &id_dest->val);
  }
  else {
    uint32_t mask = (id_dest->width == 4 ? 0xffffffffu : ((1u << bits) - 1));
    uint32_t val = id_dest->val & mask;
    t0 = ((val << count) | (val >> (bits - count))) & mask;
  }
  operand_write(id_dest, &t0);

  print_asm_template2(rol);
}

make_EHelper(sar) {
  rtl_andi(&t1, &id_src->val, 0x1f);
  rtl_sar(&t0, &id_dest->val, &t1);
  operand_write(id_dest, &t0);
  rtl_update_ZFSF(&t0, id_dest->width);
  // unnecessary to update CF and OF in NEMU

  print_asm_template2(sar);
}

make_EHelper(shl) {
  rtl_andi(&t1, &id_src->val, 0x1f);
  rtl_shl(&t0, &id_dest->val, &t1);
  operand_write(id_dest, &t0);
  rtl_update_ZFSF(&t0, id_dest->width);
  // unnecessary to update CF and OF in NEMU

  print_asm_template2(shl);
}

make_EHelper(shr) {
  rtl_andi(&t1, &id_src->val, 0x1f);
  rtl_shr(&t0, &id_dest->val, &t1);
  operand_write(id_dest, &t0);
  rtl_update_ZFSF(&t0, id_dest->width);
  // unnecessary to update CF and OF in NEMU

  print_asm_template2(shr);
}

make_EHelper(setcc) {
  uint8_t subcode = decoding.opcode & 0xf;
  rtl_setcc(&t2, subcode);
  operand_write(id_dest, &t2);

  print_asm("set%s %s", get_cc_name(subcode), id_dest->str);
}

make_EHelper(not) {
  rtl_not(&id_dest->val);
  operand_write(id_dest, &id_dest->val);

  print_asm_template1(not);
}
