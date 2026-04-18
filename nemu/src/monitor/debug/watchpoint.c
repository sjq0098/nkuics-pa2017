#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include <stdio.h>

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i + 1;
    wp_pool[i].next = &wp_pool[i + 1];
    wp_pool[i].expr[0] = '\0';
    wp_pool[i].old_val = 0;
    wp_pool[i].enabled = false;
  }
  wp_pool[NR_WP - 1].next = NULL;

  head = NULL;
  free_ = wp_pool;
}

WP* new_wp() {
  assert(free_ != NULL);

  WP *wp = free_;
  free_ = free_->next;

  wp->next = head;
  head = wp;
  wp->expr[0] = '\0';
  wp->old_val = 0;
  wp->enabled = true;

  return wp;
}

void free_wp(WP *wp) {
  if (head == wp) {
    head = head->next;
  }
  else {
    WP *p = head;
    while (p != NULL && p->next != wp) {
      p = p->next;
    }
    assert(p != NULL);
    p->next = wp->next;
  }

  wp->next = free_;
  free_ = wp;
  wp->expr[0] = '\0';
  wp->old_val = 0;
  wp->enabled = false;
}

WP* get_wp_head() {
  return head;
}

bool scan_watchpoint() {
  bool changed = false;

  for (WP *p = head; p != NULL; p = p->next) {
    if (!p->enabled) {
      continue;
    }

    bool success = true;
    uint32_t new_val = expr(p->expr, &success);
    if (!success) {
      continue;
    }

    if (new_val != p->old_val) {
      printf("Watchpoint %d: %s\n"
             "Old value = 0x%08x\n"
             "New value = 0x%08x\n",
             p->NO, p->expr, p->old_val, new_val);
      p->old_val = new_val;
      changed = true;
    }
  }

  return changed;
}

