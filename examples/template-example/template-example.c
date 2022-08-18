// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * template-example.c -- template example
 */

/*int
main(int argc, char *argv[])
{
	return 0;
}*/

#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <libpmem2.h>
#include <librpma.h>
#include <stdlib.h>
#include <stdio.h>
#include "common-conn.h"
#include "common-hello.h"
#include "common-pmem_map_file.h"
#include "common-map_file_with_signature_check.h"
#include "common-pmem_map_file.h"

int
main(int argc, char *argv[])
{
	int fd;
	struct pmem2_config *cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;
	pmem2_persist_fn persist;

    if (argc != 4) {
		fprintf(stderr, "usage: %s src-file offset length\n", argv[0]);
		exit(1);
	}

	size_t offset = atoi(argv[2]);
	size_t user_length = atoi(argv[3]);
	size_t length = user_length;

	if ((fd = open(argv[1], O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

	if (pmem2_config_new(&cfg)) {
		pmem2_perror("pmem2_config_new");
		exit(1);
	}

	if (pmem2_source_from_fd(&src, fd)) {
		pmem2_perror("pmem2_source_from_fd");
		exit(1);
	}

	if (pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		exit(1);
	}

    size_t alignment;

	if (pmem2_source_alignment(src, &alignment)) {
		pmem2_perror("pmem2_source_alignment");
		exit(1);
	}else{
		printf("%d\n", alignment);
	}

	size_t offset_align = offset % alignment;

	if (offset_align != 0) {
		offset = offset - offset_align;
		length += offset_align;
	}

	size_t len_align = length % alignment;

	if (len_align != 0)
		length += (alignment - len_align);

	if (pmem2_config_set_offset(cfg, offset)) {
		pmem2_perror("pmem2_config_set_offset");
		exit(1);
	}

	if (pmem2_config_set_length(cfg, length)) {
		pmem2_perror("pmem2_config_set_length");
		exit(1);
	}

	if (pmem2_map_new(&map, cfg, src)) {
		pmem2_perror("pmem2_map_new");
		exit(1);
	}

    char *addr = pmem2_map_get_address(map);
	addr += offset_align;
	size_t size = pmem2_map_get_size(map);
     
	 //write to pmem
	//strcpy(addr, "hello, persistent memory");

	for (size_t i = 0; i < 20; i++) {
		printf("%c ", addr[i]);
		if ((i & 0x0F) == 0x0F)
			printf("\n");
	}
    pmem2_map_delete(&map);
	pmem2_source_delete(&src);
	pmem2_config_delete(&cfg);
	close(fd);
}
/*int main()
{

	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
    
    int fd;
    char *pmemaddr;
    int is_pmem;
  
	if ((fd = open("/dev/dax1.0", O_CREAT|O_RDWR, 0666)) < 0) {
		perror("open");
		exit(1);
	}
    if ((pmemaddr = pmem_map(fd)) == NULL) {
		perror("pmem_map");
		exit(1);
	}
	close(fd);
    //fallocate(fd, 0, 10 * 4096, 4 * 4096);

   // pmaddr = static_cast<char*>(mmap(NULL, 4194304, PROT_READ | PROT_WRITE, MAP_NORESERVE | MAP_SHARED_VALIDATE | MAP_SYNC, fd, 0));
   // pmaddr[0] = 'a';
    cout << "Hello World\n";
    return 0;
}*/