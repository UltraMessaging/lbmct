/* min_ct_src.c - Connected Topics minimal example publisher
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
 * OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE LIABLE TO  * LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR INDIRECT
 * DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE TRANSACTIONS
 * CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF THE
 * LIKELIHOOD OF SUCH DAMAGES.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <unistd.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif
#include <lbm/lbm.h>
#include "lbmct.h"
#include "lbmct_private.h"
#include "prt.h"


/* State that app can be in. */
#define PRE_CREATE 0
#define CREATE_CALLED 1
#define DELETE_CALLED 2

int app_state = PRE_CREATE;


/* Very simplistic error handling macro.  Pass in return value of LBM function.
 * Prints error and aborts (core dump). */
#define LBM_ERR(_e) do {\
  if ((_e) != LBM_OK) {\
    fprintf(stderr, "LBM_ERR failed at %s:%d (%s)\n",\
       BASENAME(__FILE__), __LINE__, lbm_errmsg());\
    fflush(stderr);\
    abort();\
  }\
} while (0)


void *minsrc_src_conn_create_cb(lbmct_src_conn_t *src_conn,
  lbmct_peer_info_t *peer_info, void *src_clientd)
{
  char *s;

  /* As an example, capture the receiver's metadata and use it as the
   * connection state.
   */
  if (peer_info->flags & LBMCT_PEER_INFO_FLAGS_SRC_METADATA) {
    s = strdup(peer_info->rcv_metadata);
  }
  else {
    s = strdup("no metadata");
  }
  printf("conn create: peer='%s'\n", s);

  app_state = CREATE_CALLED;

  return s;
}  /* minsrc_src_conn_create_cb */


void minsrc_src_conn_delete_cb(lbmct_src_conn_t *src_conn,
  lbmct_peer_info_t *peer_info, void *src_clientd, void *conn_clientd)
{
  printf("conn delete: peer='%s'\n", conn_clientd);

  app_state = DELETE_CALLED;

  free(conn_clientd);
} /* minsrc_src_conn_delete_cb */


int main(int argc, char **argv) {
  lbm_context_attr_t *ctx_attr;
  lbm_context_t *ctx;
  lbmct_t *ct;
  lbmct_src_t *ct_src;
  lbm_src_t *um_src;
  int err;

  /* If config file supplied, read it. */
  if (argc > 1) {
    err = lbm_config(argv[1]);
    LBM_ERR(err);
  }

  /* Create context, making sure "response_tcp_nodelay" is set.
   */
  err = lbm_context_attr_create(&ctx_attr);
  LBM_ERR(err);
  err = lbm_context_attr_str_setopt(ctx_attr,
    "response_tcp_nodelay", "1");
  LBM_ERR(err);

  err = lbm_context_create(&ctx, ctx_attr, NULL, NULL);
  LBM_ERR(err);

  err = lbm_context_attr_delete(ctx_attr);
  LBM_ERR(err);

  /* Create CT object. */
  err = lbmct_create(&ct, ctx, NULL, "MinSrcMetadata", 15);
  LBM_ERR(err);


  /* Create connected source and wait connected. */
  err = lbmct_src_create(&ct_src, ct, "MinTopic", NULL, NULL,
    minsrc_src_conn_create_cb, minsrc_src_conn_delete_cb, NULL);
  LBM_ERR(err);

  um_src = lbmct_src_get_um_src(ct_src);

  while (app_state < CREATE_CALLED) {
    SLEEP_MSEC(100);
  }


  /* Send a message. */
  err = lbm_src_send(um_src, "Hello!", 7, LBM_MSG_FLUSH);
  LBM_ERR(err);

  /* Clean up. */
  err = lbmct_src_delete(ct_src);
  LBM_ERR(err);

  /* Wait for graceful close. */
  while (app_state < DELETE_CALLED) {
    SLEEP_MSEC(100);
  }

  err = lbmct_delete(ct);
  LBM_ERR(err);

  err = lbm_context_delete(ctx);
  LBM_ERR(err);

  return 0;
}  /* main */
