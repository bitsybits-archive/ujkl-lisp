#ifndef UJKL_H
#define UJKL_H

// #define UJKL_DEBUG
// #define TRACE
// #define TRACE_FULL

#ifdef __cplusplus
extern "C" {
#endif

#include "sources/types.h"

#include "sources/data.c"
#include "sources/lists.c"
#include "sources/tables.c"
#include "sources/iter.c"
#include "sources/runtime.c"
#include "sources/symbols.c"
#include "sources/print.c"
#include "sources/dump.c"

#define VM_VERSION "MonkeyRocker"
#define SYMBOLS_BLOCK_SIZE 128
#define PAIRS_BLOCK_SIZE 16

#undef API
#define API static

API value_t repl;

// Look for dots and parse into list of symbols if found.
API value_t getSymbols(const char* start, const char* end) {
  value_t parts = Nil;
  const char* s = start;
  const char* i = s;
  while (i < end) {
    if (*i == '.' && i > s) {
      value_t sym = SymbolRange(s, i);
      parts = cons(sym, parts);
      s = i + 1;
    }
    i++;
  }
  if (isNil(parts)) return SymbolRange(start, end);
  return list_ireverse(cons(SymbolRange(s,end), parts));
}

API void parse(const char *data) {
  value_t stack = Nil;
  value_t value = Nil;
  bool quote = false;
  bool neg = false;
  while (*data) {
    // Skip whitespace
    if (*data == ' ') {
      data++;
    }
    // Push stack when on open paren
    else if (*data == '(' || *data == '[') {
      stack = cons(value, stack);
      value = Nil;
      if (quote) {
        quote = false;
        value = cons(quoteSym, value);
      }
      if (*data == '[') {
        value = cons(listSym, value);
      }
      data++;
    }
    // pop stack on close paren
    else if (*data == ')' || *data == ']') {
      value_t fixed = Nil;
      bool dot = false;
      while (value.type == PairType) {
        pair_t pair = get_pair(value);
        if (eq(pair.left, Dot)) {
          dot = true;
        }
        else {
          if (dot) {
            dot = false;
            fixed = free_cell(fixed).left;
          }
          set_cdr(value, fixed);
          fixed = value;
        }
        value = pair.right;
      }
      value = cons(fixed, car(stack));
      stack = free_cell(stack).right;
      data++;
    }
    else if (*data == '\'') {
      quote = true;
      data++;
    }
    else if (*data == '-' && *(data + 1) &&
             *(data + 1) >= '0' && *(data + 1) <= '9') {
      neg = true;
      data++;
    }
    else if (*data >= '0' && *data <= '9') {
      int num = 0;
      while (*data >= '0' && *data <= '9') {
        num = num * 10 + *data - '0';
        data++;
      }
      if (neg) {
        neg = false;
        num = -num;
      }
      value_t atom = Integer(num);
      if (quote) {
        quote = false;
        atom = cons(quoteSym, atom);
      }
      value = cons(atom, value);
    }
    else if (*data == '"') {
      const char* start = ++data;
      while (*data && *data != '"') data++;
      value = cons(cons(quoteSym, SymbolRange(start, data++)), value);
    }
    else {
      const char* start = data;
      while (*data && *data != ' ' &&
             *data != '(' && *data != ')' &&
             *data != '[' && *data != ']') {
        data++;
      }
      value_t atom;
      int len = data - start;
      if (len == 1 && start[0] == '.') {
        atom = Dot;
      }
      else if (len == 3 &&
               start[0] == 'n' &&
               start[1] == 'i' &&
               start[2] == 'l') {
        atom = Nil;
      }
      else if (len == 4 &&
               start[0] == 't' &&
               start[1] == 'r' &&
               start[2] == 'u' &&
               start[3] == 'e') {
        atom = True;
      }
      else if (len == 5 &&
               start[0] == 'f' &&
               start[1] == 'a' &&
               start[2] == 'l' &&
               start[3] == 's' &&
               start[4] == 'e') {
        atom = False;
      }
      else {
        atom = getSymbols(start, data);
      }
      if (quote) {
        quote = false;
        atom = cons(quoteSym, atom);
      }
      value = cons(atom, value);
    }
  }
  stack = free_list(stack);
  value = list_ireverse(value);

#ifdef UJKL_DEBUG
  print("> ");
  dump_line(value);
#endif // UJKL_DEBUG

  value_t parts = value;
  while (value.type == PairType) {
    value_t expr = next(&value);
#ifdef TRACE
    full_dump(eval(repl, expr));
#else
    dump(eval(repl, expr));
#endif // TRACE
    // free_list(expr);   // Hmmm, if we free this list we get a memory leak
  }
  free_list(parts);
  int freed = collectgarbage(repl);

#ifdef UJKL_DEBUG
  print("gc: ");
  print_int(freed);
  print_char('\n');
#endif // UJKL_DEBUG
}

// Passes values through unaffected
API value_t _quote(value_t args) {
  return cdr(args);
}

// Evals values, but otherwise passes them through
API value_t _list(value_t args) {
  return args;
}

API value_t _cons(value_t args) {
  value_t a = next(&args);
  value_t b = next(&args);
  return cons(a, b);
}

API value_t _car(value_t args) {
  value_t a = next(&args);
  return car(a);
}

API value_t _cdr(value_t args) {
  value_t a = next(&args);
  return cdr(a);
}

API value_t _set_car(value_t args) {
  value_t a = next(&args);
  value_t b = next(&args);
  return Bool(set_car(a, b));
}

API value_t _set_cdr(value_t args) {
  value_t a = next(&args);
  value_t b = next(&args);
  return Bool(set_cdr(a, b));
}

API value_t _add(value_t args) {
  int sum = 0;
  while (args.type == PairType) {
    value_t value = next(&args);
    if (value.type != IntegerType) return TypeError;
    sum += value.data;
  }
  return Integer(sum);
}

API value_t _sub(value_t args) {
  value_t value = next(&args);
  if (value.type != IntegerType) return TypeError;
  int sum = value.data;
  while (args.type == PairType) {
    value = next(&args);
    if (value.type != IntegerType) return TypeError;
    sum -= value.data;
  }
  return Integer(sum);
}

API value_t _mul(value_t args) {
  int product = 1;
  while (args.type == PairType) {
    value_t value = next(&args);
    if (value.type != IntegerType) return TypeError;
    product *= value.data;
  }
  return Integer(product);
}

API value_t _div(value_t args) {
  value_t value = next(&args);
  if (value.type != IntegerType) return TypeError;
  int product = value.data;
  while (args.type == PairType) {
    value = next(&args);
    if (value.type != IntegerType) return TypeError;
    product /= value.data;
  }
  return Integer(product);
}

API value_t _mod(value_t args) {
  value_t a = next(&args);
  value_t b = next(&args);
  if (a.type != IntegerType || b.type != IntegerType) {
    return TypeError;
  }
  return Integer(a.data % b.data);
}

API value_t _lt(value_t args) {
  value_t value = next(&args);
  if (value.type != IntegerType) return TypeError;
  int last = value.data;
  while (args.type == PairType) {
    value = next(&args);
    if (value.type != IntegerType) return TypeError;
    if (last >= value.data) return False;
    last = value.data;
  }
  return True;
}

API value_t _lte(value_t args) {
  value_t value = next(&args);
  if (value.type != IntegerType) return TypeError;
  int last = value.data;
  while (args.type == PairType) {
    value = next(&args);
    if (value.type != IntegerType) return TypeError;
    if (last > value.data) return False;
    last = value.data;
  }
  return True;
}

API value_t _gt(value_t args) {
  value_t value = next(&args);
  if (value.type != IntegerType) return TypeError;
  int last = value.data;
  while (args.type == PairType) {
    value = next(&args);
    if (value.type != IntegerType) return TypeError;
    if (last <= value.data) return False;
    last = value.data;
  }
  return True;
}

API value_t _gte(value_t args) {
  value_t value = next(&args);
  if (value.type != IntegerType) return TypeError;
  int last = value.data;
  while (args.type == PairType) {
    value = next(&args);
    if (value.type != IntegerType) return TypeError;
    if (last < value.data) return False;
    last = value.data;
  }
  return True;
}

API value_t _eq(value_t args) {
  value_t value = next(&args);
  while (args.type == PairType) {
    if (!eq(value, next(&args))) return False;
  }
  return True;
}

API value_t _neq(value_t args) {
  value_t value = next(&args);
  while (args.type == PairType) {
    if (eq(value, next(&args))) return False;
  }
  return True;
}

API value_t _print(value_t args) {
  dump_line(args);
  return Undefined;
}

API value_t _eval(value_t args) {
  value_t code = next(&args);
  value_t env = next(&args);
  return eval(env, code);
}

API value_t _is_list(value_t args) {
  return Bool(is_list(next(&args)));
}
API value_t _list_length(value_t args) {
  value_t list = next(&args);
  if (!is_list(list)) return TypeError;
  return Integer(list_length(list));
}
API value_t _list_reverse(value_t args) {
  value_t list = next(&args);
  if (!is_list(list)) return TypeError;
  return list_reverse(list);
}
API value_t _list_ireverse(value_t args) {
  value_t list = next(&args);
  if (!is_list(list)) return TypeError;
  return list_ireverse(list);
}
API value_t _list_append(value_t args) {
  value_t list = next(&args);
  if (!is_list(list)) return TypeError;
  return list_append(list, args);
}
API value_t _list_concat(value_t args) {
  value_t lists = Nil;
  while (args.type == PairType) {
    value_t list = next(&args);
    if (!is_list(list)) return TypeError;
    lists = cons(list, lists);
  }
  value_t combined = Nil;
  while (lists.type == PairType) {
    combined = list_append(next(&lists), combined);
  }
  return combined;
}

API value_t _list_sort(value_t args) {
  value_t list = next(&args);
  if (!is_list(list)) return TypeError;
  return list_sort(list);
}

API value_t _list_iget(value_t args) {
  value_t list = next(&args);
  if (!is_list(list)) return TypeError;
  value_t index = next(&args);
  if (index.type != IntegerType) return TypeError;
  return list_get(list, index.data);
}

API value_t _list_iset(value_t args) {
  value_t list = next(&args);
  if (!is_list(list)) return TypeError;
  value_t index = next(&args);
  if (index.type != IntegerType) return TypeError;
  value_t value = next(&args);
  return list_set(list, index.data, value);
}

API value_t _list_has(value_t args) {
  value_t list = next(&args);
  if (!is_list(list)) return TypeError;
  while (args.type == PairType) {
    if (!list_has(list, next(&args))) return False;
  }
  return True;
}

API value_t _list_add(value_t args) {
  value_t list = next(&args);
  if (!is_list(list)) return TypeError;
  while (args.type == PairType) {
    list = list_add(list, next(&args));
  }
  return list;
}

API value_t _list_remove(value_t args) {
  value_t list = next(&args);
  if (!is_list(list)) return TypeError;
  while (args.type == PairType) {
    list = list_remove(list, next(&args));
  }
  return list;
}


// Define a function
API value_t _def(value_t args) {
  value_t env = next(&args);
  value_t key = next(&args);
  is_list(key) ?
    table_aset(env, key, copy(args)) :
    table_set(env, key, copy(args));
  return key;
}

API value_t _do(value_t args) {
  value_t env = next(&args);
  return block(env, args);
}

API value_t _is_table(value_t args) {
  return Bool(is_table(next(&args)));
}

API value_t _get(value_t args) {
  value_t env = next(&args);
  value_t key = eval(env, next(&args));
  return is_list(key) ?
    table_aget(env, key) :
    table_get(env, key);
}

API value_t _table_get(value_t args) {
  value_t table = next(&args);
  if (!is_table(table)) return TypeError;
  value_t key = next(&args);
  return is_list(key) ?
    table_aget(table, key) :
    table_get(table, key);
}

API value_t _has(value_t args) {
  value_t env = next(&args);
  while (args.type == PairType) {
    value_t key = eval(env, next(&args));
    if (!(is_list(key) ?
        table_ahas(env, key) :
        table_has(env, key))) {
      return False;
    }
  }
  return True;
}

API value_t _table_has(value_t args) {
  value_t table = next(&args);
  if (!is_table(table)) return TypeError;
  while (args.type == PairType) {
    value_t key = next(&args);
    if (!(is_list(key) ?
        table_ahas(table, key) :
        table_has(table, key))) {
      return False;
    }
  }
  return True;
}

API value_t _set(value_t args) {
  value_t env = next(&args);
  value_t value = Undefined;
  while (args.type == PairType) {
    value_t key = eval(env, next(&args));
    value = eval(env, next(&args));
    is_list(key) ?
      table_aset(env, key, value) :
      table_set(env, key, value);
  }
  return value;
}

API value_t _table_set(value_t args) {
  value_t table = next(&args);
  if (!is_table(table)) return TypeError;
  while (args.type == PairType) {
    value_t key = next(&args);
    value_t value = next(&args);
    table = is_list(key) ?
      table_aset(table, key, value) :
      table_set(table, key, value);
  }
  return table;
}

API value_t _del(value_t args) {
  value_t env = next(&args);
  while (args.type == PairType) {
    value_t key = eval(env, next(&args));
    is_list(key) ?
      table_adel(env, key) :
      table_del(env, key);
  }
  return Undefined;
}

API value_t _table_del(value_t args) {
  value_t table = next(&args);
  if (!is_table(table)) return TypeError;
  while (args.type == PairType) {
    value_t key = next(&args);
    table = is_list(key) ?
      table_adel(table, key) :
      table_del(table, key);
  }
  return table;
}

API void each_callback(value_t ctx, value_t item) {
  set_cdr(ctx, apply(car(ctx), item));
}

API value_t _iter_each(value_t args) {
  value_t iter = next(&args);
  value_t fn = next(&args);
  value_t ctx = cons(fn, Undefined);
  iter_any(iter, ctx, each_callback);
  return free_cell(ctx).right;
}

API void map_callback(value_t ctx, value_t item) {
  set_cdr(ctx, cons(apply(car(ctx), item), cdr(ctx)));
}

API value_t _iter_map(value_t args) {
  value_t iter = next(&args);
  value_t fn = next(&args);
  value_t ctx = cons(fn, Nil);
  iter_any(iter, ctx, map_callback);
  return list_ireverse(free_cell(ctx).right);
}

API void filter_callback(value_t ctx, value_t item) {
  if (isTruthy(apply(car(ctx), item))) {
    set_cdr(ctx, cons(car(item), cdr(ctx)));
  }
}

API value_t _iter_filter(value_t args) {
  value_t iter = next(&args);
  value_t fn = next(&args);
  value_t ctx = cons(fn, Nil);
  iter_any(iter, ctx, filter_callback);
  return list_ireverse(free_cell(ctx).right);
}

API const builtin_t *functions = (const builtin_t[]){
  {"get", _get},
  {"has", _has},
  {"del", _del},
  {"set", _set},
  {"def", _def},
  {"do", _do},
  // if
  // else-if
  // else
  // while
  {"quote", _quote},
  /////////////////////

  {"list", _list},
  {"print", _print},
  {"eval", _eval},

  {"cons", _cons},
  {"car", _car},
  {"cdr", _cdr},
  {"set-car", _set_car},
  {"set-cdr", _set_cdr},

  {"table?", _is_table},
  {"t-get", _table_get},
  {"t-has", _table_has},
  {"t-del!", _table_del},
  {"t-set!", _table_set},

  {"list?", _is_list},
  {"length?", _list_length},
  {"reverse", _list_reverse},
  {"reverse!", _list_ireverse},
  {"append!", _list_append},
  {"concat!", _list_concat},
  {"sort", _list_sort},
  {"iget", _list_iget},
  {"iset!", _list_iset},
  {"has?", _list_has},
  {"add!", _list_add},
  {"remove!", _list_remove},

  {"+", _add},
  {"-", _sub},
  {"*", _mul},
  {"/", _div},
  {"%", _mod},
  {"<", _lt},
  {"<=", _lte},
  {">", _gt},
  {">=", _gte},
  {"=", _eq},
  {"!=", _neq},

  {"each", _iter_each},
  {"map", _iter_map},
  {"filter", _iter_filter},

  {0,0},
};

#ifdef UJKL_DEBUG
API const char** lines = (const char*[]) {
  "(def *10 (n) (* n 10))",
  "(*10 13)",
  "(def even (n) (= 0 (% n 2)))",
  "(even 1)",
  "(even 2)",
  "(each 5 print)",
  "(map 5 *10)",
  "(filter 10 even)",
  "(each (map (filter 10 even) *10) print)",
  "(def greet (person) (print 'Hello (t-get person 'name)))",
  "(set 'jack.name \"Jack Dean\")",
  "(greet jack)",
  0
};
#endif // UJKL_DEBUG

API void init_repl(){
  // Initialize repl environment with a version variable and ref to self.
  repl = table_set(Nil, Symbol("env"), Nil);
  // table_set(repl, Symbol("env"), repl);
  // table_set(repl, Symbol("version"), Symbol(VM_VERSION));
}

void init_ujkl() {
  symbols_init(functions, 7);
  quoteSym = Symbol("quote");
  listSym = Symbol("list");
  init_repl();
}

void restart_ujkl() {
  free_list(repl);
  collectgarbage(repl);
  symbols_clean_user_table();
  free_data_pairs();
  init_repl();
}

#ifdef UJKL_DEBUG
void parse_debug_line(int i) {
  parse(lines[i]);
}
#endif // UJKL_DEBUG

#ifdef __cplusplus
}
#endif

#endif // UJKL_H