/* lbmct.h - External definitions for Connected Topics.
 *
 * See https://github.com/UltraMessaging/lbmct
 *
 * Copyright (c) 2005-2018 Informatica Corporation. All Rights Reserved.
 * Permission is granted to licensees to use or alter this software for
 * any purpose, including commercial applications, according to the terms
 * laid out in the Software License Agreement.
 *
 * This source code example is provided by Informatica for educational
 * and evaluation purposes only.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
 * NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE UNINTERRUPTED
 * OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE LIABLE TO
 * LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR INDIRECT
 * DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE TRANSACTIONS
 * CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF THE
 * LIKELIHOOD OF SUCH DAMAGES.
 */

#ifndef LBMCT_H
#define LBMCT_H

/* This include files uses definitions from these includes. */
#include <lbm/lbm.h>
#include "prt.h"

#if defined(_WIN32)
#  ifdef LBMCT_EXPORTS
#    define LBMCT_API __declspec(dllexport)
#  else
#    define LBMCT_API __declspec(dllimport)
#  endif
#else
#  define LBMCT_API
#endif
#include "tmr.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Forward definitions. */
struct lbmct_t_stct;
typedef struct lbmct_t_stct lbmct_t;

struct lbmct_src_t_stct;
typedef struct lbmct_src_t_stct lbmct_src_t;

struct lbmct_src_conn_t_stct;
typedef struct lbmct_src_conn_t_stct lbmct_src_conn_t;

struct lbmct_rcv_t_stct;
typedef struct lbmct_rcv_t_stct lbmct_rcv_t;

struct lbmct_rcv_conn_t_stct;
typedef struct lbmct_rcv_conn_t_stct lbmct_rcv_conn_t;


/*!
 * Structure to pass important information to the application during
 * the connection create and delete callbacks.  The "err" field indicates
 * if the connection create/delete was clean and successful.
 * IMPORTANT: to maintain ABI compatibility:
 *   1. Do not remove fields from this structure.  Fields can be renamed
 *      to "unused1", "unused2", etc., but keep their original types.
 *   2. Do not change the data type of a field.  You can change it to "unused"
 *      and add a new one (see next item).
 *   3. Only add fields near the end, above the "lbmct_reserved[]" array.
 *      When a field is added, the equivilent number of bytes must be
 *      subtracted from the reserved array to keep the structure size the same.
 */
typedef struct {
  int status;  /* LBMCT_CONN_STATUS_* */
  lbm_uint32_t flags;  /* Bitmap of LBMCT_PEER_INFO_FLAGS_* */
  char *src_metadata;
  size_t src_metadata_len;
  char *rcv_metadata;
  size_t rcv_metadata_len;
  lbm_uint_t rcv_start_seq_num;  /* Receive-side sequence number of CRSP. */
  lbm_uint_t rcv_end_seq_num;  /* Receive-side sequence number or DRSP. */
  char rcv_source_name[LBM_MSG_MAX_SOURCE_LEN];  /* Not very useful to app. */
  char lbmct_reserved[160];  /* Reserved for future growth.  Set to zero. */
} lbmct_peer_info_t;


typedef void *(*lbmct_src_conn_create_function_cb)(lbmct_src_conn_t *src_conn,
  lbmct_peer_info_t *peer_info, void *clientd);
typedef void (*lbmct_src_conn_delete_function_cb)(lbmct_src_conn_t *src_conn,
  lbmct_peer_info_t *peer_info, void *clientd, void *conn_clientd);

typedef void *(*lbmct_rcv_conn_create_function_cb)(lbmct_rcv_conn_t *rcv_conn,
  lbmct_peer_info_t *peer_info, void *clientd);
typedef void (*lbmct_rcv_conn_delete_function_cb)(lbmct_rcv_conn_t *rcv_conn,
  lbmct_peer_info_t *peer_info, void *clientd, void *conn_clientd);


/* When lbmct is started with lbmct_create(), there are some optional
 * configuration options that can be specified.
 * See lbmct_process_config().
 */
#define LBMCT_CT_CONFIG_FLAGS_TEST_BITS  0x00000001
#define LBMCT_CT_CONFIG_FLAGS_DOMAIN_ID  0x00000002
#define LBMCT_CT_CONFIG_FLAGS_DELAY_CREQ 0x00000004
#define LBMCT_CT_CONFIG_FLAGS_RETRY_IVL  0x00000008
#define LBMCT_CT_CONFIG_FLAGS_MAX_TRIES  0x00000010
#define LBMCT_CT_CONFIG_FLAGS_PRE_DELIVERY 0x00000020

/* Default values for config options. */
#define LBMCT_CT_CONFIG_DEFAULT_TEST_BITS  0x00000000
#define LBMCT_CT_CONFIG_DEFAULT_DOMAIN_ID  -1
#define LBMCT_CT_CONFIG_DEFAULT_DELAY_CREQ 10    /* 10 ms */
#define LBMCT_CT_CONFIG_DEFAULT_RETRY_IVL  1000  /* 1 sec */
#define LBMCT_CT_CONFIG_DEFAULT_MAX_TRIES  5
#define LBMCT_CT_CONFIG_DEFAULT_PRE_DELIVERY 0

/* IMPORTANT: to maintain ABI compatibility:
 *   1. Do not remove fields from this structure.  Fields can be renamed
 *      to "unused1", "unused2", etc., but keep their original types.
 *   2. Do not change the data type of a field.  You can change it to "unused"
 *      and add a new one (see next item).
 *   3. Only add fields near the end, above the "lbmct_reserved[]" array.
 *      When a field is added, the equivilent number of bytes must be
 *      subtracted from the reserved array to keep the structure size the same.
 */
typedef struct lbmct_config_t_stct {
  lbm_uint32_t flags;  /* LBMCT_CONFIG_FLAGS_... */
  lbm_uint32_t test_bits;  /* Set bits for internal testing. */
  int domain_id;   /* Domain ID for context passed into lbmct_create(). */
  int delay_creq;  /* Time (in ms) to delay sending initial CREQ handshake. */
  int retry_ivl;   /* Timeout to retry a handshake. */
  int max_tries;   /* Give up after this many handshake tries. */
  int pre_delivery; /* Enables delivery of received msgs before handshakes. */
  char lbmct_reserved[128];  /* Reserved for future growth. */
} lbmct_config_t;


/* The "test_bits" config item is not for general application use.  It is
 * used to modify internal behavior for testing purposes.  Do not use for
 * normal operation.
 */
#define LBMCT_TEST_BITS_NO_CREQ 0x00000001
#define LBMCT_TEST_BITS_NO_CRSP 0x00000002
#define LBMCT_TEST_BITS_NO_C_OK 0x00000004
#define LBMCT_TEST_BITS_NO_DREQ 0x00000008
#define LBMCT_TEST_BITS_NO_DRSP 0x00000010
#define LBMCT_TEST_BITS_NO_D_OK 0x00000020
#define LBMCT_TEST_BITS_SKIP_BREAK 0x00000040
#define LBMCT_TEST_BITS_LOG_TICKS 0x00000080


LBMCT_API int lbmct_create(lbmct_t **ctp, lbm_context_t *ctx, lbmct_config_t *config,
  const char *metadata, size_t metadata_sz);
LBMCT_API int lbmct_delete(lbmct_t *ct);
LBMCT_API int lbmct_src_create(lbmct_src_t **ct_srcp, lbmct_t *ct, const char *topic_str,
  lbm_src_topic_attr_t *src_attr,
  lbm_src_cb_proc src_cb,
  lbmct_src_conn_create_function_cb src_conn_create_cb,
  lbmct_src_conn_delete_function_cb src_conn_delete_cb,
  void *clientd);
LBMCT_API lbm_src_t *lbmct_src_get_um_src(lbmct_src_t *ct_src);
LBMCT_API int lbmct_src_delete(lbmct_src_t *ct_src);
LBMCT_API int lbmct_rcv_create(lbmct_rcv_t **ct_rcvp, lbmct_t *ct, const char *topic_str,
  lbm_rcv_topic_attr_t *rcv_attr,
  lbm_rcv_cb_proc rcv_cb,
  lbmct_rcv_conn_create_function_cb rcv_conn_create_cb,
  lbmct_rcv_conn_delete_function_cb rcv_conn_delete_cb,
  void *clientd);
LBMCT_API int lbmct_rcv_delete(lbmct_rcv_t *ct_rcv);
LBMCT_API lbm_rcv_t *lbmct_rcv_get_um_rcv(lbmct_rcv_t *ct_rcv);
LBMCT_API void lbmct_debug_dump(lbmct_t *ct, const char *msg);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif  /* LBMCT_H */
