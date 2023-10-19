// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "helpers.h"
#include "../utils/printf.h"

struct block_meta *global_base;

void split_block(struct block_meta *block, size_t size)
{
	struct block_meta *free_block = (struct block_meta *)
										((char *)(block) +
											ALIGN(size) +
											ALIGN(sizeof(struct block_meta)));

	free_block->size = ALIGN(ALIGN(block->size) -
						ALIGN(size) -
						ALIGN(sizeof(struct block_meta)));
	free_block->status = STATUS_FREE;

	if (block->next != NULL)
		free_block->next = block->next;
	else
		free_block->next = NULL;
	block->status = STATUS_ALLOC;
	block->size = ALIGN(size);
	block->next = free_block;
}

struct block_meta *find_free_block(size_t size)
{
	struct block_meta *current = global_base;
	struct block_meta *best_fit_block = NULL;
	size_t best_fit_size = 0;

	while (current != NULL) {
		if (current->status == STATUS_FREE &&
				ALIGN(current->size) >= ALIGN(size)) {
			if (best_fit_block == NULL) {
				best_fit_block = current;
				best_fit_size = ALIGN(current->size);
			} else if (ALIGN(current->size) < ALIGN(best_fit_size)) {
				best_fit_block = current;
				best_fit_size = ALIGN(current->size);
			}
		}
		current = current->next;
	}

	if (best_fit_block != NULL)
		best_fit_block->status = STATUS_ALLOC;

	if (best_fit_block != NULL &&
			(ALIGN(best_fit_size) - ALIGN(size) >=
				ALIGN(sizeof(struct block_meta)) + ALIGN(1)))
		split_block(best_fit_block, size);

	return best_fit_block;
}

void coalesce(void)
{
	struct block_meta *current = global_base;

	while (current != NULL) {
		if (current->status == STATUS_FREE && current->next != NULL && current->next->status == STATUS_FREE) {
			current->size = ALIGN(current->size) + ALIGN(current->next->size) + ALIGN(sizeof(struct block_meta));
			current->next = current->next->next;

			if (current->next != NULL && current->next->status == STATUS_FREE)
				continue;
		}
		current = current->next;
	}
}

struct block_meta *coalesce_realloc(struct block_meta *block)
{
	struct block_meta *current = global_base;

	while (current != block)
		current = current->next;

	if (current->next != NULL && current->next->status == STATUS_FREE) {
		current->size = ALIGN(current->size) + ALIGN(current->next->size) + ALIGN(sizeof(struct block_meta));
		current->next = current->next->next;
		return current;
	}

	return NULL;
}

struct block_meta *request_space_malloc(struct block_meta *last, size_t size)
{
	struct block_meta *block = NULL;

	if (last == NULL) {
		if (size < MMAP_THRESHOLD) {
			block = sbrk(0);

			DIE(block == (void *) -1, "sbrk");

			void *request = sbrk(MMAP_THRESHOLD);

			DIE(request == (void *) -1, "sbrk");

			block = (struct block_meta *)request;
			block->size = MMAP_THRESHOLD - ALIGN(sizeof(struct block_meta));
			block->status = STATUS_ALLOC;
			block->next = NULL;

			if (ALIGN(block->size) - ALIGN(size) >= ALIGN(sizeof(struct block_meta)) + ALIGN(1)) {
				split_block(block, size);
				return block;
			}
		} else {
			void *ptr = mmap(NULL, ALIGN(size + sizeof(struct block_meta)),
							PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

			DIE(ptr == MAP_FAILED, "mmap");

			block = (struct block_meta *)ptr;

			block->status = STATUS_MAPPED;
			block->size = ALIGN(size);
			block->next = NULL;
		}
	} else {
		if (size >= MMAP_THRESHOLD) {
			void *ptr = mmap(NULL, ALIGN(size + sizeof(struct block_meta)),
							PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			DIE(ptr == MAP_FAILED, "mmap");

			block = (struct block_meta *)ptr;

			block->status = STATUS_MAPPED;
			block->size = ALIGN(size);
			block->next = NULL;

		} else {
			while (last->next != NULL)
				last = last->next;

			if (last->status == STATUS_FREE) {
				void *request = sbrk(ALIGN(size) - ALIGN(last->size));

				DIE(request == (void *) -1, "sbrk");

				last->size = ALIGN(size);
				last->status = STATUS_ALLOC;
				block = last;
			} else {
				void *request = sbrk(ALIGN(size) + ALIGN(sizeof(struct block_meta)));

				DIE(request == (void *) -1, "sbrk");

				block = (struct block_meta *)request;

				block->status = STATUS_ALLOC;
				block->size = ALIGN(size);
				block->next = NULL;
				last->next = block;
			}
		}
	}

	return block;
}

struct block_meta *request_space_calloc(struct block_meta *last, size_t size)
{
	struct block_meta *block = NULL;

	if (last == NULL) {
		if (ALIGN(size + sizeof(struct block_meta)) < CALLOC_THRESHOLD) {
			block = sbrk(0);

			DIE(block == (void *) -1, "sbrk");

			void *request = sbrk(MMAP_THRESHOLD);

			DIE(request == (void *) -1, "sbrk");

			block = (struct block_meta *)request;
			block->size = MMAP_THRESHOLD - ALIGN(sizeof(struct block_meta));
			block->status = STATUS_ALLOC;
			block->next = NULL;

			if (ALIGN(block->size) - ALIGN(size) >= ALIGN(sizeof(struct block_meta)) + ALIGN(1)) {
				split_block(block, size);
				return block;
			}
		} else {
			void *ptr = mmap(NULL, ALIGN(size + sizeof(struct block_meta)),
							PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			DIE(ptr == MAP_FAILED, "mmap");

			block = (struct block_meta *)ptr;

			block->status = STATUS_MAPPED;
			block->size = ALIGN(size);
			block->next = NULL;
		}
	} else {
		if (ALIGN(size + sizeof(struct block_meta)) >= CALLOC_THRESHOLD) {
			void *ptr = mmap(NULL, ALIGN(size + sizeof(struct block_meta)),
							PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			DIE(ptr == MAP_FAILED, "mmap");

			block = (struct block_meta *)ptr;

			block->status = STATUS_MAPPED;
			block->size = ALIGN(size);
			block->next = NULL;
		} else {
			while (last->next != NULL)
				last = last->next;

			if (last->status == STATUS_FREE) {
				void *request = sbrk(ALIGN(size) - ALIGN(last->size));

				DIE(request == (void *) -1, "sbrk");

				last->size = ALIGN(size);
				last->status = STATUS_ALLOC;
				block = last;
			} else {
				void *request = sbrk(ALIGN(size) + ALIGN(sizeof(struct block_meta)));

				DIE(request == (void *) -1, "sbrk");

				block = (struct block_meta *)request;

				block->status = STATUS_ALLOC;
				block->size = ALIGN(size);
				block->next = NULL;
				last->next = block;
			}
		}
	}

	return block;
}

void *os_malloc(size_t size)
{
	if (size <= 0)
		return NULL;

	struct block_meta *block = NULL;

	if (global_base == NULL) {
		block = request_space_malloc(NULL, size);
		if (block == NULL)
			return NULL;

		if (size < MMAP_THRESHOLD)
			global_base = block;
	} else {
		struct block_meta *last = global_base;

		if (size < MMAP_THRESHOLD) {
			coalesce();
			block = find_free_block(size);
		}

		if (block == NULL) {
			block = request_space_malloc(last, size);
			if (block == NULL)
				return NULL;
		} else {
			block->status = STATUS_ALLOC;
		}
	}

	return (block + 1);
}

void os_free(void *ptr)
{
	if (ptr == NULL)
		return;

	struct block_meta *block_ptr = (struct block_meta *)ptr - 1;

	if (block_ptr->status == STATUS_MAPPED) {
		int ret = munmap(block_ptr, ALIGN(block_ptr->size + sizeof(struct block_meta)));

		DIE(ret == -1, "munmap");
	} else {
		block_ptr->status = STATUS_FREE;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (nmemb == 0 || size == 0)
		return NULL;

	size_t total_size = nmemb * size;

	if (total_size / nmemb != size)
		return NULL;

	struct block_meta *block = NULL;

	if (global_base == NULL) {
		block = request_space_calloc(NULL, nmemb * size);
		if (block == NULL)
			return NULL;
		global_base = block;

		memset(block + 1, 0, nmemb * size);
	} else {
		struct block_meta *last = global_base;

		if (nmemb * size < CALLOC_THRESHOLD) {
			coalesce();
			block = find_free_block(nmemb * size);
		}

		if (block == NULL) {
			block = request_space_calloc(last, nmemb * size);
			if (block == NULL)
				return NULL;
			memset(block + 1, 0, nmemb * size);
		} else {
			block->status = STATUS_ALLOC;
			memset(block + 1, 0, nmemb * size);
		}
	}

	return (block + 1);
}

void *os_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
		return os_malloc(size);

	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	struct block_meta *block;
	struct block_meta *new_block;
	struct block_meta *current = global_base;

	block = (struct block_meta *)ptr - 1;

	if (block->status == STATUS_FREE)
		return NULL;

	if (block->status == STATUS_MAPPED) {
		void *new_block = os_malloc(size);

		if (new_block == NULL)
			return NULL;

		memcpy(new_block + 1, block + 1, block->size);

		int ret = munmap(block, ALIGN(block->size + sizeof(struct block_meta)));

		DIE(ret == -1, "munmap");

		return new_block + 1;
	}

	int ok = 0;

	if (ALIGN(size) > ALIGN(block->size)) {
		while (block->size < ALIGN(size)) {
			if (block->next == NULL || block->next->status == STATUS_ALLOC) {
				ok = 1;
				break;
			}
			if (block->next != NULL) {
				if (block->next->status == STATUS_FREE) {
					block->size = ALIGN(block->size) + ALIGN(block->next->size) + ALIGN(sizeof(struct block_meta));
					block->next = block->next->next;
				}
			}
		}
		if (ok == 0)
			return block + 1;

		new_block = request_space_malloc(block, size);
		if (new_block == NULL)
			return NULL;
		memcpy(new_block + 1, block + 1, block->size);
		block->status = STATUS_FREE;
		return new_block + 1;
	}

	if (ALIGN(block->size) > ALIGN(size)) {
		split_block(block, size);
		return block + 1;
	}

	return NULL;
}
