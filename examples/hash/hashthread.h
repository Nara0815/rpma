#ifndef _HASHTHREAD_H
#define _HASHTHREAD_H

#include <stdint.h>
#include <stdlib.h>
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

#define MAX_LINE_LENGTH 50
#define KEY_NUMBERS 1000000
#define TABLE_SIZE   21 // 2097152 buckets
#define HOP_NUMBER   32

/* Arguments to pass to thread function */
typedef struct _thread_data_t {
  int tid;
  char **keys;
  int threadgroup;
  int start;
  int end;
  struct rpma_peer *peer;

  struct rpma_conn *conn;
  //struct rpma_mr_local *dst_mr;
  struct rpma_mr_remote *src_mr;
  //void *dst_ptr;
} thread_data_t;


typedef struct _thread_data_t thread_data_t;
void *thr_func(void *arg);
#endif /* _HASHTHREAD_H */