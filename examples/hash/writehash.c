/*
 * testhash.c -- code to read from YCSB trace and populate the hopscotch hashtable
 *
 * Write the content of hashtable to pmem
 */

#include "hopscotch.h"
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


#define MAX_LINE_LENGTH 50
#define KEY_NUMBERS 1000000
#define TABLE_SIZE   21 // 2097152 buckets
/*
 * Populate hashtable and write to PMEM
 */
int
main(int argc, const char *const argv[])
{
    
    int fd; //pmem file
	struct pmem2_config *cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;
	pmem2_persist_fn persist;

    if (argc != 5) {
		fprintf(stderr, "usage: %s load-file pmem-file offset length\n ", argv[0]);
		exit(1);
	}

    /*YCSB trace file*/ 
    char *path;
    path = argv[1];

    /*PMEM parameters*/
	size_t offset = atoi(argv[2]);
	size_t user_length = atoi(argv[3]);
	size_t length = user_length;


    char line[MAX_LINE_LENGTH] = {0};
    unsigned int line_count = 0;
   
    /*Delimiter for input file: cmd key*/
    const char s[2] = " "; 
    char *token = " ";
    char *keys[KEY_NUMBERS] = {};

    int ret;
    void *val;


      /*Populate the hashtable*/
    struct hopscotch_hash_table *ht;
    unsigned int collision = 0;
    int value = 2008;
    size_t sz; 


    /* Open file */
    FILE *file = fopen(path, "r");
    
    if (!file)
    {
        perror(path);
        exit(1);
    }

        /* Initialize with key length 24 bit (32)*/
    ht = hopscotch_init(NULL, 24);
    if ( NULL == ht ) {
         return -1;
    }


    /* Get each line until there are none left */
    while (fgets(line, MAX_LINE_LENGTH, file))
    {
        /*Get command*/
        token = strtok(line, s);
    
        /*Get key*/
        token = strtok(NULL, s);

        keys[line_count] = strdup(token);

      //  ret = hopscotch_insert(ht, value, value);
        //printf("line[%06d]: %s\n", line_count, keys[line_count]);
        line_count++;
    }

    for(int i = 0; i < KEY_NUMBERS; i++){
        ret = hopscotch_insert(ht, keys[i], keys[i]);
        if (ret == -4){
            collision++; //# of relocated buckets
        }
    }



   // val = hopscotch_lookup(ht, keys[100471] );
   // if ( val == keys[100471] ) {
        /* Failed */
        //printf("not mstch %s", val);
       // printf("hop %d\n",ht->buckets[100471].hopinfo);
        //return -1;
    //}
   // printf("match %s", val);
    
    /*Write table data to PMEM*/
	if ((fd = open(argv[2], O_RDWR)) < 0) {
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
    size_t off;
    char hop[4];
    //char empty = "0";
    //char valid = "1";

    /* Total bucket size 64*/
    int keysize = 32;
    int valsize = 27;
    int hopsize = 4;
    int metasize = 1;

     
    /*Clean*/
    for ( int i = 0; i < 1000000000; i++ ) {
        strcpy(addr+i, " ");
    }

    sz = 1ULL << TABLE_SIZE;
    off = 0;

   
    for ( int i = 0; i < sz; i++ ) {
        
        if ( ht->buckets[i].key ) {

            strcpy(addr+off, "1");
            off+=metasize;

            sprintf(hop, "%d", ht->buckets[i].hopinfo);
            strcpy(addr+off, hop);
            off+=hopsize;

            strcpy(addr+off, ht->buckets[i].key);
            off+=keysize;

            strcpy(addr+off, ht->buckets[i].data);
            off+=valsize;
            
        }else{
            //empty slot 
            strcpy(addr+off, "0");
            off+=64;
        }
    }   

    printf("%d\n", off);

	/*for (size_t i = 0; i < 1000; i++) {
		printf("%c ", addr[i]);
		if ((i & 0x0F) == 0x0F)
			printf("\n");
	}
*/

    pmem2_map_delete(&map);
	pmem2_source_delete(&src);
	pmem2_config_delete(&cfg);
	close(fd);

    /* Release */
    hopscotch_release(ht);

    /* Close file */
    if (fclose(file))
    {
        perror(path);
        exit(1);
    }

    return 0;
}