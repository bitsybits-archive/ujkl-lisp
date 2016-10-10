
#ifndef SYMBOLS_C
#define SYMBOLS_C

#include "types.h"
#include <stdlib.h> // for realloc

static char *symbols;
static size_t symbols_len;
static const builtin_t *builtins;
static const builtin_t *user_builtins;
static uint8_t user_idx;
// API int first_fn;  // redefinition

void symbols_clean_user_table()
{
  free(symbols);
  symbols = NULL;
  symbols_len = 0;
}

API void symbols_init(const builtin_t *fns, int num_keywords) {
  builtins = fns;
  user_builtins = NULL;
  user_idx = 255;
  first_fn = num_keywords;
}

API void symbols_set_user_fns(const builtin_t *user_fns, uint8_t free_user_idx) {
  user_builtins = user_fns;
  user_idx = free_user_idx;
}

static void symbols_resize(size_t needed) {
  if (needed < symbols_len) return;
  // Allocate memory in blocks to reduce fragmentation
  // and batch allocations.
  size_t new_len = needed + (SYMBOLS_BLOCK_SIZE - needed % SYMBOLS_BLOCK_SIZE);
  symbols = (char*) realloc(symbols, new_len);
  for (size_t j = symbols_len; j < new_len; j++) {
    symbols[j] = 0;
  }
  symbols_len = new_len;
}

API api_fn symbols_get_fn(int index) {
  return (index >= 0) ?
  ((index >= user_idx) ? 
    user_builtins[index - user_idx].fn 
    : builtins[index].fn) 
  : 0;
}

// Resolve a symbol index to a null-terminated string.
API const char *symbols_get_name(int index) {
  if (index >= 0 && index < user_idx) {
    return builtins[index].name;
  } else if(index >= user_idx) {
    return user_builtins[index - user_idx].name;
  }
  const char *names = symbols;
  while (++index) {
    while (*names++);
  }
  return names;
}


// Lookup or store a symbol.  Builtins will return positive indexes and
// user-defined symbols will be negative.
API int symbols_set(const char *word, size_t len) {
  // Calculate the length if not given (assuming null terminated)
  if (!len) { while(word[len]) {len++;} }

  // First try to match against builtins and return positive index if found.
  int idx = 0;
  const char *name;
  const builtin_t *fns = builtins;
  int step = 0;
  while (fns) {
    while ((name = fns[idx - step].name)) {
      size_t j = 0;
      while (name[j] && j < len && word[j] == name[j]) { j++; }
      if (j == len && name[j] == 0) {
        return idx;
      }
      idx++;
    }
    fns = (0 == step) ? user_builtins : NULL;
    step = idx = user_idx;
  }
  idx = -1;

  // If not found, look for symbol in user table.
  // This is a giant `char*` containing consecutives null terminated strings.
  size_t i = 0; // This is our byte offset into the symbols table
  while (i < symbols_len && symbols[i]) {
    size_t j = 0;
    // Match the body of the word against the table entry.
    while (i < symbols_len && j < len && symbols[i] == word[j]) {
      i++;
      j++;
    }
    // If it matched completely, we found it!
    if (j == len && symbols[i] == 0) {
      return idx;
    }
    // If we didn't match, skip to end of word.
    while (i < symbols_len && symbols[i++]);
    // Decrement the logical index.
    idx--;
  }

  // Allocate more space if needed
  symbols_resize(i + len + 1);

  // Append the new symbol to the list
  for (size_t j = 0; j < len; j++) {
    symbols[i + j] = word[j];
  }
  symbols[i + len] = 0;
  return idx;
}

#endif
