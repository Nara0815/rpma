// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */
/* Copyright 2021-2022, Fujitsu */

/*
 * client.c -- a client that fetches hashtable parts from servers
 *
 */


#include "hopscotch.h"
#include "hashthread.h"
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
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include "common-conn.h"
#include "common-hello.h"
#include "common-map_file_with_signature_check.h"
#include "common-pmem_map_file.h"
#include <stdatomic.h>

#define MAX_LINE_LENGTH 50
#define KEY_NUMBERS 1000000
#define TABLE_SIZE   21 // 2097152 buckets
#define HOP_NUMBER   32
#define NUM_THREADS 2


#ifdef USE_PMEM
#define USAGE_STR "usage: %s <server_address> <port> [<pmem-path>]\n"PMEM_USAGE
#else
#define USAGE_STR "usage: %s <server_address> <port>\n"
#endif /* USE_PMEM */

atomic_int cnt;

void *thr_func(void *arg)
{
    /*Hash parameters*/
	size_t sz; 
    size_t keylen = 24;
    size_t off;
    char hop[4];
    int keysize = 32;
    int valsize = 27;
    int hopsize = 4;
    int metasize = 1;
	int metadata_size = 5;
    char hopdest[4];
    //char key[keylen];
    char *valid = {'1'};
	char meta[] = "0";
	char metadata[5];
	char key[keylen];
	uint32_t h;
    size_t idx;
    size_t i;
    size_t k;
    uint32_t hopinfo;
	sz = 1ULL << TABLE_SIZE;
	size_t index = 0;
    char **keys;
    struct rpma_conn *conn;
    struct rpma_mr_local *dst_mr;
    struct rpma_mr_remote *src_mr;
    void *dst_ptr; 
	int start, end;
	struct rpma_peer *peer;

	//struct ibv_wc wc;
    thread_data_t *data = (thread_data_t *)arg;
    keys = data->keys;
    int tid = data->tid;
    int threadgroup = data->threadgroup;
    conn = data->conn;
    //dst_mr = data->dst_mr;
    src_mr = data->src_mr;
	 
	 peer = data->peer;
    //dst_ptr = data->dst_ptr;
	start = data->start;
	end = data->end;
    int ret;
    struct ibv_wc wc;
    //printf("hello from thr_func, thread id: %d\n", tid*threadgroup);
    //printf(" %s\n", keys[tid*threadgroup+1]);
   
   	//struct rpma_mr_local *dst_mr = NULL;

	//size_t src_size = 0;

	/*memory buffer for read destination*/
	//void *dst_ptr = NULL; 

   	/* allocate a memory */
	dst_ptr = malloc_aligned(2*KILOBYTE);
	if (dst_ptr == NULL) {
		ret = -1;
		printf("dddlol");
		return NULL;
	}




	/* register the memory */
	ret = rpma_mr_reg(peer, dst_ptr, 2*KILOBYTE, RPMA_MR_USAGE_READ_DST,
				&dst_mr);
	if (ret){
		printf("dddlol");
		return NULL;}

 for(k = start; k < end; k++){
        /*Compute hashcode of key and retrieve segment from PMEM*/

        h = _jenkins_hash(keys[k], keylen);

        idx = h & (sz - 1);
		
       // printf("search %d-%d\n", tid, k);
		
		/* post an RDMA read operation */
		ret = rpma_read(conn, dst_mr, 0, src_mr, idx*64, 2*KILOBYTE,
			RPMA_F_COMPLETION_ALWAYS, NULL);
		if (ret){
			printf("read fail");
			pthread_exit(NULL);
		}

		
			//(void) rpma_mr_remote_delete(&src_mr);


    /*Completion queue to correctly receive ordered chars
	/* get the connection's main CQ */
	struct rpma_cq *cq = NULL;
	ret = rpma_conn_get_cq(conn, &cq);
	if (ret){printf("read fail");
		pthread_exit(NULL);}
		//(void) rpma_mr_remote_delete(&src_mr);

	/* wait for the completion to be ready */
	ret = rpma_cq_wait(cq);
	if (ret){printf("read fail");
		pthread_exit(NULL);}
		//(void) rpma_mr_remote_delete(&src_mr);

	/* wait for a completion of the RDMA read */
	ret = rpma_cq_get_wc(cq, 1, &wc, NULL);
	if (ret){printf("read fail");
		pthread_exit(NULL);}
		//(void) rpma_mr_remote_delete(&src_mr);

	if (wc.status != IBV_WC_SUCCESS) {
		ret = -1;
		(void) fprintf(stderr, "rpma_read() failed: %s\n",
				ibv_wc_status_str(wc.status));
		(void) rpma_mr_remote_delete(&src_mr);
	}

	if (wc.opcode != IBV_WC_RDMA_READ) {
		ret = -1;
		(void) fprintf(stderr,
				"unexpected wc.opcode value (%d != %d)\n",
				wc.opcode, IBV_WC_RDMA_READ);
		(void) rpma_mr_remote_delete(&src_mr);
	}


		/*for(int i=0; i<64; i++){
			
			
			printf( "Read message meta: %c\n", ((char *)dst_ptr)[i]);

  
		}*/

		
	
		for(int i=0; i<metadata_size; i++){
			
			metadata[i] = ((char *)dst_ptr)[i];
			//printf( "Read message meta: %c\n", metadata[i]);

    //(void) fprintf(stdout, "Read message: %c\n", ((char *)dst_ptr)[i]);
		}

		

		if ( valid == metadata[0]) {

        //Get hopinfo
            memcpy(&hopdest, &metadata[1], hopsize);
            hopinfo = atoi(hopdest);

		 
     /* 
      * If hopinfo equals to 1, return current bucket, else traverse through keys
      */

            if(hopinfo == 1){
                //Return current bucket

				for(int i=0; i<keylen; i++){

                   
			
					key[i] = ((char *)dst_ptr)[metasize+hopsize+i];
					//printf( "Read message meta: %c\n", key[i]);

    				//(void) fprintf(stdout, "Read message: %c\n", ((char *)dst_ptr)[i]);
				}

				cnt++;
				//printf("The atomic counter is %u\n", cnt);
                
//printf("found %s-%d-%d\n", key, tid, k);
                //printf("%s\n", key);
                //return...

            }else{
            
                for ( i = 0; i < HOP_NUMBER; i++ ) {
                    if ( hopinfo & (1 << i) ) {

						for(int j=0; j<keylen; j++){
			
							key[j] = ((char *)dst_ptr)[metasize+hopsize+i*64+j];

						}         


                        if ( 0 == memcmp(keys[k], key, keylen-4) ) {
                        /* Found */
				//printf("found %s-%d-%d\n", key, tid, k);
						cnt++;
						//printf("The atomic counter is %u\n", cnt);
                        
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
            return NULL;
        }
    }

	//(void) rpma_mr_dereg(&dst_mr);


	free(dst_ptr);
	//printf("before exit%d\n", tid);

    return NULL;
}


int
main(int argc, char *argv[])
{

	/* validate parameters */
	if (argc < 1) {
		fprintf(stderr, USAGE_STR, argv[0]);
		exit(-1);
	}

    /*YCSB trace key file*/ 
    char *path;
    path = "/home/nara/trace/c-load-1.0";

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
	int metadata_size = 5;
    char hopdest[4];
    //char key[keylen];
    char *valid = {'1'};
	char meta[] = "0";
	char metadata[5];
	char key[keylen];
	uint32_t h;
    size_t idx;
    size_t i;
    size_t k;
    uint32_t hopinfo;
	sz = 1ULL << TABLE_SIZE;
	size_t index = 0;

    /*Thread related parameters*/
    pthread_t thr[NUM_THREADS];
	/*Arguments to pass to thread*/
    thread_data_t thr_data[NUM_THREADS];
	int threadgroup = KEY_NUMBERS / NUM_THREADS; //number of queries per thread
    
	printf("threadgroup: %d\n", threadgroup);

    /* Open file */
    FILE *file = fopen(path, "r");
    
    if (!file)
    {
        perror(path);
        exit(1);
    }

    /* Get each line until there are none left */
    while (fgets(line, MAX_LINE_LENGTH, file) && line_count < KEY_NUMBERS)
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


	/* configure logging thresholds to see more details */
	rpma_log_set_threshold(RPMA_LOG_THRESHOLD, RPMA_LOG_LEVEL_INFO);
	rpma_log_set_threshold(RPMA_LOG_THRESHOLD_AUX, RPMA_LOG_LEVEL_INFO);

	/* parameters for machine 16*/
	char *addr = "192.168.101.16";
	char *port = "8004";
	int ret;


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

	/*
	 * lookup an ibv_context via the address and create a new peer using it
	 */
	ret = client_peer_via_address(addr, &peer);
	if (ret)
		return ret;



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

    struct timeval stop, start;
    gettimeofday(&start, NULL);
    /*Create threads*/
    int rc;
    for (int i = 0; i < NUM_THREADS; ++i) {
    	thr_data[i].tid = i;
		thr_data[i].threadgroup = threadgroup;
		thr_data[i].keys = &keys;
		thr_data[i].conn = conn;
		thr_data[i].peer = peer;
		thr_data[i].src_mr = src_mr;
        //thr_data[i].dst_ptr = dst_ptr;
		thr_data[i].start=i*threadgroup;
		thr_data[i].end=i*threadgroup+threadgroup;
		//printf(" %s\n", thr_data[i].keys[i*threadgroup+1]);
    	if ((rc = pthread_create(&thr[i], NULL, thr_func, &thr_data[i]))) {
      		fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
      		return EXIT_FAILURE;
    	}
    }
	printf("ye");
  	/* block until all threads complete */
  	for (i = 0; i < NUM_THREADS; ++i) {
    	pthread_join(thr[i], NULL);
  	}
printf("ddd");
	gettimeofday(&stop, NULL);

printf("ddd");


printf("took %lu us\n", (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec); 
printf("took %lu us per query\n", ((stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec)/cnt); 

printf("The atomic counter is %u\n", cnt);


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

	return ret;
}
