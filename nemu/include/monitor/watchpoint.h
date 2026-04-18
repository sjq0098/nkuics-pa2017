#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  char expr[128];
  uint32_t old_val;
  bool enabled;

} WP;

void init_wp_pool();
WP* new_wp();
void free_wp(WP *wp);
WP* get_wp_head();
bool scan_watchpoint();

#endif
