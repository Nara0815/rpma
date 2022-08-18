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

/*
 * Populate hashtable and write to PMEM
 */
int
main(int argc, const char *const argv[])
{

    if (argc != 2) {
		fprintf(stderr, "usage: %s load-file\n", argv[0]);
		exit(1);
	}

  
    char *path;
    char line[MAX_LINE_LENGTH] = {0};
    unsigned int line_count = 0;
   
    /*Delimiter for input file: cmd key*/
    const char s[2] = " "; 
    char *token;

    char *keys[KEY_NUMBERS] = {};


    int fd_pmem;
    int ret;
    void *val;

    path = argv[1];
    /* Open file */
    FILE *file = fopen(path, "r");
    
    if (!file)
    {
        perror(path);
        exit(1);
    }

    /* Get each line until there are none left */
    while (fgets(line, MAX_LINE_LENGTH, file))
    {
        /*Get command*/
        token = strtok(line, s);
        /*Get key*/
        token = strtok(NULL, s);

        keys[line_count] = token;
        /* Print each line */
        //printf("line[%06d]: %s\n", line_count, keys[line_count]);
        line_count++;
        
    
    }
    
    /* Close file */
    if (fclose(file))
    {
        perror(path);
        exit(1);
    }


    /*Process data from fd_load into vector*/


    struct hopscotch_hash_table *ht;

    /* Initialize with key length 8Byte*/
    ht = hopscotch_init(NULL, 32);
    if ( NULL == ht ) {
        return -1;
    }

    /* Insert */
   // ret = hopscotch_insert(ht, key1, key1);
   // if ( ret < 0 ) {
        /* Failed to insert */
   //   return -1;
   // }

    printf(line_count);

    /* Release */
    hopscotch_release(ht);

    return 0;
}
/*
    val = hopscotch_lookup(ht, key1);
    if ( val != key1 ) {*/
        /* Failed */
       // return -1;
   // }
   // int ret;

    /* Reset */
    //ret = 0;