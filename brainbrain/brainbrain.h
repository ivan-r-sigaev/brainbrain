/*
	MIT License
	
	Copyright (c) 2024 Kakusakov
	
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	
	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

// // You may use following defines if you want to customize the header implementation.
// #define BB_ASSERT(x) your_assert(x)
// #define BB_MALLOC    your_malloc
// #define BB_REALLOC   your_realloc
// #define BB_FREE      your_free
// 
// // Turns this header into header implementation file.
// #define BB_IMPLEMENTATION
// #include "brainbrain.h"

#pragma once
#include <stddef.h>
#include <stdio.h>

// The internal representation for the bf program.
// May be further interpreted or compiled.
typedef struct Repr Repr;

Repr* repr_parse(const char* src, size_t mem_size);
size_t repr_mem_size(Repr* repr);
void repr_print(Repr* repr);
void repr_execute(Repr* repr, FILE* input, FILE* output);
void repr_free(Repr* repr);

#ifdef BB_IMPLEMENTATION
#include <stdbool.h>
#include <string.h>

#ifndef BB_ASSERT
#include <assert.h>
#define BB_ASSERT(x) assert(x)
#endif

#ifndef BB_MALLOC
#include <stdlib.h>
#define BB_MALLOC malloc
#endif

#ifndef BB_REALLOC
#include <stdlib.h>
#define BB_REALLOC realloc
#endif

#ifndef BB_FREE
#include <stdlib.h>
#define BB_FREE free
#endif

static void* safe_malloc(size_t size) 
{
	void* block = BB_MALLOC(size);
	BB_ASSERT(block != NULL);
	return block;
}

static void* safe_realloc(void* block, size_t size)
{
	void* new_block = BB_REALLOC(block, size);
	BB_ASSERT(new_block != NULL);
	return new_block;
}

typedef enum OpTag OpTag;
enum OpTag
{
	OpIncrement,
	OpDecrement,
	OpOutput,
	OpInput,
};

typedef struct Op Op;
struct Op
{
	OpTag tag;
	size_t index;
	size_t count;
};

typedef struct Ops Ops;
struct Ops
{
	size_t count;
	size_t capacity;
	Op items[];
};

typedef struct Block Block;
struct Block
{
	Block* branch;
	Block* next;
	size_t last_index;
	Ops ops;
};

typedef struct BlockStack BlockStack;
struct BlockStack
{
	size_t count;
	size_t capacity;
	Block* items[];
};

struct Repr {
	Block* root;
	size_t mem_size;
};

static Block* block_append(Block* block, Op op)
{
	BB_ASSERT(block->ops.count <= block->ops.capacity);
	if (block->ops.count != 0)
	{
		Op* last = &block->ops.items[block->ops.count - 1];
		// TODO: Could optimize for operators that cancel each other out here.
		if (last->index == op.index && last->tag == op.tag) {
			last->count += op.count;
			return block;
		}
	}
	if (block->ops.count == block->ops.capacity)
	{
		size_t new_cap = (block->ops.capacity != 0) ? block->ops.capacity * 2 : 1;
		block = safe_realloc(block, sizeof(Block) + new_cap * sizeof(Op));
		block->ops.capacity = new_cap;
	}
	block->ops.items[block->ops.count++] = op;
	return block;
}

static BlockStack* block_stack_push(BlockStack* stack, Block* block)
{
	BB_ASSERT(stack->count <= stack->capacity);
	if (stack->count == stack->capacity)
	{
		size_t new_cap = (stack->capacity != 0) ? stack->capacity * 2 : 1;
		stack = safe_realloc(stack, sizeof(BlockStack) + new_cap * sizeof(Block*));
		stack->capacity = new_cap;
	}
	stack->items[stack->count++] = block;
	return stack;
}

static Block* block_stack_pop(BlockStack* stack)
{
	BB_ASSERT(stack->count != 0);
	return stack->items[--(stack->count)];
}

static void check_valid_bf(const char* src)
{
	size_t indent_level = 0;
	size_t char_count = 0;
	size_t line_count = 0;
	for (const char* c = src; *c != '\0'; c++, char_count++)
	{
		if (*c == '\n')
		{
			line_count++;
			char_count = 0;
		}
		if (*c == '[') indent_level++;
		if (*c == ']')
		{
			if (indent_level == 0)
			{
				int result = fprintf_s(
					stderr,
					"Invalid code: no matching openeing brace ('[') "
					"for closing brace (']') at line %zu byte %zu.\n",
					line_count, char_count);
				BB_ASSERT(result > 0);
				exit(1);
			}
			indent_level--;
		}
	}
	if (indent_level != 0)
	{
		int result = fprintf_s(
			stderr,
			"Invalid code: %zu opening braces ('[') are left unbalanced "
			"(lacking a corresponding closing brace (']')) "
			"upon reaching the end of source code.\n",
			indent_level);
		BB_ASSERT(result > 0);
		exit(1);
	}
}

Repr* repr_parse(const char* src, size_t mem_size)
{
	check_valid_bf(src);
	BlockStack* unclosed = safe_malloc(sizeof(BlockStack));
	*unclosed = (BlockStack){ 0 };
	Block* root = safe_malloc(sizeof(Block));
	*root = (Block){ 0 };
	Block* current = root;
	Block** to_current = &root;
	bool should_push_stack = false;
	size_t index = 0;
	for (const char* c = src; *c != '\0'; c++)
	{
		switch (*c)
		{
		case '+':
			current = block_append(
				current,
				(Op) {
				.tag = OpIncrement,
					.index = index,
					.count = 1
			});
			break;
		case '-':
			current = block_append(
				current,
				(Op) {
				.tag = OpDecrement,
					.index = index,
					.count = 1
			});
			break;
		case ',':
			current = block_append(
				current,
				(Op) {
				.tag = OpInput,
					.index = index,
					.count = 1
			});
			break;
		case '.':
			current = block_append(
				current,
				(Op) {
				.tag = OpOutput,
					.index = index,
					.count = 1
			});
			break;
		case '>':
			index = (index + 1) % mem_size;
			break;
		case '<':
			index = (index + mem_size - 1) % mem_size;
			break;
		case '[':
		{
			if (should_push_stack)
			{
				should_push_stack = false;
				unclosed = block_stack_push(unclosed, current);
			}
			current->last_index = index;
			index = 0;
			Block* block = safe_malloc(sizeof(Block));
			*block = (Block){ 0 };
			//unclosed = block_stack_push(unclosed, block);
			should_push_stack = true;
			current->next = block;
			*to_current = current;
			to_current = &current->next;
			current = block;
			break;
		}
		case ']':
		{
			if (should_push_stack)
			{
				should_push_stack = false;
				unclosed = block_stack_push(unclosed, current);
			}
			current->last_index = index;
			index = 0;
			Block* block = safe_malloc(sizeof(Block));
			*block = (Block){ 0 };
			current->next = block_stack_pop(unclosed);
			current->next->branch = block;
			*to_current = current;
			to_current = &current->next->branch;
			current = block;
			break;
		}
		default:
			break;
		}
	}
	current->last_index = index;
	*to_current = current;
	BB_ASSERT(unclosed->count == 0);
	BB_FREE(unclosed);
	Repr* repr = safe_malloc(sizeof(Repr));
	*repr = (Repr){
		.root = root,
		.mem_size = mem_size };
	return repr;
}

size_t repr_mem_size(Repr* repr)
{
	return repr->mem_size;
}

static void block_free(Block* block) {
	if (block == NULL) return;
	Block* tmp = block->next;
	BB_FREE(block);
	while (tmp != NULL && tmp != block)
	{
		if (tmp->branch != NULL)
		{
			Block* branch = tmp->branch;
			block_free(tmp);
			tmp = branch;
		}
		else
		{
			Block* next = tmp->next;
			BB_FREE(tmp);
			tmp = next;
		}
	};
}

static void print_indent(size_t indent)
{
	for (size_t j = 0; j < indent; j++)
	{
		int result = printf_s("\t");
		BB_ASSERT(result > 0);
	}
}

static void block_print(Block* block, size_t indent)
{
	print_indent(indent);
	int result = printf_s("Block 0x%p:\n", block);
	BB_ASSERT(result > 0);
	for (size_t i = 0; i < block->ops.count; i++)
	{
		char c;
		Op op = block->ops.items[i];
		switch (op.tag)
		{
		case OpIncrement:
			c = '+';
			break;
		case OpDecrement:
			c = '-';
			break;
		case OpInput:
			c = ',';
			break;
		case OpOutput:
			c = '.';
			break;
		default:
			BB_ASSERT(false);
		}
		print_indent(indent + 1);
		result = printf_s("%c[%zu] (%zu times)\n", c, op.index, op.count);
		BB_ASSERT(result > 0);
	}
	print_indent(indent + 1);
	result = printf_s("[%zu]\n", block->last_index);
	BB_ASSERT(result > 0);
}

static void block_print_chain(Block* block, size_t indent)
{
	if (block == NULL) return;
	Block* tmp = block->next;
	block_print(block, indent);
	while (tmp != NULL && tmp != block)
	{
		if (tmp->branch != NULL)
		{
			print_indent(indent);
			int result = printf_s("Loop:\n");
			BB_ASSERT(result > 0);
			block_print_chain(tmp, indent + 1);
			tmp = tmp->branch;
		}
		else
		{
			block_print(tmp, indent);
			tmp = tmp->next;
		}
	};
}

void repr_print(Repr* repr)
{
	int result = printf_s(
		"Memory size:%zu\n"
		"Blocks:\n",
		repr->mem_size);
	BB_ASSERT(result > 0);
	block_print_chain(repr->root, 1);
}

void repr_execute(Repr* repr, FILE* input, FILE* output)
{
	Block* block = repr->root;
	size_t wrap = repr->mem_size;
	size_t index = 0;
	unsigned char* memory = safe_malloc(wrap * sizeof(unsigned char));
	memset(memory, 0, wrap * sizeof(unsigned char));
	while (block != NULL)
	{
		if (block->branch && memory[index] == 0)
		{
			block = block->branch;
			continue;
		}
		size_t rel_index = index;
		for (size_t i = 0; i < block->ops.count; i++)
		{
			Op op = block->ops.items[i];
			switch (op.tag)
			{
			case OpIncrement:
				memory[(rel_index + op.index) % wrap] += op.count;
				break;
			case OpDecrement:
				memory[(rel_index + op.index) % wrap] -= op.count;
				break;
			case OpInput:
			{
				char* result = fgets(&memory[(rel_index + op.index) % wrap], op.count + 1, input);
				BB_ASSERT(result != NULL);
				break;
			}
			case OpOutput:
				for (size_t j = 0; j < op.count; j++)
				{
					int result = putc(memory[(rel_index + op.index) % wrap], output);
					BB_ASSERT(result != EOF);
				}
				break;
			default:
				BB_ASSERT(false);
			}
		}
		index = (index + block->last_index) % wrap;
		block = block->next;
	}
	BB_FREE(memory);
}

void repr_free(Repr* repr)
{
	block_free(repr->root);
	BB_FREE(repr);
}
#endif
