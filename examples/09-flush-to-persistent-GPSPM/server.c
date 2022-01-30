// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2022, Intel Corporation */
/* Copyright 2021-2022, Fujitsu */

/*
 * server.c -- a server of the flush-to-persistent-GPSPM example
 *
 * Please see README.md for a detailed description of this example.
 */

#include <inttypes.h>
#include <librpma.h>
#include <stdlib.h>
#include <stdio.h>
#include "common-conn.h"
#include "flush-to-persistent-GPSPM.h"

/* Generated by the protocol buffer compiler from: GPSPM_flush.proto */
#include "GPSPM_flush.pb-c.h"

#ifdef USE_LIBPMEM
#include <libpmem.h>
#define USAGE_STR "usage: %s <server_address> <port> [<pmem-path>]\n"PMEM_USAGE
#else
#define USAGE_STR "usage: %s <server_address> <port>\n"
#endif /* USE_LIBPMEM */

int
main(int argc, char *argv[])
{
	/* validate parameters */
	if (argc < 3) {
		fprintf(stderr, USAGE_STR, argv[0]);
		return -1;
	}

	/* configure logging thresholds to see more details */
	rpma_log_set_threshold(RPMA_LOG_THRESHOLD, RPMA_LOG_LEVEL_INFO);
	rpma_log_set_threshold(RPMA_LOG_THRESHOLD_AUX, RPMA_LOG_LEVEL_INFO);

	/* read common parameters */
	char *addr = argv[1];
	char *port = argv[2];
	int ret;

	/* resources - memory region */
	void *mr_ptr = NULL;
	size_t mr_size = 0;
	size_t data_offset = 0;
	struct rpma_mr_local *mr = NULL;

	/* messaging resources */
	void *msg_ptr = NULL;
	void *send_ptr = NULL;
	void *recv_ptr = NULL;
	struct rpma_mr_local *msg_mr = NULL;
	GPSPMFlushRequest *flush_req;
	GPSPMFlushResponse flush_resp = GPSPM_FLUSH_RESPONSE__INIT;
	size_t flush_resp_size = 0;

	int is_pmem = 0;

#ifdef USE_LIBPMEM
	char *pmem_path = NULL;
	if (argc >= 4) {
		pmem_path = argv[3];

		/* map the file */
		mr_ptr = pmem_map_file(pmem_path, 0 /* len */, 0 /* flags */,
				0 /* mode */, &mr_size, &is_pmem);
		if (mr_ptr == NULL) {
			(void) fprintf(stderr, "pmem_map_file() for %s "
					"failed\n", pmem_path);
			return -1;
		}

		/* pmem is expected */
		if (!is_pmem) {
			(void) fprintf(stderr, "%s is not an actual PMEM\n",
				pmem_path);
			(void) pmem_unmap(mr_ptr, mr_size);
			return -1;
		}

		/*
		 * At the beginning of the persistent memory, a signature is
		 * stored which marks its content as valid. So the length
		 * of the mapped memory has to be at least of the length of
		 * the signature to convey any meaningful content and be usable
		 * as a persistent store.
		 */
		if (mr_size < SIGNATURE_LEN) {
			(void) fprintf(stderr, "%s too small (%zu < %zu)\n",
					pmem_path, mr_size, SIGNATURE_LEN);
			(void) pmem_unmap(mr_ptr, mr_size);
			return -1;
		}
		data_offset = SIGNATURE_LEN;

		/*
		 * All of the space under the offset is intended for
		 * the string contents. Space is assumed to be at least 1 KiB.
		 */
		if (mr_size - data_offset < KILOBYTE) {
			fprintf(stderr, "%s too small (%zu < %zu)\n",
				pmem_path, mr_size, KILOBYTE + data_offset);
			(void) pmem_unmap(mr_ptr, mr_size);
			return -1;
		}

		/*
		 * If the signature is not in place the persistent content has
		 * to be initialized and persisted.
		 */
		if (strncmp(mr_ptr, SIGNATURE_STR, SIGNATURE_LEN) != 0) {
			/* write an initial empty string and persist it */
			char *ch = (char *)mr_ptr + data_offset;
			ch[0] = '\0';
			pmem_persist(ch, 1);
			/* write the signature to mark the content as valid */
			memcpy(mr_ptr, SIGNATURE_STR, SIGNATURE_LEN);
			pmem_persist(mr_ptr, SIGNATURE_LEN);
		}
	}
#endif /* USE_LIBPMEM */

	/* if no pmem support or it is not provided */
	if (mr_ptr == NULL) {
		(void) fprintf(stderr, NO_PMEM_MSG);
		mr_ptr = malloc_aligned(KILOBYTE);
		if (mr_ptr == NULL)
			return -1;

		mr_size = KILOBYTE;
	}

	/* allocate messaging buffer */
	msg_ptr = malloc_aligned(KILOBYTE);
	if (msg_ptr == NULL) {
		ret = -1;
		goto err_free;
	}
	send_ptr = (char *)msg_ptr + SEND_OFFSET;
	recv_ptr = (char *)msg_ptr + RECV_OFFSET;

	/* RPMA resources */
	struct rpma_peer *peer = NULL;
	struct rpma_ep *ep = NULL;
	struct rpma_conn_req *req = NULL;
	struct rpma_conn *conn = NULL;
	enum rpma_conn_event conn_event = RPMA_CONN_UNDEFINED;
	struct ibv_wc wc;

	/* if the string content is not empty */
	if (((char *)mr_ptr + data_offset)[0] != '\0') {
		(void) printf("Old value: %s\n", (char *)mr_ptr + data_offset);
	}

	/*
	 * lookup an ibv_context via the address and create a new peer using it
	 */
	if ((ret = server_peer_via_address(addr, &peer)))
		goto err_free;

	/* start a listening endpoint at addr:port */
	if ((ret = rpma_ep_listen(peer, addr, port, &ep)))
		goto err_peer_delete;

	/* register the memory */
	if ((ret = rpma_mr_reg(peer, mr_ptr, mr_size,
			RPMA_MR_USAGE_WRITE_DST |
			(is_pmem ? RPMA_MR_USAGE_FLUSH_TYPE_PERSISTENT :
				RPMA_MR_USAGE_FLUSH_TYPE_VISIBILITY),
			&mr)))
		goto err_ep_shutdown;

#ifdef USE_LIBPMEM
	/* rpma_mr_advise() should be called only in case of FsDAX */
	if (is_pmem && strstr(pmem_path, "/dev/dax") == NULL) {
		ret = rpma_mr_advise(mr, 0, mr_size,
			IBV_ADVISE_MR_ADVICE_PREFETCH_WRITE,
			IBV_ADVISE_MR_FLAG_FLUSH);
		if (ret)
			goto err_mr_dereg;
	}
#endif /* USE_LIBPMEM */

	/* register the messaging memory */
	if ((ret = rpma_mr_reg(peer, msg_ptr, KILOBYTE,
			RPMA_MR_USAGE_SEND | RPMA_MR_USAGE_RECV |
				RPMA_MR_USAGE_FLUSH_TYPE_VISIBILITY,
			&msg_mr))) {
		(void) rpma_mr_dereg(&mr);
		goto err_ep_shutdown;
	}

	/* get size of the memory region's descriptor */
	size_t mr_desc_size;
	ret = rpma_mr_get_descriptor_size(mr, &mr_desc_size);
	if (ret)
		goto err_mr_dereg;

	/* calculate data for the server read */
	struct common_data data = {0};
	data.data_offset = data_offset;
	data.mr_desc_size = mr_desc_size;

	/* get the memory region's descriptor */
	if ((ret = rpma_mr_get_descriptor(mr, &data.descriptors[0])))
		goto err_mr_dereg;

	struct rpma_conn_cfg *cfg = NULL;
	if ((ret = rpma_conn_cfg_new(&cfg)))
		goto err_mr_dereg;

	if ((ret = rpma_conn_cfg_set_rcq_size(cfg, RCQ_SIZE)))
		goto err_cfg_delete;

	/*
	 * Wait for an incoming connection request, accept it and wait for its
	 * establishment.
	 */
	struct rpma_conn_private_data pdata;
	pdata.ptr = &data;
	pdata.len = sizeof(struct common_data);

	/* receive an incoming connection request */
	if ((ret = rpma_ep_next_conn_req(ep, cfg, &req)))
		goto err_req_delete;

	/* prepare buffer for a flush request */
	if ((ret = rpma_conn_req_recv(req, msg_mr, RECV_OFFSET, MSG_SIZE_MAX,
			NULL)))
		goto err_req_delete;

	/* accept the connection request and obtain the connection object */
	if ((ret = rpma_conn_req_connect(&req, &pdata, &conn))) {
		(void) rpma_conn_req_delete(&req);
		goto err_req_delete;
	}

	/* wait for the connection to be established */
	ret = rpma_conn_next_event(conn, &conn_event);
	if (!ret && conn_event != RPMA_CONN_ESTABLISHED) {
		fprintf(stderr,
			"rpma_conn_next_event returned an unexpected event: %s\n",
			rpma_utils_conn_event_2str(conn_event));
		ret = -1;
	}
	if (ret)
		goto err_conn_delete;

	/* wait for the receive completion to be ready */
	struct rpma_cq *rcq = NULL;
	if ((ret = rpma_conn_get_rcq(conn, &rcq)))
		goto err_conn_delete;
	if ((ret = rpma_cq_wait(rcq)))
		goto err_conn_delete;
	if ((ret = rpma_cq_get_wc(rcq, 1, &wc, NULL)))
		goto err_conn_delete;

	/* validate the receive completion */
	if (wc.status != IBV_WC_SUCCESS) {
		ret = -1;
		(void) fprintf(stderr, "rpma_recv() failed: %s\n",
				ibv_wc_status_str(wc.status));
		goto err_conn_delete;
	}

	if (wc.opcode != IBV_WC_RECV) {
		ret = -1;
		(void) fprintf(stderr,
				"unexpected wc.opcode value "
				"(0x%" PRIXPTR " != 0x%" PRIXPTR ")\n",
				(uintptr_t)wc.opcode,
				(uintptr_t)IBV_WC_RECV);
		goto err_conn_delete;
	}

	/* unpack a flush request from the received buffer */
	flush_req = gpspm_flush_request__unpack(NULL, wc.byte_len, recv_ptr);
	if (flush_req == NULL) {
		fprintf(stderr, "Cannot unpack the flush request buffer\n");
		goto err_conn_delete;
	}
	(void) printf("Flush request received: {offset: 0x%" PRIXPTR
			", length: 0x%" PRIXPTR ", op_context: 0x%" PRIXPTR
			"}\n", flush_req->offset, flush_req->length,
			flush_req->op_context);

#ifdef USE_LIBPMEM
	void *op_ptr = (char *)mr_ptr + flush_req->offset;
	pmem_persist(op_ptr, flush_req->length);
#else
	(void) printf(
			"At this point, pmem_persist(3) should be called if libpmem will be in use\n");
#endif /* USE_LIBPMEM */

	/* prepare a flush response and pack it to a send buffer */
	flush_resp.op_context = flush_req->op_context;
	flush_resp_size = gpspm_flush_response__get_packed_size(&flush_resp);
	if (flush_resp_size > MSG_SIZE_MAX) {
		fprintf(stderr,
				"Size of the packed flush response is bigger than the available space of the send buffer (%"
				PRIu64 " > %u\n", flush_resp_size,
				MSG_SIZE_MAX);
		goto err_conn_delete;
	}
	(void) gpspm_flush_response__pack(&flush_resp, send_ptr);
	gpspm_flush_request__free_unpacked(flush_req, NULL);

	/* send the flush response */
	if ((ret = rpma_send(conn, msg_mr, SEND_OFFSET, flush_resp_size,
			RPMA_F_COMPLETION_ALWAYS, NULL)))
		goto err_conn_delete;

	/* wait for the send completion to be ready */
	struct rpma_cq *cq = NULL;
	if ((ret = rpma_conn_get_cq(conn, &cq)))
		goto err_conn_delete;
	if ((ret = rpma_cq_wait(cq)))
		goto err_conn_delete;
	if ((ret = rpma_cq_get_wc(cq, 1, &wc, NULL)))
		goto err_conn_delete;

	/* validate the send completion */
	if (wc.status != IBV_WC_SUCCESS) {
		ret = -1;
		(void) fprintf(stderr, "rpma_send() failed: %s\n",
				ibv_wc_status_str(wc.status));
		goto err_conn_delete;
	}

	if (wc.opcode != IBV_WC_SEND) {
		ret = -1;
		(void) fprintf(stderr,
				"unexpected wc.opcode value "
				"(0x%" PRIXPTR " != 0x%" PRIXPTR ")\n",
				(uintptr_t)wc.opcode,
				(uintptr_t)IBV_WC_SEND);
		goto err_conn_delete;
	}

	/*
	 * Wait for RPMA_CONN_CLOSED, disconnect and delete the connection
	 * structure.
	 */
	ret = common_wait_for_conn_close_and_disconnect(&conn);
	if (ret)
		goto err_conn_delete;

	(void) printf("New value: %s\n", (char *)mr_ptr + data_offset);

err_conn_delete:
	(void) rpma_conn_delete(&conn);

err_req_delete:
	(void) rpma_conn_req_delete(&req);

err_cfg_delete:
	(void) rpma_conn_cfg_delete(&cfg);

err_mr_dereg:
	(void) rpma_mr_dereg(&msg_mr);
	(void) rpma_mr_dereg(&mr);

err_ep_shutdown:
	/* shutdown the endpoint */
	(void) rpma_ep_shutdown(&ep);

err_peer_delete:
	/* delete the peer object */
	(void) rpma_peer_delete(&peer);

err_free:
	free(msg_ptr);

#ifdef USE_LIBPMEM
	if (is_pmem) {
		pmem_unmap(mr_ptr, mr_size);
		mr_ptr = NULL;
	}
#endif /* USE_LIBPMEM */

	if (mr_ptr != NULL)
		free(mr_ptr);

	return ret;
}
