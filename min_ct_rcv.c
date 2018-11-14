/* min_ct_rcv.c - Connected Topics minimal example subscriber
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


/* State that app can be in. */
#define PRE_CREATE 0
#define CREATE_CALLED 1
#define DELETE_CALLED 2

int app_state = PRE_CREATE;


void *minrcv_rcv_conn_create_cb(lbmct_rcv_conn_t *rcv_conn,
  lbmct_peer_info_t *peer_info, void *rcv_clientd)
{
  char *s;

  /* As an example, capture the source's metadata and use it as the
   * connection state.
   */
  if (peer_info->flags & LBMCT_PEER_INFO_FLAGS_SRC_METADATA) {
    /* This is the receiver; print the source's metadata. */
    s = strdup(peer_info->src_metadata);
  }
  else {
    s = strdup("no metadata");
  }
  printf("conn create: peer='%s'\n", s); fflush(stdout);

  app_state = CREATE_CALLED;

  return s;
}  /* minrcv_rcv_conn_create_cb */


void minrcv_rcv_conn_delete_cb(lbmct_rcv_conn_t *rcv_conn,
  lbmct_peer_info_t *peer_info, void *rcv_clientd, void *conn_clientd)
{
  printf("conn delete: peer='%s'\n", conn_clientd); fflush(stdout);

  app_state = DELETE_CALLED;

  free(conn_clientd);
} /* minrcv_rcv_conn_delete_cb */


int min_rcv_cb(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
  int err;

  if (msg->type == LBM_MSG_DATA) {
    if (msg->properties == NULL) {
      printf("min_rcv_cb: user message: '%s', peer='%s', app_state=%d\n",
        msg->data, msg->source_clientd, app_state); fflush(stdout);
    }
    else {
      printf("min_rcv_cb: ignore ct handshake, peer='%s', app_state=%d\n",
        msg->source_clientd, app_state); fflush(stdout);
    }
  }  /* if msg type data */
  else {
    printf("min_rcv_cb: event=%d, peer='%s', app_state=%d\n",
      msg->type, msg->source_clientd, app_state); fflush(stdout);
  }

  return 0;
}  /* min_rcv_cb */


int main(int argc, char **argv) {
  lbm_context_attr_t *ctx_attr;
  lbm_context_t *ctx;
  lbmct_t *ct;
  lbmct_rcv_t *ct_rcv;
  int err;

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
  err = lbmct_create(&ct, ctx, NULL, "MinRcvMetadata", 15);
  LBM_ERR(err);


  /* Create connected receiver. */
  err = lbmct_rcv_create(&ct_rcv, ct, "MinTopic", NULL, min_rcv_cb,
    minrcv_rcv_conn_create_cb, minrcv_rcv_conn_delete_cb, NULL);
  LBM_ERR(err);


  /* Wait for graceful close. */
  while (app_state < DELETE_CALLED) {
    SLEEP_MSEC(100);
  }


  /* Clean up. */
  err = lbmct_rcv_delete(ct_rcv);
  LBM_ERR(err);

  err = lbmct_delete(ct);
  LBM_ERR(err);

  err = lbm_context_delete(ctx);
  LBM_ERR(err);

  return 0;
}  /* main */
