#pragma once
#include <stddef.h>

// The internal representation for the bf program.
// May be further interpreted or compiled.
typedef struct Repr Repr;

Repr* repr_parse(const char* src, size_t mem_size);
size_t repr_mem_size(Repr* repr);
void repr_print(Repr* repr);
void repr_free(Repr* repr);
