#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>

enum {
  TK_NOTYPE = 256,
  TK_NUM,
  TK_HEX,
  TK_REG,
  TK_EQ,
  TK_NEQ,
  TK_AND,
  TK_OR,
  TK_DEREF,
  TK_NEG

  /* TODO: Add more token types */

};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},             // spaces
  {"==", TK_EQ},                 // equal
  {"!=", TK_NEQ},                // not equal
  {"&&", TK_AND},                // logic and
  {"\\|\\|", TK_OR},             // logic or
  {"0[xX][0-9a-fA-F]+", TK_HEX}, // hexadecimal number
  {"[0-9]+", TK_NUM},            // decimal number
  {"\\$[a-zA-Z]+", TK_REG},      // register
  {"\\+", '+'},                  // plus
  {"-", '-'},                    // minus
  {"\\*", '*'},                  // multiply or dereference
  {"/", '/'},                    // divide
  {"\\(", '('},                  // left parenthesis
  {"\\)", ')'},                  // right parenthesis
  {"!", '!'}                     // logic not
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

Token tokens[32];
int nr_token;

static bool is_operand_end(int type) {
  return type == TK_NUM || type == TK_HEX || type == TK_REG || type == ')';
}

static bool is_right_associative(int type) {
  return type == '!' || type == TK_DEREF || type == TK_NEG;
}

static uint32_t eval_register(const char *name, bool *success) {
  int i;
  for (i = 0; i < 8; i ++) {
    if (strcmp(name, regsl[i]) == 0) {
      return reg_l(i);
    }
    if (strcmp(name, regsw[i]) == 0) {
      return reg_w(i);
    }
  }

  for (i = 0; i < 8; i ++) {
    if (strcmp(name, regsb[i]) == 0) {
      return reg_b(i);
    }
  }

  if (strcmp(name, "eip") == 0) {
    return cpu.eip;
  }

  *success = false;
  return 0;
}

static bool check_parentheses(int p, int q) {
  if (tokens[p].type != '(' || tokens[q].type != ')') {
    return false;
  }

  int balance = 0;
  int i;
  for (i = p; i <= q; i ++) {
    if (tokens[i].type == '(') {
      balance ++;
    }
    else if (tokens[i].type == ')') {
      balance --;
      if (balance < 0) {
        return false;
      }
      if (balance == 0 && i < q) {
        return false;
      }
    }
  }

  return balance == 0;
}

static int precedence(int type) {
  switch (type) {
    case TK_OR: return 1;
    case TK_AND: return 2;
    case TK_EQ:
    case TK_NEQ: return 3;
    case '+':
    case '-': return 4;
    case '*':
    case '/': return 5;
    case '!':
    case TK_DEREF:
    case TK_NEG: return 6;
    default: return 0;
  }
}

static int find_dominant_op(int p, int q, bool *success) {
  int balance = 0;
  int op = -1;
  int min_precedence = 0x7fffffff;
  int i;

  for (i = p; i <= q; i ++) {
    if (tokens[i].type == '(') {
      balance ++;
      continue;
    }

    if (tokens[i].type == ')') {
      balance --;
      if (balance < 0) {
        *success = false;
        return -1;
      }
      continue;
    }

    if (balance != 0) {
      continue;
    }

    int cur_precedence = precedence(tokens[i].type);
    if (cur_precedence != 0 &&
        (cur_precedence < min_precedence ||
         (cur_precedence == min_precedence && !is_right_associative(tokens[i].type)))) {
      min_precedence = cur_precedence;
      op = i;
    }
  }

  if (balance != 0 || op == -1) {
    *success = false;
    return -1;
  }

  return op;
}

static uint32_t eval(int p, int q, bool *success) {
  if (p > q) {
    *success = false;
    return 0;
  }
  else if (p == q) {
    switch (tokens[p].type) {
      case TK_NUM: return strtoul(tokens[p].str, NULL, 10);
      case TK_HEX: return strtoul(tokens[p].str, NULL, 16);
      case TK_REG: return eval_register(tokens[p].str + 1, success);
      default: assert(0);
    }
  }
  else if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, success);
  }
  else {
    int op = find_dominant_op(p, q, success);
    if (!*success) {
      return 0;
    }

    switch (tokens[op].type) {
      case '!': {
        uint32_t val = eval(op + 1, q, success);
        if (!*success) {
          return 0;
        }
        return !val;
      }
      case TK_DEREF: {
        uint32_t addr = eval(op + 1, q, success);
        if (!*success) {
          return 0;
        }
        return vaddr_read(addr, 4);
      }
      case TK_NEG: {
        uint32_t val = eval(op + 1, q, success);
        if (!*success) {
          return 0;
        }
        return -val;
      }
      default: break;
    }

    uint32_t val1 = eval(p, op - 1, success);
    if (!*success) {
      return 0;
    }

    uint32_t val2 = eval(op + 1, q, success);
    if (!*success) {
      return 0;
    }

    switch (tokens[op].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/':
        if (val2 == 0) {
          *success = false;
          return 0;
        }
        return val1 / val2;
      case TK_EQ: return val1 == val2;
      case TK_NEQ: return val1 != val2;
      case TK_AND: return val1 && val2;
      case TK_OR: return val1 || val2;
      default: assert(0);
    }
  }
}

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);
        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NOTYPE:
            break;
          case TK_NUM:
          case TK_HEX:
          case TK_REG:
            assert(substr_len < sizeof(tokens[nr_token].str));
            tokens[nr_token].type = rules[i].token_type;
            memcpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token ++;
            break;
          default:
            tokens[nr_token].type = rules[i].token_type;
            tokens[nr_token].str[0] = '\0';
            nr_token ++;
            break;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  int j;
  for (j = 0; j < nr_token; j ++) {
    if (tokens[j].type == '*') {
      if (j == 0 || !is_operand_end(tokens[j - 1].type)) {
        tokens[j].type = TK_DEREF;
      }
    }
    else if (tokens[j].type == '-') {
      if (j == 0 || !is_operand_end(tokens[j - 1].type)) {
        tokens[j].type = TK_NEG;
      }
    }
  }

  return true;
}

uint32_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  *success = true;
  return eval(0, nr_token - 1, success);
}
