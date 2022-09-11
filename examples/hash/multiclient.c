/*
 * multiclient.c -- an example of client connecting and reading from multiple servers.
 *
 * The client in this example reads data from the remote memory of multiple servers to a local
 * volatile one.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <librpma.h>

#include "common-conn.h"

#ifdef TEST_USE_CMOCKA
#include "cmocka_headers.h"
#include "cmocka_alloc.h"
#endif

#ifdef TEST_MOCK_MAIN
#define main client_main
#endif

int
main(int argc, char *argv[])
{
	if (argc < 1) {
		fprintf(stderr, "usage: %s <server_address> <port>\n",
			argv[0]);
		exit(-1);
	}

	/* configure logging thresholds to see more details */
	rpma_log_set_threshold(RPMA_LOG_THRESHOLD, RPMA_LOG_LEVEL_INFO);
	rpma_log_set_threshold(RPMA_LOG_THRESHOLD_AUX, RPMA_LOG_LEVEL_INFO);

	/* parameters */
    char *addr = "192.168.101.19";
	char *port = "7777";

		/* parameters */
	char *addr1 = "192.168.103.17";
	char *port1 = "8888";

	/* resources - general */
	struct rpma_peer *peer = NULL;
	struct rpma_conn *conn = NULL;
	struct ibv_wc wc;

	/* resources - general */
	struct rpma_peer *peer1 = NULL;
	struct rpma_conn *conn1 = NULL;
	struct ibv_wc wc1;

	/*
	 * resources - memory regions:
	 * - src_* - a remote one which is a source for the read
	 * - dst_* - a local, volatile one which is a destination for the read
	 */
	void *dst_ptr = NULL;
	struct rpma_mr_local *dst_mr = NULL;
	struct rpma_mr_remote *src_mr = NULL;
	size_t src_size = 0;

	/*
	 * resources - memory regions:
	 * - src_* - a remote one which is a source for the read
	 * - dst_* - a local, volatile one which is a destination for the read
	 */
	void *dst_ptr1 = NULL;
	struct rpma_mr_local *dst_mr1 = NULL;
	struct rpma_mr_remote *src_mr1 = NULL;
	size_t src_size1 = 0;
	/*
	 * lookup an ibv_context via the address and create a new peer using it
	 */
	int ret = client_peer_via_address(addr, &peer);
	if (ret)
		return ret;


	/*
	 * lookup an ibv_context via the address and create a new peer using it
	 */
	int ret1 = client_peer_via_address(addr1, &peer1);
	if (ret1)
		return ret1;

	/* allocate a memory */
	dst_ptr = malloc_aligned(KILOBYTE);
	if (dst_ptr == NULL) {
		ret = -1;
		goto err_peer_delete;
	}


	/* allocate a memory */
	dst_ptr1 = malloc_aligned(KILOBYTE);
	if (dst_ptr1 == NULL) {
		ret1 = -1;
		goto err_peer_delete;
	}

	/* register the memory */
	ret = rpma_mr_reg(peer, dst_ptr, KILOBYTE, RPMA_MR_USAGE_READ_DST,
				&dst_mr);
	if (ret)
		goto err_mr_free;


	/* register the memory */
	ret1 = rpma_mr_reg(peer1, dst_ptr1, KILOBYTE, RPMA_MR_USAGE_READ_DST,
				&dst_mr1);
	if (ret1)
		goto err_mr_free;

	/* establish a new connection to a server listening at addr:port */
	ret = client_connect(peer, addr, port, NULL, NULL, &conn);
	if (ret)
		goto err_mr_dereg;


	/* establish a new connection to a server listening at addr:port */
	ret1 = client_connect(peer1, addr1, port1, NULL, NULL, &conn1);
	if (ret1)
		goto err_mr_dereg1;


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

	/* receive a memory info from the server */
	struct rpma_conn_private_data pdata1;
	ret1 = rpma_conn_get_private_data(conn1, &pdata1);
	if (ret1) {
		goto err_conn_disconnect1;
	} else if (pdata1.ptr == NULL) {
		fprintf(stderr,
				"The server has not provided a remote memory region. (the connection's private data is empty): %s",
				strerror(ret1));
		goto err_conn_disconnect1;
	}

	/*
	 * Create a remote memory registration structure from the received
	 * descriptor.
	 */
	struct common_data *dst_data = pdata.ptr;

	ret = rpma_mr_remote_from_descriptor(&dst_data->descriptors[0],
			dst_data->mr_desc_size, &src_mr);
	if (ret)
		goto err_conn_disconnect;

	/*
	 * Create a remote memory registration structure from the received
	 * descriptor.
	 */
	struct common_data *dst_data1 = pdata1.ptr;

	ret1 = rpma_mr_remote_from_descriptor(&dst_data1->descriptors[0],
			dst_data1->mr_desc_size, &src_mr1);
	if (ret1)
		goto err_conn_disconnect1;


	/* get the remote memory region size */
	ret = rpma_mr_remote_get_size(src_mr, &src_size);
	if (ret) {
		goto err_mr_remote_delete;
	} else if (src_size > KILOBYTE) {
		fprintf(stderr,
				"Remote memory region size too big to reading to the sink buffer of the assumed size (%zu > %d)\n",
				src_size, KILOBYTE);
		goto err_mr_remote_delete;
	}



	/* post an RDMA read operation */
	ret = rpma_read(conn, dst_mr, 0, src_mr, 0, src_size,
			RPMA_F_COMPLETION_ALWAYS, NULL);
	if (ret)
		goto err_mr_remote_delete;


	/* get the remote memory region size */
	ret1 = rpma_mr_remote_get_size(src_mr1, &src_size1);
	if (ret1) {
		goto err_mr_remote_delete1;
	} else if (src_size1 > KILOBYTE) {
		fprintf(stderr,
				"Remote memory region size too big to reading to the sink buffer of the assumed size (%zu > %d)\n",
				src_size1, KILOBYTE);
		goto err_mr_remote_delete1;
	}

	/* post an RDMA read operation */
	ret1 = rpma_read(conn1, dst_mr1, 0, src_mr1, 0, src_size1,
			RPMA_F_COMPLETION_ALWAYS, NULL);
	if (ret1)
		goto err_mr_remote_delete1;


	/* get the connection's main CQ */
	struct rpma_cq *cq = NULL;
	ret = rpma_conn_get_cq(conn, &cq);
	if (ret)
		goto err_mr_remote_delete;

	/* get the connection's main CQ */
	struct rpma_cq *cq1 = NULL;
	ret1 = rpma_conn_get_cq(conn1, &cq1);
	if (ret1)
		goto err_mr_remote_delete1;


	/* wait for the completion to be ready */
	ret = rpma_cq_wait(cq);
	if (ret)
		goto err_mr_remote_delete;


	/* wait for the completion to be ready */
	ret1 = rpma_cq_wait(cq1);
	if (ret1)
		goto err_mr_remote_delete1;

	/* wait for a completion of the RDMA read */
	ret = rpma_cq_get_wc(cq, 1, &wc, NULL);
	if (ret)
		goto err_mr_remote_delete;


	/* wait for a completion of the RDMA read */
	ret1 = rpma_cq_get_wc(cq1, 1, &wc1, NULL);
	if (ret1)
		goto err_mr_remote_delete1;


	if (wc.status != IBV_WC_SUCCESS) {
		ret = -1;
		(void) fprintf(stderr, "rpma_read() failed: %s\n",
				ibv_wc_status_str(wc.status));
		goto err_mr_remote_delete;
	}

		if (wc1.status != IBV_WC_SUCCESS) {
		ret1 = -1;
		(void) fprintf(stderr, "rpma_read() failed: %s\n",
				ibv_wc_status_str(wc1.status));
		goto err_mr_remote_delete1;
	}

	if (wc.opcode != IBV_WC_RDMA_READ) {
		ret = -1;
		(void) fprintf(stderr,
				"unexpected wc.opcode value (%d != %d)\n",
				wc.opcode, IBV_WC_RDMA_READ);
		goto err_mr_remote_delete;
	}
	if (wc1.opcode != IBV_WC_RDMA_READ) {
		ret1 = -1;
		(void) fprintf(stderr,
				"unexpected wc.opcode value (%d != %d)\n",
				wc1.opcode, IBV_WC_RDMA_READ);
		goto err_mr_remote_delete1;


	}
	(void) fprintf(stdout, "Read a message: %s\n", (char *)dst_ptr);
(void) fprintf(stdout, "Read a message: %s\n", (char *)dst_ptr1);

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


err_mr_remote_delete1:
	/* delete the remote memory region's structure */
	(void) rpma_mr_remote_delete(&src_mr1);

err_conn_disconnect1:
	(void) common_disconnect_and_wait_for_conn_close(&conn1);

err_mr_dereg1:
	/* deregister the memory region */
	(void) rpma_mr_dereg(&dst_mr1);

err_mr_free1:
	/* free the memory */
	free(dst_ptr1);

err_peer_delete1:
	/* delete the peer */
	(void) rpma_peer_delete(&peer1);


	return ret;
}