// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "helpers.h"

#define MMAP_THRESHOLD (128*1024)
#define PAGE_SIZE 4096
struct block_meta *head; //Heap_Start; initially is NULL since we havent alloc'd anything

void coalesce_blocks(void) //function that coalesce the free blocks in our list
{
	struct block_meta *flag = head;

	if (flag->next == NULL) //we only have a block in our list
		return;

	while (flag != NULL) {
		if (flag->status == STATUS_FREE) {//we found a free block
			if (flag->next == NULL) //its the last one, so no coalesce
				break;
			if (flag->next->status == STATUS_FREE) {//the next block is free as well
				flag->size += flag->next->size + SIZE_HEADER; //the size increases
				flag->next = flag->next->next;
			} else {
				flag = flag->next;
			}
		} else {
			flag = flag->next;
		}
	}
}

void split_block(struct block_meta *block, size_t size) //function that splits block with first one of size "size"
{
	size_t block_size = block->size; //size of block
	struct block_meta *aux = block->next;

	block->size = size; //update the size with requested size
	block->next = (struct block_meta *)((void *)block + SIZE_HEADER + size); //add new block to the list
	block->status = STATUS_ALLOC;
	//update metadata for new block
	block->next->size = block_size - size - SIZE_HEADER;
	block->next->next = aux;
	block->next->status = STATUS_FREE;
}

void *find_best_fit(size_t size) //function that returns the best fit for requested size
{
	struct block_meta *flag = head;
	struct block_meta *best_fit = NULL;

	coalesce_blocks(); //make sure all free blocks are coalesced before start searching

	while (flag != NULL) {
		if (flag->status == STATUS_FREE) {//found a free block
			if (flag->size >= size) {//free block has enough size for us
				if (best_fit == NULL) {
					best_fit = flag;
				} else {//save the one with size closer to our requested size
					if (flag->size - size < best_fit->size - size)
						best_fit = flag;
				}
			}
		}
		flag = flag->next;
	}

	if (best_fit == NULL) //return NULL if we dont find any block
		return NULL;
	best_fit->status = STATUS_ALLOC; //we alloc the block
	if (best_fit->size - size >= SIZE_HEADER + 1) //split it if we can
		split_block(best_fit, size);

	return (void *)best_fit;
}

void *os_malloc(size_t size)
{
	if (size <= 0) //for invalid size we return NULL
		return NULL;

	struct block_meta *block;

	if (ALIGN(size) >= MMAP_THRESHOLD) {//we have to use mmap
		block = (struct block_meta *)mmap(NULL, SIZE_HEADER + ALIGN(size), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
		block->size = ALIGN(size);
		block->status = STATUS_MAPPED;
		return (void *)block + SIZE_HEADER;
	}

	if (head == NULL) {//first malloc ever in the program => we create the big 128KB free block
		head = sbrk(MMAP_THRESHOLD);
		head->size = MMAP_THRESHOLD - SIZE_HEADER;
		head->next = NULL;
		head->status = STATUS_FREE;
	}

	block = find_best_fit(ALIGN(size)); //returns the block

	if (block == NULL) {//we cant find a fit for our size
		struct block_meta *flag = head;

		while (flag->next != NULL)
			flag = flag->next;
		if (flag->status == STATUS_FREE) {//last block is free => expand it
			size_t expand_value = ALIGN(size) - flag->size;

			sbrk(expand_value);
			flag->size = flag->size + expand_value;
			flag->status = STATUS_ALLOC;

			return (void *)flag + SIZE_HEADER;
		}

		//if last block is not free => alloc a new one
		sbrk(SIZE_HEADER + ALIGN(size));
		block = (struct block_meta *)((void *)flag + SIZE_HEADER + flag->size);
		flag->next = block;

		block->next = NULL;
		block->size = ALIGN(size);
		block->status = STATUS_ALLOC;
	}

	return (void *)block + SIZE_HEADER;
}

void os_free(void *ptr)
{
	if (ptr == NULL) {//invalid pointer
		return;
	}

	struct block_meta *block = (struct block_meta *)(ptr - SIZE_HEADER);

	if (block->status == STATUS_ALLOC) {//the block we want to free is on heap
		block->status = STATUS_FREE;
		coalesce_blocks(); //coalesce blocks
	} else if (block->status == STATUS_MAPPED) {//the block is a mapped one
		munmap((void *)block, SIZE_HEADER + block->size);
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (nmemb*size <= 0) //for invalid size we return NULL
		return NULL;

	struct block_meta *block;

	if (ALIGN(nmemb*size) + SIZE_HEADER >= PAGE_SIZE) {//we have to use mmap
		block = (struct block_meta *)mmap(NULL, SIZE_HEADER + ALIGN(nmemb*size), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
		block->size = ALIGN(nmemb*size);
		block->status = STATUS_MAPPED;
		memset((void *)block+SIZE_HEADER, 0, nmemb*size);
		return (void *)block+SIZE_HEADER;
	}

	if (head == NULL) {//first calloc ever in the program => we create the big 128KB free block
		head = sbrk(MMAP_THRESHOLD);
		head->size = MMAP_THRESHOLD - SIZE_HEADER;
		head->next = NULL;
		head->status = STATUS_FREE;
	}

	block = find_best_fit(ALIGN(nmemb*size)); //returns the block

	if (block == NULL) {//we cant find a fit for our size
		struct block_meta *flag = head;

		while (flag->next != NULL)
			flag = flag->next;
		if (flag->status == STATUS_FREE) {
			size_t expand_value = ALIGN(nmemb*size) - flag->size;

			sbrk(expand_value);
			flag->size = flag->size + expand_value;
			flag->status = STATUS_ALLOC;
			memset((void *)flag + SIZE_HEADER, 0, nmemb*size);
			return (void *)flag + SIZE_HEADER;
		}

		sbrk(SIZE_HEADER + ALIGN(nmemb*size));
		block = (struct block_meta *)((void *)flag + SIZE_HEADER + flag->size);
		flag->next = block;
		block->next = NULL;
		block->size = ALIGN(nmemb*size);
		block->status = STATUS_ALLOC;
	}

	memset((void *)block + SIZE_HEADER, 0, nmemb*size);
	return (void *)block + SIZE_HEADER;
}

void *os_realloc(void *ptr, size_t size)
{
	if (ptr == NULL) //if ptr is NULL we just malloc the size
		return os_malloc(size);

	if (size == 0) {//if size is 0 we free the ptr
		os_free(ptr);
		return NULL;
	}

	struct block_meta *block = (struct block_meta *)(ptr - SIZE_HEADER);

	if (ALIGN(size) == block->size) //if aligned requested size is the same as size of block => we dont do anything
		return ptr;

	if (block->status == STATUS_FREE) //invalid block status
		return NULL;

	if (block->status == STATUS_MAPPED) {//if initial block is a mapped one => alloc one with malloc
		struct block_meta *newblock = (struct block_meta *)(os_malloc(size) - SIZE_HEADER);

		if (block->size < size) {//we copy the data from old block
			memcpy((void *)newblock+SIZE_HEADER, ptr, block->size);
		} else {
			memcpy((void *)newblock+SIZE_HEADER, ptr, size);
		}

		os_free(ptr); //free the old block
		return (void *)newblock + SIZE_HEADER;
	}

	//cases where old block is on heap

	if (ALIGN(size) < block->size) {//new size is smaller than old block size
		if (block->size - ALIGN(size) >= SIZE_HEADER + 1) //we just need to split the block if we can
			split_block(block, ALIGN(size));
		return ptr; //return the same pointer since we didnt change address
	}
	//new size is bigger than old size
	if (ALIGN(size) >= MMAP_THRESHOLD) {//new size bigger than threshold => new mapped block through malloc
		struct block_meta *newblock = (struct block_meta *)(os_malloc(size) - SIZE_HEADER);

		memcpy((void *)newblock + SIZE_HEADER, ptr, block->size);
		os_free(ptr);
		return (void *)newblock + SIZE_HEADER;
	}
	//new size smaller than MMAP_THRESHOLD
	coalesce_blocks(); //coalesce free blocks before search
	//the old block is the last one
	if (block->next == NULL) {
		struct block_meta *newblock = find_best_fit(ALIGN(size)); //try to find a better fit

		if (newblock == NULL) {//we didnt find anything => expand the block
			size_t expand_value = ALIGN(size) - block->size;

			sbrk(expand_value);
			block->size = block->size + expand_value;
			return ptr;
		}

		//we check if we can split the new found block
		if (newblock->size - ALIGN(size) >= SIZE_HEADER + 1)
			split_block(newblock, ALIGN(size));
		memcpy((void *)newblock + SIZE_HEADER, ptr, block->size);
		os_free(ptr);
		return (void *)newblock + SIZE_HEADER;
	}

	//the old block is not the last one and next block is a free one
	if (block->next->status == STATUS_FREE) {
		if (block->next->size + SIZE_HEADER + block->size >= ALIGN(size)) {//we can coalesce the 2 blocks
			size_t free_space = block->next->size + SIZE_HEADER + block->size - ALIGN(size);

			if (free_space >= SIZE_HEADER + 1) {//split the block if we can
				struct block_meta *aux = block->next->next;

				block->size = ALIGN(size);
				block->next = (void *)block + SIZE_HEADER + ALIGN(size);
				block->next->next = aux;
				block->next->size = free_space - SIZE_HEADER;
				block->next->status = STATUS_FREE;
			} else {//we just coalesce the 2 blocks together
				struct block_meta *aux = block->next->next;

				block->size = ALIGN(size);
				block->next = aux;
			}
			return ptr;
		}
	}
	//the next block is not a free one or the free block is not enough for us => we need to malloc
	struct block_meta *newblock = (struct block_meta *)(os_malloc(size) - SIZE_HEADER);

	memcpy((void *)newblock + SIZE_HEADER, ptr, block->size);
	os_free(ptr);
	return (void *)newblock + SIZE_HEADER;
}
