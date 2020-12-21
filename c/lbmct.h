/* lbmct.h - External definitions for Connected Topics.
 *
 * See https://github.com/UltraMessaging/lbmct
 *
 * Copyright (c) 2005-2019 Informatica Corporation. All Rights Reserved.
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
#include "prt.h"
#include "err.h"

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


/*! \brief Structure to hold an instance of Connected Topics object.

   A Connected Topics object is an active container for Connected
   Sources and Connected Receivers.
   A Connected Topics instance is associated with a UM context,
   and a UM context must not have more than one Connected Topics
   instance associated with it.
   A Connected Topics instance has an independent thread associated with it.

   See \ref lbmct_create().

   All CT receivers and sources associated with the lbmct object must be
   deleted before the lbmct itself can be deleted.
 */
#define HANDSHAKE_TOPIC_STR "LbmCt.h"
struct lbmct_t_stct {
  lbm_context_t *ctx;
};


/*! \brief Structure to hold an instance of Connected Topics configuration
           object.

    A configuration object is used by user application to specify operating
    parameters for a Connected Topics object.
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


LBMCT_API int lbmct_create(lbmct_t **ctp, lbm_context_t *ctx);
/*
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
*/

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif  /* LBMCT_H */
