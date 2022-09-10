// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */
/* Copyright 2021-2022, Fujitsu */

/*
 * client.c -- a client that fetches hashtable parts from servers
 *
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
#include <inttypes.h>
#include <librpma.h>
#include <stdlib.h>
#include <stdio.h>
#include "common-conn.h"
#include "common-hello.h"
#include "common-map_file_with_signature_check.h"
#include "common-pmem_map_file.h"

#define MAX_LINE_LENGTH 50
#define KEY_NUMBERS 1000000
#define TABLE_SIZE   21 // 2097152 buckets
#define HOP_NUMBER   32


#ifdef USE_PMEM
#define USAGE_STR "usage: %s  [<pmem-path>]\n"PMEM_USAGE
#else
#define USAGE_STR "usage: %s\n"
#endif /* USE_PMEM */

int
main(int argc, char *argv[])
{
	/* validate parameters */
	if (argc < 2) {
		fprintf(stderr, "usage: %s load-file", argv[0]);
		exit(-1);
	}

    /*YCSB trace key file*/ 
    char *path;
    path = argv[1];

	/* configure logging thresholds to see more details */
	rpma_log_set_threshold(RPMA_LOG_THRESHOLD, RPMA_LOG_LEVEL_INFO);
	rpma_log_set_threshold(RPMA_LOG_THRESHOLD_AUX, RPMA_LOG_LEVEL_INFO);

	/* parameters for machine 16*/
	char *addr = "192.168.101.16";
	char *port = "7777";
	int ret;
	size_t src_size;

	/* resources - general */
	struct rpma_peer *peer = NULL;
	struct rpma_conn *conn = NULL;
	struct ibv_wc wc;

	/*
	 * resources - memory regions:
	 * - src_* - a remote one which is a source for the read
	 * - dst_* - a local, volatile one which is a destination for the read
	 */
	struct rpma_mr_local *dst_mr = NULL;
	struct rpma_mr_remote *src_mr = NULL;
	//size_t src_size = 0;

	/*memory buffer for read destination*/
	void *dst_ptr = NULL; 


	/*memory buffer for keys from parsed file*/
    char line[MAX_LINE_LENGTH] = {0};
    unsigned int line_count = 0;
   
    /*Delimiter for input file: cmd key*/
    const char s[2] = " "; 
    char *token = " ";
    char *keys[KEY_NUMBERS] = {};


	/*Hash parameters*/

	size_t sz; 
    size_t keylen = 24;
    size_t off;
    char hop[4];
    int keysize = 32;
    int valsize = 27;
    int hopsize = 4;
    int metasize = 1;
    char *valid = {'1'};
	char meta[1] = {};
	sz = 1ULL << TABLE_SIZE;


    /*Hash parameters*/
    uint32_t h;
    size_t idx;
    size_t i;
    size_t k;
    uint32_t hopinfo;
    char hopdest[4];
    char key[keylen];

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

        keys[line_count] = strdup(token);

      //  ret = hopscotch_insert(ht, value, value);
        //printf("line[%06d]: %s\n", line_count, keys[line_count]);
        line_count++;
    }

	/*
	 * lookup an ibv_context via the address and create a new peer using it
	 */
	ret = client_peer_via_address(addr, &peer);
	if (ret)
		return ret;

	/* allocate a memory */
	dst_ptr = malloc_aligned(2*KILOBYTE);
	if (dst_ptr == NULL) {
		ret = -1;
		goto err_peer_delete;
	}

	/* register the memory */
	ret = rpma_mr_reg(peer, dst_ptr, KILOBYTE, RPMA_MR_USAGE_READ_DST,
				&dst_mr);
	if (ret)
		goto err_mr_free;

	/* establish a new connection to a server listening at addr:port */
	ret = client_connect(peer, addr, port, NULL, NULL, &conn);
	if (ret)
		goto err_mr_dereg;

	/* receive a memory info from the server */
	struct rpma_conn_private_data pdata;
	ret = rpma_conn_get_private_data(conn, &pdata);
	if (ret) {
		goto err_conn_disconnect;
	} else if (pdata.ptr == NULL) {
		fprintf(stderr,
				"The server has not provided a remote memory region. (the connection's private data is empty): %s",
				strerror(ret));
		goto err_conn_disconnect;
	}

	/*
	 * Create a remote memory registration structure from the received
	 * descriptor.
	 */
	struct common_data *src_data = pdata.ptr;

	ret = rpma_mr_remote_from_descriptor(&src_data->descriptors[0],
			src_data->mr_desc_size, &src_mr);
	if (ret)
		goto err_conn_disconnect;

	/* get the remote memory region size */
	/*ret = rpma_mr_remote_get_size(src_mr, &src_size);
	if (ret) {
		goto err_mr_remote_delete;
	} else if (src_size > 2*KILOBYTE) {
		fprintf(stderr,
				"Remote memory region size too big to reading to the sink buffer of the assumed size (%zu > %d)\n",
				src_size, 2*KILOBYTE);
		goto err_mr_remote_delete;
	}*/


    /*Compute hash and post RDMA read operation*/
k=1;
    //for(k = 0; k < 10; k++){

        /*Compute hashcode of key and retrieve segment from PMEM*/

        h = _jenkins_hash(keys[k], keylen);

        idx = h & (sz - 1);

        //printf("%s----%lu--\n", keys[k], (unsigned long)idx);

		/* post an RDMA read operation */
		//ret = rpma_read(conn, dst_mr, 0, src_mr, src_data->data_offset+idx*64, 2*KILOBYTE,
			//RPMA_F_COMPLETION_ALWAYS, NULL);


            	ret = rpma_read(conn, dst_mr, 0, src_mr, 108399744, 2*KILOBYTE,
			RPMA_F_COMPLETION_ALWAYS, NULL);
	if (ret)
		goto err_mr_remote_delete;

		for(int i=0; i< 10; i++){
			

    (void) fprintf(stdout, "Read message: %c\n", ((char *)dst_ptr)[i]);
		}

		ret = rpma_read(conn, dst_mr, 0, src_mr, src_data->data_offset, 2*KILOBYTE,
			RPMA_F_COMPLETION_ALWAYS, NULL);

		if (ret)
			goto err_mr_remote_delete;

    (void) fprintf(stdout, "Read message: %c\n", ((char *)dst_ptr)[i]);

(void) fprintf(stdout, "Read a message: %s\n", (char *)dst_ptr);
        memcpy(&meta, (char *)&dst_ptr, metasize);

        printf( "Read message: %s\n", meta);

        //Checking if bucket empty
        if ( valid == (char *)dst_ptr) {
 	
        //Get hopinfo
            memcpy(&hopdest, (char *)&dst_ptr + metasize, hopsize);
            hopinfo = atoi(hopdest);


     /* 
      * If hopinfo equals to 1, return current bucket, else traverse through keys
      */

            if(hopinfo == 1){
                //Return current bucket
                
				memcpy(&key, (char *)&dst_ptr + metasize + hopsize, keylen);
                //memcpy(&key, &addr[idx*64 + metasize + hopsize], keylen);

                printf("%s\n", key);
                //return...

            }else{
            
                for ( i = 0; i < HOP_NUMBER; i++ ) {
                    if ( hopinfo & (1 << i) ) {
                        
						memcpy(&key, (char *)&dst_ptr + metasize + hopsize + i*64, keylen);
                        //memcpy(&key, &addr[(idx+i)*64 + metasize + hopsize], keylen);

                        if ( 0 == memcmp(keys[k], key, keylen-4) ) {
                        /* Found */
                        
                        //printf("found");
                        //printf("%s----%d\n", key, i);
                        //return ht->buckets[idx + i].data;
                        }
                    }
                } 
            }

        } else{
        //Key not found if bucket is empty
            printf("Not found\n");
        //return NULL;
        }
    //}

    

	/* post an RDMA read operation */
	/*ret = rpma_read(conn, dst_mr, 0, src_mr, src_data->data_offset, KILOBYTE,
			RPMA_F_COMPLETION_ALWAYS, NULL);
	if (ret)
		goto err_mr_remote_delete;

    (void) fprintf(stdout, "Read message: %s\n", (char *)dst_ptr);*/






	/* get the connection's main CQ */
	struct rpma_cq *cq = NULL;
	ret = rpma_conn_get_cq(conn, &cq);
	if (ret)
		goto err_mr_remote_delete;

	/* wait for the completion to be ready */
	ret = rpma_cq_wait(cq);
	if (ret)
		goto err_mr_remote_delete;

	/* wait for a completion of the RDMA read */
	ret = rpma_cq_get_wc(cq, 1, &wc, NULL);
	if (ret)
		goto err_mr_remote_delete;

	if (wc.status != IBV_WC_SUCCESS) {
		ret = -1;
		(void) fprintf(stderr, "rpma_read() failed: %s\n",
				ibv_wc_status_str(wc.status));
		goto err_mr_remote_delete;
	}

	if (wc.opcode != IBV_WC_RDMA_READ) {
		ret = -1;
		(void) fprintf(stderr,
				"unexpected wc.opcode value (%d != %d)\n",
				wc.opcode, IBV_WC_RDMA_READ);
		goto err_mr_remote_delete;
	}

	/*read from here*/

	//(void) fprintf(stdout, "Read message: %s\n", (char *)dst_ptr);

err_mr_remote_delete:
	/* delete the remote memory region's structure */
	(void) rpma_mr_remote_delete(&src_mr);

err_conn_disconnect:
	(void) common_disconnect_and_wait_for_conn_close(&conn);

err_mr_dereg:
	/* deregister the memory region */
	(void) rpma_mr_dereg(&dst_mr);

err_mr_free:
	/* free the memory */
	free(dst_ptr);

err_peer_delete:
	/* delete the peer */
	(void) rpma_peer_delete(&peer);

    /* Close file */
    if (fclose(file))
    {
        perror(path);
        exit(1);
    }

	return ret;
}
