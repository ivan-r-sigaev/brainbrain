#include <stdbool.h>
#include <stdio.h>
#include "core.h"
#include "repr.h"

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

struct Repr {
	Block* root;
	size_t mem_size;
};

typedef struct BlockStack BlockStack;
struct BlockStack
{
	size_t count;
	size_t capacity;
	Block* items[];
};

static Block* block_append(Block* block, Op op) 
{
	BB_ASSERT(block->ops.count <= block->ops.capacity);
	if (block->ops.count != 0) 
	{
		Op* last = &block->ops.items[block->ops.count - 1];
		// TODO: Could optimize for operators that cancel each other out here.
		//       I.e. '+' and '-', '<' and '>'...
		if (last->index == op.index && last->tag == op.tag) {
			last->count += op.count;
			return block;
		}
	}
	if (block->ops.count == block->ops.capacity)
	{
		size_t new_cap = (block->ops.capacity != 0) ? block->ops.capacity * 2 : 1;
		block = BB_REALLOC(block, sizeof(Block) + new_cap * sizeof(Op));
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
		stack = BB_REALLOC(stack, sizeof(BlockStack) + new_cap * sizeof(Block*));
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
	size_t index = 0;
	for (const char* c = src; *c != '\0'; c++, index++)
	{
		if (*c == '[') indent_level++;
		if (*c == ']')
		{
			if (indent_level == 0) 
			{
				BB_ASSERT(fprintf_s(
					stderr,
					"Invalid code: no matching openeing brace ('[') "
					"for closing brace (']') at byte %zu.\n",
					index) > 0);
				exit(1);
			}
			indent_level--;
		}
	}
	if (indent_level != 0)
	{
		BB_ASSERT(fprintf_s(
			stderr,
			"Invalid code: %zu opening braces ('[') are left unbalanced "
			"(lacking a corresponding closing brace (']')) "
			"upon reaching the end of source code.\n",
			indent_level) > 0);
		exit(1);
	}
}

Repr* repr_parse(const char* src, size_t mem_size)
{
	check_valid_bf(src);
	BlockStack* unclosed = BB_ALLOC(sizeof(BlockStack));
	*unclosed = (BlockStack){0};
	Block* root = BB_ALLOC(sizeof(Block));
	*root = (Block){0};
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
				(Op){
					.tag = OpIncrement,
					.index = index,
					.count = 1});
			break;
		case '-':
			current = block_append(
				current,
				(Op){
					.tag = OpDecrement,
					.index = index,
					.count = 1});
			break;
		case ',':
			current = block_append(
				current,
				(Op){
					.tag = OpInput,
					.index = index,
					.count = 1});
			break;
		case '.':
			current = block_append(
				current,
				(Op){
					.tag = OpOutput,
					.index = index,
					.count = 1});
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
			Block* block = BB_ALLOC(sizeof(Block));
			*block = (Block){0};
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
			Block* block = BB_ALLOC(sizeof(Block));
			*block = (Block){0};
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
	Repr* repr = BB_ALLOC(sizeof(Repr));
	*repr = (Repr){
		.root = root,
		.mem_size = mem_size};
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
	while(tmp != NULL && tmp != block)
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
		BB_ASSERT(printf_s("\t") > 0);
	}
}

static void block_print(Block* block, size_t indent) 
{
	print_indent(indent);
	BB_ASSERT(printf_s("Block %p:\n", block) > 0);
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
		BB_ASSERT(printf_s("%c[%zu] (%zu times)\n", c, op.index, op.count) > 0);
	}
	print_indent(indent + 1);
	BB_ASSERT(printf_s("[%zu]\n", block->last_index) > 0);
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
			BB_ASSERT(printf_s("Loop:\n") > 0);
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
	BB_ASSERT(printf_s(
		"Memory size:%zu\n"
		"Blocks:\n",
		repr->mem_size) > 0);
	block_print_chain(repr->root, 1);
}

void repr_free(Repr* repr) 
{
	block_free(repr->root);
	BB_FREE(repr);
}
