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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#define BF_MEMORY_SIZE 3000
#define BF_MEMORY_SIZE_STR "3000"

#include <assert.h>
#define ASSERT(x) assert(x)

static void crash_bad_output_flag(void)
{
	fprintf(stderr, "error: \"-o\" flag is specified, but output file is not.\n");
	exit(1);
}

static void crash_no_input_files(void)
{
	fprintf(stderr, "error: Please, specify an input file.\n");
	exit(1);
}

static void crash_multiple_output_files(void)
{
	fprintf(stderr, "error: Can't select more than one output file.\n");
	exit(1);
}

static void crash_multiple_input_files(void)
{
	fprintf(stderr, "error: Can't select more than one input file.\n");
	exit(1);
}

static void crash_multiple_targets(void)
{
	fprintf(stderr, "error: Can't select more than one target.\n");
	exit(1);
}

static void crash_alloc_failed(void)
{
	fprintf(stderr, "error: Failed to allocate enough memory.\n");
	exit(1);
}

static void crash_bad_bf(void)
{
	// TODO: The error should be more helpful.
	fprintf(stderr, "error: Source code contatins invalid brainf*ck.\n");
	exit(1);
}

typedef uint8_t OpTag;
enum
{
	OP_TAG_INC,
	OP_TAG_SHIFT,
	OP_TAG_READ,
	OP_TAG_WRITE,
};

typedef struct OpInc OpInc;
struct OpInc
{
	uint8_t value;
};

typedef struct OpShift OpShift;
struct OpShift
{
	uint16_t index;
};

typedef struct Op Op;
struct Op
{
	OpTag tag;
	union
	{
		OpInc inc;
		OpShift shift;
	} as;
};

typedef struct Ops Ops;
struct Ops
{
	size_t capacity;
	size_t count;
	Op* items;
};

typedef struct Block Block;
struct Block
{
	Block* next;
	Block* exit;
	Ops ops;
};

typedef struct Blocks Blocks;
struct Blocks
{
	size_t capacity;
	size_t count;
	Block** items;
};

static void blocks_push(Blocks* blocks, Block* block)
{
	ASSERT(blocks != NULL);
	if (blocks->count == blocks->capacity)
	{
		ASSERT(blocks->capacity <= SIZE_MAX / sizeof(Block) / 2);
		blocks->capacity = (blocks->capacity == 0) ? 1 : blocks->capacity * 2;
		blocks->items = realloc(blocks->items, blocks->capacity * sizeof(Block*));
		if (blocks->items == NULL) crash_alloc_failed();
	}
	blocks->items[blocks->count++] = block;
}

static Block* blocks_pop(Blocks* blocks)
{
	ASSERT(blocks != NULL);
	if (blocks->count == 0) crash_bad_bf();
	return blocks->items[--blocks->count];
}

static void block_append_op(Block* block, Op op)
{
	Ops* ops = &block->ops;
	if (ops->count != 0)
	{
		Op* last = &ops->items[ops->count - 1];
		if (last->tag == op.tag)
		{
			if (op.tag == OP_TAG_INC)
			{
				last->as.inc.value += op.as.inc.value;
				if (last->as.inc.value == 0) block->ops.count--;
				return;
			}
			if (op.tag == OP_TAG_SHIFT) 
			{
				last->as.shift.index += op.as.shift.index;
				last->as.shift.index %= BF_MEMORY_SIZE;
				if (last->as.shift.index == 0) block->ops.count--;
				return;
			}
		}
	}
	if (block->ops.count == block->ops.capacity)
	{
		ASSERT(ops->capacity <= SIZE_MAX / sizeof(Op) / 2);
		ops->capacity = (ops->capacity == 0) ? 1 : ops->capacity * 2;
		ops->items = realloc(ops->items, ops->capacity * sizeof(Op));
		if (ops->items == NULL) crash_alloc_failed();
	}
	block->ops.items[block->ops.count++] = op;
}

static Block* parse(const char* src)
{
	Block* root = calloc(1, sizeof(Block));
	if (root == NULL) crash_alloc_failed();
	Block* block = root;
	Blocks unclosed = {0};

	for (const char* c = src; *c != '\0'; c++)
	{
		switch (*c)
		{
		case '+': case '-': 
		case '>': case '<':
		case '.': case ',': {
			Op tag =
				(*c == '+') ? (Op){ .tag = OP_TAG_INC, .as.inc.value = 1 } :
				(*c == '-') ? (Op){ .tag = OP_TAG_INC, .as.inc.value = UINT8_MAX }:
				(*c == '<') ? (Op){ .tag = OP_TAG_SHIFT, .as.shift.index = BF_MEMORY_SIZE - 1 } :
				(*c == '>') ? (Op){ .tag = OP_TAG_SHIFT, .as.shift.index = 1 } :
				(*c == ',') ? (Op){ .tag = OP_TAG_READ } :
				(*c == '.') ? (Op){ .tag = OP_TAG_WRITE } :
				(ASSERT(0), (Op){0});
			block_append_op(block, tag);
		} break;
		case '[': {
			Block* next = calloc(1, sizeof(Block));
			if (next == NULL) crash_alloc_failed();
			block->next = next;
			block = block->next;
			blocks_push(&unclosed, block);
		} break;
		case ']': {
			Block* backedge = blocks_pop(&unclosed);
			Block* next = calloc(1, sizeof(Block));
			if (next == NULL) crash_alloc_failed();
			backedge->exit = next;
			block = next;
		} break;
		default: break;
		}
	}

	if (unclosed.count != 0) crash_bad_bf();
	free(unclosed.items);
	return root;
}

typedef enum Target Target;
enum Target
{
	TARGET_NOT_SELECTED = 0,
	TARGET_BF,
	TARGET_NASM_LIBC,
	TARGET_NASM_LINUX,
};

static int print_tab(size_t count, FILE* file)
{
	for (size_t i = 0; i < count; i++)
	{
		if (fputs("    ", file) == EOF) return 0;
	}
	return 1;
}

static int emit_file_head(FILE* file, Target target)
{
	switch (target)
	{
	case TARGET_BF: break;
	case TARGET_NASM_LINUX: {
		if (fprintf(
			file,
			"global _start\n"
			"\n"
			"section .bss\n"
			"tmp resd 1\n"
			"\n"
			"section .data\n"
			"mem db " BF_MEMORY_SIZE_STR " dup(0)\n"
			"\n"
			"section .text\n"
			"_start:\n"
			"xor esi, esi\n"
		) < 0) return 0;
	} break;
	case TARGET_NASM_LIBC: {
		if (fprintf(
			file,
			"extern putchar\n"
			"extern getchar\n"
			"extern exit\n"
			"global _start\n"
			"\n"
			"section .data\n"
			"mem db " BF_MEMORY_SIZE_STR " dup(0)\n"
			"\n"
			"section .text\n"
			"_start:\n"
			"xor esi, esi\n"
		) < 0) return 0;
	} break;
	default: {
		ASSERT(0);
	} break;
	}
	return 1;
}

static int emit_file_tail(FILE* file, Target target)
{
	switch (target)
	{
	case TARGET_BF: break;
	case TARGET_NASM_LINUX: {
		if (fprintf(
			file,
			"mov eax, 1\n"
			"mov ebx, 0\n"
			"int 80h\n"
		) < 0) return 0;
	} break;
	case TARGET_NASM_LIBC: {
		if (fprintf(
			file,
			"mov rdi, 0\n"
			"call exit\n"
		) < 0) return 0;
	} break;
	default: {
		ASSERT(0);
	} break;
	}
	return 1;
}

static int emit_loop_head(Block* loop, size_t layer, FILE* file, Target target)
{
	switch (target)
	{
	case TARGET_BF: {
		if (!print_tab(layer, file)) return 0;
		if (fprintf(file, "[\n") < 0) return 0;
	} break;
	case TARGET_NASM_LIBC: case TARGET_NASM_LINUX: {
		if (fprintf(
			file,
			".loop_%p:\n"
			"cmp byte [mem + esi], 0\n"
			"je .end_%p\n",
			loop,
			loop
		) < 0) return 0;
	} break;
	default: {
		ASSERT(0);
	} break;
	}
	return 1;
}

static int emit_loop_tail(Block* loop, size_t layer, FILE* file, Target target)
{
	switch (target)
	{
	case TARGET_BF: {
		if (!print_tab(layer - 1, file)) return 0;
		if (fprintf(file, "]\n") < 0) return 0;
	} break;
	case TARGET_NASM_LIBC: case TARGET_NASM_LINUX: {
		if (fprintf(
			file,
			"jmp .loop_%p\n"
			".end_%p:\n",
			loop,
			loop
		) < 0) return 0;
	} break;
	default: {
		ASSERT(0);
	} break;
	}
	return 1;
}

static int inc_signed_count(OpInc inc)
{
	int32_t count = inc.value;
	if (count > INT8_MAX) count = -((int32_t)UINT8_MAX - count + 1);
	return (int)count;
}

static int emit_op_inc(OpInc inc, size_t layer, FILE* file, Target target)
{
	switch (target)
	{
	case TARGET_BF: {
		if (!print_tab(layer, file)) return 0;
		int count = inc_signed_count(inc);
		for (int i = count; i > 0; i--)
		{
			if (fprintf(file, "+") < 0) return 0;
		}
		for (int i = -count; i > 0; i--)
		{
			if (fprintf(file, "-") < 0) return 0;
		}
		if (fprintf(file, "\n") < 0) return 0;
	} break;
	case TARGET_NASM_LIBC: case TARGET_NASM_LINUX: {
		if (fprintf(
			file,
			"mov al, [mem + esi]\n"
			"add al, %" PRIu8 "\n"
			"mov [mem + esi], al\n",
			inc.value
		) < 0) return 0;
	} break;
	default: {
		ASSERT(0);
	} break;
	}
	return 1;
}


static int shift_signed_count(OpShift shift)
{
	int32_t count = shift.index;
	if (count > BF_MEMORY_SIZE / 2) count = -((int32_t)BF_MEMORY_SIZE - count);
	return (int)count;
}

static int emit_op_shift(OpShift shift, size_t layer, FILE* file, Target target)
{
	switch (target)
	{
	case TARGET_BF: {
		if (!print_tab(layer, file)) return 0;
		int count = shift_signed_count(shift);
		for (int i = count; i > 0; i--)
		{
			if (fprintf(file, ">") < 0) return 0;
		}
		for (int i = -count; i > 0; i--)
		{
			if (fprintf(file, "<") < 0) return 0;
		}
		if (fprintf(file, "\n") < 0) return 0;
	} break;
	case TARGET_NASM_LIBC: case TARGET_NASM_LINUX: {
		if (fprintf(
			file,
			"add si, %" PRIu16 "\n"
			"xor dx, dx\n"
			"mov ax, si\n"
			"mov bx, " BF_MEMORY_SIZE_STR  "\n"
			"div bx\n"
			"mov si, dx\n",
			shift.index
		) < 0) return 0;
	} break;
	default: {
		ASSERT(0);
	} break;
	}
	return 1;
}

static int emit_op_read(FILE* file, size_t layer, Target target)
{
	switch (target)
	{
	case TARGET_BF: {
		if (!print_tab(layer, file)) return 0;
		if (fprintf(file, ",\n") < 0) return 0;
	} break;
	case TARGET_NASM_LIBC: {
		if (fprintf(
			file,
			"call getchar\n"
			"mov [mem + esi], al\n"
		) < 0) return 0;
	} break;
	case TARGET_NASM_LINUX: {
		if (fprintf(
			file,
			"mov eax, 0x3\n"
			"mov ebx, 0x1\n"
			"mov ecx, tmp\n"
			"mov edx, 0x1\n"
			"int 80h\n"
			"mov eax, [tmp]\n"
			"mov [mem + esi], eax\n"
		) < 0) return 0;
	} break;
	default: {
		ASSERT(0);
	} break;
	}
	return 1;
}

static int emit_op_write(FILE* file, size_t layer, Target target)
{
	switch (target)
	{
	case TARGET_BF: {
		if (!print_tab(layer, file)) return 0;
		if (fprintf(file, ".\n") < 0) return 0;
	} break;
	case TARGET_NASM_LIBC: {
		if (fprintf(
			file,
			"xor rdi, rdi\n"
			"mov dil, [mem + esi]\n"
			"call putchar\n"
		) < 0) return 0;
	} break;
	case TARGET_NASM_LINUX: {
		if (fprintf(
			file,
			"xor eax, eax\n"
			"mov al, [mem + esi]\n"
			"mov [tmp], eax\n"
			"mov eax, 0x4\n"
			"mov ebx, 0x1\n"
			"mov ecx, tmp\n"
			"mov edx, 0x1\n"
			"int 80h\n"
		) < 0) return 0;
	} break;
	default: {
		ASSERT(0);
	} break;
	}
	return 1;
}

static int emit_code(Block* block, FILE* file, Target target)
{
	size_t layer = 0;
	Blocks loops = {0};
	if (!emit_file_head(file, target)) goto error;
	while (1)
	{
		if (block->exit != NULL)
		{
			if (!emit_loop_head(block, layer, file, target)) goto error;
			blocks_push(&loops, block);
			layer++;
		}

		for (size_t i = 0; i < block->ops.count; i++)
		{
			Op op = block->ops.items[i];
			switch (op.tag)
			{
			case OP_TAG_INC: {
				if (!emit_op_inc(op.as.inc, layer, file, target)) goto error;
			} break;
			case OP_TAG_SHIFT: {
				if (!emit_op_shift(op.as.shift, layer, file, target)) goto error;
			} break;
			case OP_TAG_READ: {
				if (!emit_op_read(file, layer, target)) goto error;
			} break;
			case OP_TAG_WRITE: {
				if (!emit_op_write(file, layer, target)) goto error;
			} break;
			default: {
				ASSERT(0);
			} break;
			}
		}
		if (block->next == NULL)
		{
			if (loops.count == 0) break;
			block = blocks_pop(&loops);
			if (!emit_loop_tail(block, layer, file, target)) goto error;
			block = block->exit;
			layer--;
		}
		else block = block->next;
	}
	if (!emit_file_tail(file, target)) goto error;

	ASSERT(layer == 0);
	free(loops.items);
	return 1;
error:
	free(loops.items);
	return 0;
}

void print_usage(const char* name)
{
	fprintf(
		stderr,
		"Usage: %s <input>\n"
		"input - path to input file.\n"
		"flags:\n"
		"-h - prints this message.\n"
		"-o filename - sepcify path to output file.\n"
		"              If output path is not specified,\n"
		"              program will write to stdout.\n"
		"--help - prints this message.\n"
		"--libc - set target to libc (default).\n"
		"--linux - set target to linux.\n"
		"--brain - generates brainf*ck insted of assembly.\n",
		name);
}

void print_file_not_opened(const char* path, const char* purpose)
{
	fprintf(
		stderr,
		"error: Failed to open %s for %s: %s\n",
		path,
		purpose,
		strerror(errno));
}

char* read_entire_file(FILE* f)
{
	if (fseek(f, 0, SEEK_END) != 0) return NULL;
	long fsize = ftell(f);
	if (fsize == -1L) return NULL;
	if (fseek(f, 0, SEEK_SET) != 0) return NULL;

	char* string = malloc(fsize + 1);
	if (string == NULL) crash_alloc_failed();
	if (fread(string, fsize, 1, f) != 1) return NULL;

	string[fsize] = 0;
	return string;
}

int main(int argc, char* argv[]) 
{
	const char* program_path = (argc >= 1) ? argv[0] : "<brainbrain-path>";
	if (argc < 2)
	{
		print_usage(program_path);
		return 1;
	}

	Target target = TARGET_NOT_SELECTED;
	const char* input_path = NULL;
	const char* output_path = NULL;

	for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
		{
			print_usage(program_path);
			return 0;
		}
		else if (strcmp(argv[i], "--brain") == 0)
		{
			if (target != TARGET_NOT_SELECTED) crash_multiple_targets();
			target = TARGET_BF;
		}
		else if (strcmp(argv[i], "--linux") == 0)
		{
			if (target != TARGET_NOT_SELECTED) crash_multiple_targets();
			target = TARGET_NASM_LINUX;
		}
		else if (strcmp(argv[i], "--libc") == 0)
		{
			if (target != TARGET_NOT_SELECTED) crash_multiple_targets();
			target = TARGET_NASM_LIBC;
		}
		else if (strcmp(argv[i], "-o") == 0)
		{
			if (output_path != NULL) crash_multiple_output_files();
			if (i + 1 >= argc) crash_bad_output_flag();
			i++;
			output_path = argv[i];
		}
		else
		{
			if (input_path != NULL) crash_multiple_input_files();
			input_path = argv[i];
		}
    }
	if (target == TARGET_NOT_SELECTED) target = TARGET_NASM_LIBC;
	if (input_path == NULL) crash_no_input_files();

	FILE* input = fopen(input_path, "rb");
	if (input == NULL)
	{
		print_file_not_opened(argv[1], "reading");
		return 1;
	}
	char* src = read_entire_file(input);
	if (src == NULL)
	{
		fprintf(stderr, "error: Uable to read from file %s: %s", input_path, strerror(errno));
		return 1;
	}
	Block* flie = parse(src);
	free(src);
	fclose(input);

	FILE* output = stdout;
	if (output_path == NULL) output_path = "stdout";
	else 
	{
		output = fopen(output_path, "wb");
		if (output == NULL)
		{
			print_file_not_opened(output_path, "writing");
			return 1;
		}
	}

	if (!emit_code(flie, output, target))
	{
		fprintf(stderr, "failed to write to %s: %s", output_path, strerror(errno));
		return 1;
	}
	fclose(output);
	return 0;
}
