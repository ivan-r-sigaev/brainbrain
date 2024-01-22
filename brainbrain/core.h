#pragma once
#include <stdlib.h>
#include <assert.h>

#define BB_ASSERT(expression) assert(expression)

static void* safe_malloc(size_t size)
{
	void* block = malloc(size);
	BB_ASSERT(block != NULL);
	return block;
}
static void* safe_realloc(void* block, size_t size)
{
	void* new_block = realloc(block, size);
	BB_ASSERT(new_block != NULL);
	return new_block;
}

#define BB_ALLOC(size)          safe_malloc((size))
#define BB_REALLOC(block, size) safe_realloc((block), (size))
#define BB_FREE(block)          free((block))
