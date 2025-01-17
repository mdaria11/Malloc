/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define DIE(assertion, call_description)						\
	do {										\
		if (assertion) {							\
			fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);		\
			perror(call_description);					\
			exit(errno);							\
		}									\
	} while (0)


#define ALIGNMENT 8 //8 bytes alignment for address
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1)) //align a size to a multiple of 8
#define SIZE_HEADER (ALIGN(sizeof(struct block_meta))) //size of header aligned

/* Structure to hold memory block metadata */
struct block_meta {
	size_t size;
	int status;
	struct block_meta *next;
};

/* Block metadata status values */
#define STATUS_FREE   0
#define STATUS_ALLOC  1
#define STATUS_MAPPED 2
