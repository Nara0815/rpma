// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * server.c -- a server that contains hashtable
 *
 *  The server acts as one of the endpoints that contain the hashtable in pmem and waits for the client to be connected
 */

#include <librpma.h>
#include <stdlib.h>
#include <stdio.h>
#include "common-conn.h"
#include "common-hello.h"
#include "common-map_file_with_signature_check.h"
#include "common-pmem_map_file.h"
#include "hopscotch.h"

#ifdef USE_PMEM
#define USAGE_STR "usage: %s <server_address> <port> [<pmem-path>]\n"PMEM_USAGE
#else
#define USAGE_STR "usage: %s <server_address> <port>\n"
#endif /* USE_PMEM */

//Per single RDMA read retrieve the whole segment
#define SEGMENT_SIZE 1024


int
main(int argc, char *argv[])
{
	/* validate parameters */
	if (argc < 3) {
		fprintf(stderr, USAGE_STR, argv[0]);
		exit(-1);
	}


	/* configure logging thresholds to see more details */
	rpma_log_set_threshold(RPMA_LOG_THRESHOLD, RPMA_LOG_LEVEL_INFO);
	rpma_log_set_threshold(RPMA_LOG_THRESHOLD_AUX, RPMA_LOG_LEVEL_INFO);

	/* parameters */
	char *addr = argv[1];
	char *port = argv[2];
    int ret;

	/* resources - general */
	struct rpma_peer *peer = NULL;
	struct rpma_ep *ep = NULL;
	struct rpma_conn *conn = NULL;

	/* resources - memory region */
    struct common_mem mem;
	memset(&mem, 0, sizeof(mem));
	struct rpma_mr_local *mr = NULL;

	//return the content in char array
	char segment[SEGMENT_SIZE];
	char *segpoint = segment;


#ifdef USE_PMEM
	if (argc >= 4) {
		char *path = argv[3];

		ret = common_pmem_map_file(path, 0, &mem);
		if (ret)
			goto err_free;
		segpoint = (uintptr_t)mem.mr_ptr + mem.data_offset;
	}
#endif /* USE_PMEM */
	/* if no pmem support or it is not provided */
	if (mem.mr_ptr == NULL) {
			return -1;
	}

    (void) printf("Next value: %s\n", segpoint);

	/*
	 * lookup an ibv_context via the address and create a new peer using it
	 */
	ret = server_peer_via_address(addr, &peer);
	if (ret)
		return ret;

	/* start a listening endpoint at addr:port */
	ret = rpma_ep_listen(peer, addr, port, &ep);
	if (ret)
		goto err_peer_delete;

	
	/* register the memory */
	ret = rpma_mr_reg(peer, mem.mr_ptr, mem.mr_size,
			RPMA_MR_USAGE_READ_SRC, &mr);
	if (ret)
		goto err_free;

	/* get size of the memory region's descriptor */
	size_t mr_desc_size;
	ret = rpma_mr_get_descriptor_size(mr, &mr_desc_size);
	if (ret)
		goto err_mr_dereg;

	struct common_data data = {0};
    data.data_offset = mem.data_offset;// + offsetof(struct hello_t, str);
	data.mr_desc_size = mr_desc_size;

	/* get the memory region's descriptor */
	ret = rpma_mr_get_descriptor(mr, &data.descriptors[0]);
	if (ret)
		goto err_mr_dereg;

	struct rpma_conn_private_data pdata;
	pdata.ptr = &data;
	pdata.len = sizeof(struct common_data);

	/*
	 * Wait for an incoming connection request, accept it and wait for its
	 * establishment.
	 */
	ret = server_accept_connection(ep, NULL, &pdata, &conn);
	if (ret)
		goto err_mr_dereg;

	/*
	 * Between the connection being established and the connection being
	 * closed the client will perform the RDMA read.
	 */

	/*
	 * Wait for RPMA_CONN_CLOSED, disconnect and delete the connection
	 * structure.
	 */
	(void) common_wait_for_conn_close_and_disconnect(&conn);

err_mr_dereg:
	/* deregister the memory region */
	(void) rpma_mr_dereg(&mr);

err_free:
#ifdef USE_PMEM
	if (mem.is_pmem) {
		common_pmem_unmap_file(&mem);
	} else
#endif /* USE_PMEM */
	if (mem.mr_ptr != NULL) {
		free(mem.mr_ptr);
	}

err_ep_shutdown:
	/* shutdown the endpoint */
	(void) rpma_ep_shutdown(&ep);

err_peer_delete:
	/* delete the peer object */
	(void) rpma_peer_delete(&peer);

	return ret;
}
