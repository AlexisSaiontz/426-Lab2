#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/mman.h>

#include "headers.h"

/*
	Note: sscanf reads from a char*, so it doesn't consume the input
*/

// Sizes in bytes
#define SUPERBLOCK (20)
#define LOG_ENTRY_BLOCK (4000)
#define REMAINING_BLOCK (3984)

// Definition of a 20B superblock
typedef struct superblock {
	uint64_t checksum;
	uint32_t generation;
	uint32_t log_start;
	uint32_t log_size;
} superblock;

// Definition of a 20B log entry
typedef struct log_entry {
        uint64_t node_a_id;
        uint64_t node_b_id;
        uint32_t opcode;
} log_entry;

// Definition of a 4KB log entry block
typedef struct log_entry_block {
	uint64_t checksum;
	uint32_t generation;
	uint32_t n_entries;

	// essentially a sequence of log_entry structs
	log_entry log_entries[199]; // In every log entry block, (4000 - 16) / 20 = 199.2
} log_entry_block;

// Returns malloced superblock read from disk
superblock* get_superblock(int fd) {
	lseek(fd, 0, SEEK_SET);
	superblock* new = mmap(NULL, SUPERBLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	read(fd, new, SUPERBLOCK);
	return new;
}

// Calculates and returns the checksum of the superblock
uint64_t checksum_superblock(void *bytes) {
	int i;
	uint64_t sum = 0;
	// skip checksum
	unsigned int *block = bytes + 8;

	// for all 8-byte words in block
	for (i = 0; SUPERBLOCK - 8 - i >= 8; i += 8) sum ^= *block++;
	// add constant to return value to avoid false positives
	return sum + 3;
}

// Calculates and returns the checksum of the log entry block
uint64_t checksum_log_entry_block(void *bytes) {
	int i;
	uint64_t sum = 0;
	// skip checksum
	unsigned int *block = bytes + 8;

	// for all 8-byte words in block
        for (i = 0; LOG_ENTRY_BLOCK - 8 - i > 0; i += 8) sum ^= *block++;
        // add constant to return value to avoid false positives
        return sum + 3;
}

// Writes superblock sup to disk
size_t write_superblock(int fd, superblock* sup) {
        lseek(fd, 0, SEEK_SET);
        sup->checksum = checksum_superblock(sup);

	// Debugging
	fprinf(stderr, "Generation: %llu, start: %llu, size: %llu", new->generation, new->log_start, new->log_size);

	return write(fd, sup, SUPERBLOCK);
}

// Returns true if checksum is equal to the XOR of all 8-byte words in superblock
bool valid_superblock(superblock *block, uint64_t checksum) {
	return checksum == checksum_superblock(block);
}

// Implements -f (fomrat) functionality
void format_superblock(int fd) {
	superblock* sup = get_superblock(fd);
	if (sup == NULL) {
		fprintf(stderr, "Failed to read superblock");
		exit(1);
	}

	if(valid_superblock(sup, sup->checksum)) {
		sup->generation = sup->generation + 1;
	} else {
		sup->generation = 0;
		sup->log_start = 1;
		sup->log_size = 2000000000;
	}
	write_superblock(fd, sup);
	free(sup);
}

/* FOR TESTING PURPOSES ONLY */
int main(int argc, char** argv) {
	int fd;

	fd = open("/dev/sdb", O_RDONLY);	

	struct superblock *new = mmap(NULL, SUPERBLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	printf("Read: %d\n", (int)read(fd, new, SUPERBLOCK));

	printf("Generation: %lu, checksum: %lu, logstart: %lu, logsize: %lu\n", (unsigned long) new->generation, (unsigned long) new->checksum, (unsigned long) new->log_start, (unsigned long) new->log_size);
	
	close(fd);
	if (valid_superblock(new, new->checksum)) {
		printf("Valid superblock!");
	} else {
		printf("Invalid");
	}
/*	
	if(sblock.generation == new.generation) printf("Generation\n");
	if(sblock.checksum == new.checksum) printf("Checksum\n");
	if(sblock.log_start == new.log_start) printf("Start\n");
	if(sblock.log_size == new.log_size) printf("Size\n");
	printf("%lu\n", sizeof(log_entry));*/

/*	fd = open("/dev/sdb", O_WRONLY);
*/
	return 0;
}

