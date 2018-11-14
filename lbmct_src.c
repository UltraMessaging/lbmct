/* lbmct_src.c - Connected Topics code for source side.
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
#include "tmr.h"
#include "prt.h"


/* UM timer callback for ct source.  Submit "tick" command. */
int lbmct_src_timer_cb(tmr_t *tmr, lbm_context_t *ctx, const void *clientd)
{
  lbmct_src_conn_t *src_conn = (lbmct_src_conn_t *)clientd;
  lbmct_t *ct = NULL;
  lbmct_ctrlr_cmd_src_conn_tick_t *src_conn_tick = NULL;
  int err;

  /* Sanity checks. */
  ct = src_conn->ct;
  if (ct == NULL) EL_RTN("src_conn is corrupted", LBM_OK);
  if (ct->sig != LBMCT_SIG_CT) EL_RTN(E_BAD_SIG(ct), LBM_OK);

  /* Only send tick if we are still expecting this timer instance. */
  if (tmr->id == src_conn->pending_tmr_id) {
    /* Send tick command. */
    PRT_MALLOC_SET_N(src_conn_tick, lbmct_ctrlr_cmd_src_conn_tick_t, 0x5a, 1);
    src_conn_tick->tmr_id = tmr->id;
    src_conn_tick->src_conn = src_conn;

    err = lbmct_ctrlr_cmd_submit_nowait(ct,
      LBMCT_CTRLR_CMD_TYPE_SRC_CONN_TICK, src_conn_tick,
      LBMCT_CTRLR_CMD_NEEDS_FREE);
    if (err != LBM_OK) EL_RTN(lbm_errmsg(), LBM_OK);
  }

  return LBM_OK;
}  /* lbmct_src_timer_cb */


int lbmct_handshake_send_crsp(lbmct_src_conn_t *src_conn)
{
  lbmct_src_t *ct_src = NULL;
  lbmct_t *ct = NULL;
  lbm_src_t *um_src = NULL;
  char *crsp_msg = NULL;
  size_t crsp_msg_len = -1;
  int err;

  /* Sanity checks. */
  if (src_conn->sig != LBMCT_SIG_SRC_CONN) E_RTN(E_BAD_SIG(src_conn), -1);
  ct_src = src_conn->ct_src;
  if (ct_src == NULL) E_RTN("src_conn is corrupted", -1);
  if (ct_src->sig != LBMCT_SIG_CT_SRC) E_RTN(E_BAD_SIG(ct_src), -1);
  ct = src_conn->ct;
  if (ct == NULL) E_RTN("ct_src is corrupted", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  ASSRT(src_conn->peer_info.flags & LBMCT_PEER_INFO_FLAGS_SRC_METADATA);
  ASSRT(src_conn->peer_info.flags & LBMCT_PEER_INFO_FLAGS_SRC_METADATA_LEN);

  um_src = ct_src->um_src;

  PRT_MALLOC_N(crsp_msg, char, LBMCT_CRSP_MSG_BASE_SZ +
    src_conn->peer_info.src_metadata_len + 1);
  /* CRSP,field_count,rcv_ct_id,rcv_uim_addr,rcv_ct_id,
   *   src_ct_id,src_uim_addr,src_conn_id,
   *   meta_len<nul>metadata
   */
  crsp_msg_len = snprintf(crsp_msg, LBMCT_CRSP_MSG_BASE_SZ,
    "%s,9,%u,%s,%u,%u,%s,%u,%lu", LBMCT_CRSP_MSG_PREFIX,
    src_conn->rcv_ct_id, src_conn->rcv_uim_addr, src_conn->rcv_conn_id,
    ct->ct_id, src_conn->src_uim_addr, src_conn->src_conn_id,
    src_conn->peer_info.src_metadata_len);
  crsp_msg_len++;  /* Include final NULL. */
  if (crsp_msg_len > LBMCT_CRSP_MSG_BASE_SZ) {
    free(crsp_msg); E_RTN("crsp msg got too long", -1);
  }
  if (src_conn->peer_info.src_metadata != NULL &&
    src_conn->peer_info.src_metadata_len > 0)
  {
    memcpy(&crsp_msg[crsp_msg_len], src_conn->peer_info.src_metadata,
      src_conn->peer_info.src_metadata_len);
    crsp_msg_len += src_conn->peer_info.src_metadata_len;
  }

  if (! (ct->active_config.test_bits & LBMCT_TEST_BITS_NO_CRSP)) {
    /* Record sending CRSP in event buffer. */
    ct->recent_events[(ct->num_recent_events ++) % LBMCT_MAX_RECENT_EVENTS] =
      0x03000000 | 0x00000002;
    err = lbm_src_send_ex(ct_src->um_src, crsp_msg, crsp_msg_len, LBM_MSG_FLUSH,
      &ct->send_ex_info);
    if (err != LBM_OK) { free(crsp_msg); E_RTN(lbm_errmsg(), -1); }
  }
  else {
    lbm_logf(LBM_LOG_NOTICE,
      "LBMCT_TEST_BITS_NO_CRSP at %s:%d, skipping send of '%s' to %s\n",
      BASENAME(__FILE__), __LINE__, crsp_msg, ct_src->topic_str);
  }
  free(crsp_msg);

  return LBM_OK;
}  /* lbmct_handshake_send_crsp */


int lbmct_handshake_send_drsp(lbmct_src_conn_t *src_conn)
{
  lbmct_src_t *ct_src = NULL;
  lbmct_t *ct = NULL;
  lbm_src_t *um_src = NULL;
  char drsp_msg[LBMCT_DRSP_MSG_SZ+1];
  int drsp_len;
  int err;

  /* Sanity checks. */
  if (src_conn->sig != LBMCT_SIG_SRC_CONN) E_RTN(E_BAD_SIG(src_conn), -1);
  ct_src = src_conn->ct_src;
  if (ct_src == NULL) E_RTN("src_conn is corrupted", -1);
  if (ct_src->sig != LBMCT_SIG_CT_SRC) E_RTN(E_BAD_SIG(ct_src), -1);
  ct = src_conn->ct;
  if (ct == NULL) E_RTN("ct_src is corrupted", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  um_src = ct_src->um_src;

  /* DRSP,field_count,rcv_ct_id,rcv_uim_addr,rcv_ct_id,
   *   src_ct_id,src_uim_addr,src_conn_id
   */
  drsp_len = snprintf(drsp_msg, sizeof(drsp_msg),
    "%s,8,%u,%s,%u,%u,%s,%u", LBMCT_DRSP_MSG_PREFIX,
    src_conn->rcv_ct_id, src_conn->rcv_uim_addr, src_conn->rcv_conn_id,
    ct->ct_id, src_conn->src_uim_addr, src_conn->src_conn_id);
  drsp_len++;  /* Include final NULL. */

  if (! (ct->active_config.test_bits & LBMCT_TEST_BITS_NO_DRSP)) {
    /* Record sending DRSP in event buffer. */
    ct->recent_events[(ct->num_recent_events ++) % LBMCT_MAX_RECENT_EVENTS] =
      0x03000000 | 0x00000005;
    err = lbm_src_send_ex(ct_src->um_src, drsp_msg, drsp_len,
      LBM_MSG_FLUSH, &ct->send_ex_info);
    if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
  }
  else {
    lbm_logf(LBM_LOG_NOTICE,
      "LBMCT_TEST_BITS_NO_DRSP at %s:%d, skipping send of '%s' to %s\n",
      BASENAME(__FILE__), __LINE__, drsp_msg, ct_src->topic_str);
  }

  return LBM_OK;
}  /* lbmct_handshake_send_drsp */


int lbmct_src_conn_delete(lbmct_src_conn_t *src_conn)
{
  lbmct_src_t *ct_src = NULL;
  lbmct_t *ct = NULL;

  /* Sanity checks. */
  if (src_conn->sig != LBMCT_SIG_SRC_CONN) E_RTN(E_BAD_SIG(src_conn), -1);
  ct_src = src_conn->ct_src;
  if (ct_src == NULL) E_RTN("src_conn is corrupted", -1);
  if (ct_src->sig != LBMCT_SIG_CT_SRC) E_RTN(E_BAD_SIG(ct_src), -1);
  ct = src_conn->ct;
  if (ct == NULL) E_RTN("src_conn is corrupted", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  /* Only call the conn_delete callback if the conn_create was called. */
  if (src_conn->app_conn_create_called) {
    if (! src_conn->app_conn_delete_called) {
      (*ct_src->app_src_conn_delete_cb)(src_conn, &src_conn->peer_info,
        ct_src->app_src_clientd, src_conn->app_conn_clientd);
      src_conn->app_conn_delete_called = 1;
    }
  }

  mul_asl_remove_node(ct->src_conn_asl, src_conn->src_conn_asl_node);
  src_conn->src_conn_asl_node = NULL;

  /* Remove peer's metadata. */
  if (src_conn->peer_info.flags & LBMCT_PEER_INFO_FLAGS_RCV_METADATA) {
    if (src_conn->peer_info.rcv_metadata != NULL) {
      free(src_conn->peer_info.rcv_metadata);
    }
    src_conn->peer_info.flags &= ~LBMCT_PEER_INFO_FLAGS_RCV_METADATA;
    src_conn->peer_info.flags &= ~LBMCT_PEER_INFO_FLAGS_RCV_METADATA_LEN;
  }

  /* Remove from ct src. */
  LBMCT_LIST_DEL(ct_src->conn_list_head, src_conn, conn_list);

  /* The parent ct_src might be in the process of being deleted.  If so, and
   * this was the last src_conn, finish deleting the ct_src.
   */
  if (ct_src->exiting && ct_src->conn_list_head == NULL) {
    /* Remove from ct. */
    LBMCT_LIST_DEL(ct->src_list_head, ct_src, src_list);

    /* Delete the underlying UM source. */
    lbm_src_delete(ct_src->um_src);
    ct_src->um_src = NULL;

    PRT_VOL32(ct_src->sig) = LBMCT_SIG_DEAD;
    free(ct_src);
  }

  if (src_conn->tmr != NULL) {
    (void)tmr_cancel_sync(src_conn->tmr);
    (void)tmr_delete_sync(src_conn->tmr);
    src_conn->tmr = NULL;
  }

  PRT_VOL32(src_conn->sig) = LBMCT_SIG_DEAD;
  free(src_conn);

  return LBM_OK;
}  /* lbmct_src_conn_delete */


/* Public API to create a source object. */
int lbmct_src_create(lbmct_src_t **ct_srcp, lbmct_t *ct, const char *topic_str,
  lbm_src_topic_attr_t *src_attr,
  lbm_src_cb_proc src_cb,
  lbmct_src_conn_create_function_cb src_conn_create_cb,
  lbmct_src_conn_delete_function_cb src_conn_delete_cb,
  void *clientd)
{
  lbmct_ctrlr_cmd_ct_src_create_t ct_src_create;
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  ct_src_create.ct_src = NULL;
  ct_src_create.topic_str = topic_str;
  ct_src_create.src_attr = src_attr;
  ct_src_create.src_cb = src_cb;
  ct_src_create.src_conn_create_cb = src_conn_create_cb;
  ct_src_create.src_conn_delete_cb = src_conn_delete_cb;
  ct_src_create.src_clientd = clientd;

  /* Waits for the ct control thread to complete the operation. */
  err = lbmct_ctrlr_cmd_submit_and_wait(ct,
    LBMCT_CTRLR_CMD_TYPE_CT_SRC_CREATE, &ct_src_create);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

  *ct_srcp = ct_src_create.ct_src;

  return LBM_OK;
}  /* lbmct_src_create */


/* Public API to get the underlying UM source object. */
lbm_src_t *lbmct_src_get_um_src(lbmct_src_t *ct_src)
{
  return ct_src->um_src;
}  /* lbmct_src_get_um_src */


/* Public API to delete a source object. */
int lbmct_src_delete(lbmct_src_t *ct_src)
{
  lbmct_t *ct = NULL;
  lbmct_ctrlr_cmd_ct_src_delete_t ct_src_delete;
  int err;

  /* Sanity checks. */
  if (ct_src->sig != LBMCT_SIG_CT_SRC) E_RTN(E_BAD_SIG(ct_src), -1);
  ct = ct_src->ct;
  if (ct == NULL) E_RTN("ct_src is corrupted", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  ct_src_delete.ct_src = ct_src;

  /* Waits for the ct control thread to complete the operation. */
  err = lbmct_ctrlr_cmd_submit_and_wait(ct,
    LBMCT_CTRLR_CMD_TYPE_CT_SRC_DELETE, &ct_src_delete);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

  return LBM_OK;
}  /* lbmct_src_delete */


/* Internal function (not public API) to create the src_conn object.
 * Called from ct controller thread.
 */
int lbmct_src_conn_create(lbmct_src_conn_t **rtn_src_conn, lbmct_src_t *ct_src,
  const char *source_name, lbm_uint32_t rcv_ct_id, char *rcv_uim_addr,
  lbm_uint32_t rcv_conn_id, char *rcv_conn_id_str)
{
  lbmct_src_conn_t *src_conn = NULL;
  lbmct_t *ct = NULL;
  char ip_str[INET_ADDRSTRLEN];  /* Large enough to hold strerror_r(). */
  int err;

  /* Sanity checks. */
  if (ct_src->sig != LBMCT_SIG_CT_SRC) E_RTN(E_BAD_SIG(ct_src), -1);
  ct = ct_src->ct;
  if (ct == NULL) E_RTN("ct_src is corrupted", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  PRT_MALLOC_SET_N(src_conn, lbmct_src_conn_t, 0x5a, 1);
  src_conn->sig = LBMCT_SIG_SRC_CONN;

  err = tmr_create(&src_conn->tmr, ct->ctx);
  if (err != LBM_OK) {
    E(lbm_errmsg());
    free(src_conn);
    return -1;
  }

  src_conn->ct = ct;
  src_conn->src_conn_id = ct->next_conn_id;
  ct->next_conn_id ++;
  src_conn->ct_src = ct_src;
  src_conn->app_conn_clientd = ct_src->app_src_clientd;
  src_conn->state = LBMCT_CONN_STATE_STARTING;
  src_conn->conn_list_next = NULL;
  src_conn->conn_list_prev = NULL;
  src_conn->app_conn_create_called = 0;
  src_conn->app_conn_delete_called = 0;
  src_conn->try_cnt = 0;

  src_conn->peer_info.status = LBMCT_CONN_STATUS_OK;
  src_conn->peer_info.flags = 0;
  src_conn->peer_info.src_metadata = ct->metadata;
  src_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_SRC_METADATA;
  src_conn->peer_info.src_metadata_len = ct->metadata_len;
  src_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_SRC_METADATA_LEN;

  /* Assemble UIM address for this ct. */
  (void)mul_inet_ntop(ct->local_uim_addr.ip_addr, ip_str, sizeof(ip_str));
  snprintf(src_conn->src_uim_addr, sizeof(src_conn->src_uim_addr),
    "TCP:%u:%s:%u",
    ct->local_uim_addr.domain_id, ip_str, ct->local_uim_addr.port);

  memcpy(src_conn->rcv_source_name, source_name,
    sizeof(src_conn->rcv_source_name));
  src_conn->rcv_ct_id = rcv_ct_id;
  memcpy(src_conn->rcv_uim_addr, rcv_uim_addr,
    sizeof(src_conn->rcv_uim_addr));
  src_conn->rcv_conn_id = rcv_conn_id;
  memcpy(src_conn->rcv_conn_id_str, rcv_conn_id_str,
    sizeof(src_conn->rcv_conn_id_str));

  *rtn_src_conn = src_conn;
  return LBM_OK;
}  /* lbmct_src_conn_create */


/* Source-side handler for received creq handshake messages. */
int lbmct_src_handle_handshake_creq(lbmct_t *ct,
  lbmct_ctrlr_cmd_src_handshake_t *src_handshake)
{
  const char *msg_data = src_handshake->msg->data;
  size_t msg_len = src_handshake->msg->len;
  const char *source_name = src_handshake->msg->source;
  mul_asl_node_t *asl_node;
  lbmct_src_conn_t tst_src_conn;
  lbmct_src_conn_t *src_conn;
  lbmct_src_t tst_ct_src;
  lbmct_src_t *found_ct_src = NULL;
  char rcv_conn_id_str[LBMCT_RCV_CONN_ID_STR_SZ+1];
  int err, is_ok;
  /* Locals for parsing the received handshake msg. */
  char cmd[LBMCT_PREFIX_SZ+1];
  unsigned int field_cnt;
  unsigned int rcv_ct_id;
  char rcv_uim_addr[LBM_MSG_MAX_SOURCE_LEN+1];
  unsigned int rcv_conn_id;
  int topic_str_ofs = 0;
  const char *topic_str;

  if (msg_len > LBMCT_CREQ_MSG_SZ) E_RTN("Conn req msg too big", -1);
  if (msg_data[msg_len - 1] != '\0') E_RTN("Conn req msg: missing null", -1);

  /* Parse the incomming handshake message. */
  topic_str_ofs = 0;  /* Needed for validation. */
  sscanf(msg_data,
    "%" STRDEF(LBMCT_PREFIX_SZ) "[a-zA-Z0-9_],"  /* cmd */
    "%u,"  /* field_cnt */
    "%u,"  /* ct_id */
    "%" STRDEF(LBMCT_UIM_ADDR_STR_SZ) "[A-Z0-9:.],"  /* uim_addr */
    "%u,"  /* conn_id */
    "%n",  /* offset to topic name */
    cmd, &field_cnt, &rcv_ct_id, rcv_uim_addr, &rcv_conn_id, &topic_str_ofs);

  /* sscanf will only set the final %n offset if everything before it is OK. */
  if (topic_str_ofs == 0) E_RTN("Conn req msg: parse error", -1);
  if (msg_data[topic_str_ofs-1] != ',') E_RTN("Conn req msg: parse error", -1);

  topic_str = &msg_data[topic_str_ofs];
  if (topic_str[0] == '\0') E_RTN("Empty topic string", -1);

  /* Assemble a unique connection endpoint ID (used as key for asl). */
  snprintf(rcv_conn_id_str, sizeof(rcv_conn_id_str),
    "%u,%s,%u", rcv_ct_id, rcv_uim_addr, rcv_conn_id);

  /* See if we've already registered this connection. */
  memcpy(tst_src_conn.rcv_conn_id_str, rcv_conn_id_str,
    sizeof(tst_src_conn.rcv_conn_id_str));
  asl_node = mul_asl_find(ct->src_conn_asl, &tst_src_conn);
  if (asl_node != NULL) {
    /* Found existing connection, get the src_conn object. */
    src_conn = mul_asl_node_key(asl_node);
    found_ct_src = src_conn->ct_src;
    if (found_ct_src->exiting) E_RTN("Got creq on exiting src", -1);
  }  /* Existing connection. */
  else {  /* New connection. */
    if (mul_strnlen(source_name, sizeof(src_conn->rcv_source_name)) >=
      sizeof(src_conn->rcv_source_name))
    {
      E_RTN("source_name too long", -1);
    }

    /* New connection, find ct_src associated with it. */
    strncpy(tst_ct_src.topic_str, topic_str, sizeof(tst_ct_src.topic_str));
    asl_node = mul_asl_find(ct->ct_src_asl, &tst_ct_src);
    if (asl_node == NULL) E_RTN("Topic str not found in ct_src_asl", -1);
    found_ct_src = mul_asl_node_key(asl_node);
    if (found_ct_src == NULL) E_RTN("mul_asl_node_key returned NULL", -1);
    if (found_ct_src->exiting) E_RTN("Got creq on exiting src", -1);

    /* Create the connection. */
    err = lbmct_src_conn_create(&src_conn, found_ct_src, source_name,
      rcv_ct_id, rcv_uim_addr, rcv_conn_id, rcv_conn_id_str);
    if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

    is_ok = mul_asl_insert_and_retrieve_node(ct->src_conn_asl, src_conn,
      &src_conn->src_conn_asl_node);
    if (is_ok != 1) E_RTN("src_conn_asl insert failed", -1);  /* Impossible! */

    LBMCT_LIST_ADD(found_ct_src->conn_list_head, src_conn, conn_list);
  }  /* else new connection */

  if (src_conn->state == LBMCT_CONN_STATE_STARTING ||
      src_conn->state == LBMCT_CONN_STATE_RUNNING)
  {
    /* Each creq restarts the sequence of crsp retries. */
    src_conn->try_cnt = 0;
    src_conn->pending_tmr_id = -1;  /* Not expecting a tick. */
    (void)tmr_cancel_sync(src_conn->tmr);

    /* Send the CRSP handshake. */
    src_conn->try_cnt++;
    err = lbmct_handshake_send_crsp(src_conn);
    if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

    err = tmr_schedule(&src_conn->pending_tmr_id, src_conn->tmr,
      lbmct_src_timer_cb, src_conn, ct->active_config.retry_ivl);
    if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
  }
  else {
    lbm_logf(LBM_LOG_ERR, "Warning at %s:%d, received CREQ handshake when src_conn in state %d\n",
      BASENAME(__FILE__), __LINE__, (int)src_conn->state);
  }

  return LBM_OK;
}  /* lbmct_src_handle_handshake_creq */


/* Called from ct control thread. */
int lbmct_src_handle_handshake_c_ok(lbmct_t *ct,
  lbmct_ctrlr_cmd_src_handshake_t *src_handshake)
{
  const char *msg_data = src_handshake->msg->data;
  size_t msg_len = src_handshake->msg->len;
  char rcv_conn_id_str[LBMCT_RCV_CONN_ID_STR_SZ+1];
  lbmct_src_conn_t tst_src_conn;
  mul_asl_node_t *asl_node;
  lbmct_src_conn_t *src_conn = NULL;
  lbmct_src_t *ct_src = NULL;
  /* Locals for parsing the received handshake msg. */
  char cmd[LBMCT_PREFIX_SZ+1];
  unsigned int field_cnt;
  unsigned int rcv_ct_id;
  char rcv_uim_addr[LBM_MSG_MAX_SOURCE_LEN+1];
  unsigned int rcv_conn_id;
  unsigned int src_ct_id;
  char src_uim_addr[LBM_MSG_MAX_SOURCE_LEN+1];
  unsigned int src_conn_id;
  unsigned int start_sqn;
  int metadata_len;
  int metadata_ofs = 0;

  sscanf(msg_data,
    "%" STRDEF(LBMCT_PREFIX_SZ) "[a-zA-Z0-9_],"  /* cmd */
    "%u,"  /* field_cnt */
    "%u,"  /* rcv_ct_id */
    "%" STRDEF(LBMCT_UIM_ADDR_STR_SZ) "[A-Z0-9:.],"  /* rcv_uim_addr */
    "%u,"  /* rcv_conn_id */
    "%u,"  /* src_ct_id */
    "%" STRDEF(LBMCT_UIM_ADDR_STR_SZ) "[A-Z0-9:.],"  /* src_uim_addr */
    "%u,"  /* src_conn_id */
    "%u,"  /* start_sqn */
    "%u"  /* metadata_len */
    "%n",  /* offset to null */
    cmd, &field_cnt,
    &rcv_ct_id, rcv_uim_addr, &rcv_conn_id,
    &src_ct_id, src_uim_addr, &src_conn_id,
    &start_sqn, &metadata_len, &metadata_ofs);

  /* sscanf will only set the final %n offset if everything before is OK. */
  if (metadata_ofs == 0) E_RTN("Conn ok msg: parse error", -1);
  if (msg_data[metadata_ofs] != '\0') E_RTN("Conn ok msg: parse error", -1);
  metadata_ofs++;  /* Step past the nul. */
  if (metadata_ofs + metadata_len != msg_len) E_RTN("meta len mismatch", -1);

  /* Find the connection. */

  /* Assemble a unique connection endpoint ID (used as key for asl). */
  snprintf(rcv_conn_id_str, sizeof(rcv_conn_id_str),
    "%u,%s,%u", rcv_ct_id, rcv_uim_addr, rcv_conn_id);

  memcpy(tst_src_conn.rcv_conn_id_str, rcv_conn_id_str,
    sizeof(tst_src_conn.rcv_conn_id_str));
  asl_node = mul_asl_find(ct->src_conn_asl, &tst_src_conn);
  if (asl_node == NULL) E_RTN("connection not found", -1);
  src_conn = mul_asl_node_key(asl_node);

  if (src_conn->sig != LBMCT_SIG_SRC_CONN) E_RTN(E_BAD_SIG(src_conn), -1);
  if (src_conn->ct != ct) E_RTN("Internal error, ct mismatch", -1);
  ct_src = src_conn->ct_src;
  if (ct_src == NULL) E_RTN("src_conn is corrupted", -1);
  if (ct_src->sig != LBMCT_SIG_CT_SRC) E_RTN(E_BAD_SIG(ct_src), -1);
  if (ct_src->exiting) E_RTN("Got c_ok on exiting src", -1);

  if (src_conn->state == LBMCT_CONN_STATE_STARTING ||
      src_conn->state == LBMCT_CONN_STATE_RUNNING) {

    if (src_conn->state == LBMCT_CONN_STATE_STARTING) {
      /* Save connection info from receiver. */
      src_conn->rcv_ct_id = rcv_ct_id;
      memcpy(src_conn->rcv_uim_addr, rcv_uim_addr,
        sizeof(src_conn->rcv_uim_addr));
      src_conn->rcv_conn_id = rcv_conn_id;

      src_conn->peer_info.rcv_start_seq_num = start_sqn;
      src_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_RCV_START_SEQ_NUM;

      if (metadata_len > 0) {
        PRT_MALLOC_N(src_conn->peer_info.rcv_metadata, char, metadata_len);
        memcpy(src_conn->peer_info.rcv_metadata, &msg_data[metadata_ofs],
          metadata_len);
      } else {
        src_conn->peer_info.rcv_metadata = NULL;
      }
      src_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_RCV_METADATA;
      src_conn->peer_info.rcv_metadata_len = metadata_len;
      src_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_RCV_METADATA_LEN;

      src_conn->state = LBMCT_CONN_STATE_RUNNING;

      src_conn->pending_tmr_id = -1;  /* Not expecting a tick. */
      (void)tmr_cancel_sync(src_conn->tmr);

      /* Call the application's per-connection clientd create function. */
      src_conn->app_conn_clientd =
        (*ct_src->app_src_conn_create_cb)(src_conn, &src_conn->peer_info,
        ct_src->app_src_clientd);
      src_conn->app_conn_create_called = 1;

      /* Cancel whatever timer might be running now. */
      src_conn->pending_tmr_id = -1;
      (void)tmr_cancel_sync(src_conn->tmr);
    }  /* if state == starting */
    else {  /* state == running */
      /* Would not normally happen except in a retry. */
      lbm_logf(LBM_LOG_ERR, "Warning at %s:%d, received C_OK handshake when src_conn in state %d\n",
        BASENAME(__FILE__), __LINE__, (int)src_conn->state);
    }
  }  /* if state = starting or running */
  else {
    lbm_logf(LBM_LOG_ERR, "Warning at %s:%d, received C_OK handshake when src_conn in state %d\n",
      BASENAME(__FILE__), __LINE__, (int)src_conn->state);
  }

  return LBM_OK;
}  /* lbmct_src_handle_handshake_c_ok */


/* Called from ct control thread. */
int lbmct_src_handle_handshake_dreq(lbmct_t *ct,
  lbmct_ctrlr_cmd_src_handshake_t *src_handshake)
{
  const char *msg_data = src_handshake->msg->data;
  char rcv_conn_id_str[LBMCT_RCV_CONN_ID_STR_SZ+1];
  lbmct_src_conn_t tst_src_conn;
  mul_asl_node_t *asl_node;
  lbmct_src_conn_t *src_conn = NULL;
  lbmct_src_t *ct_src = NULL;
  int err;
  /* Locals for parsing the received handshake msg. */
  char cmd[LBMCT_PREFIX_SZ+1];
  unsigned int field_cnt;
  unsigned int rcv_ct_id;
  char rcv_uim_addr[LBM_MSG_MAX_SOURCE_LEN+1];
  unsigned int rcv_conn_id;
  unsigned int src_ct_id;
  char src_uim_addr[LBM_MSG_MAX_SOURCE_LEN+1];
  unsigned int src_conn_id;
  int null_ofs = 0;

  sscanf(msg_data,
    "%" STRDEF(LBMCT_PREFIX_SZ) "[a-zA-Z0-9_],"  /* cmd */
    "%u,"  /* field_cnt */
    "%u,"  /* rcv_ct_id */
    "%" STRDEF(LBMCT_UIM_ADDR_STR_SZ) "[A-Z0-9:.],"  /* rcv_uim_addr */
    "%u,"  /* rcv_conn_id */
    "%u,"  /* src_ct_id */
    "%" STRDEF(LBMCT_UIM_ADDR_STR_SZ) "[A-Z0-9:.],"  /* src_uim_addr */
    "%u"  /* src_conn_id */
    "%n",  /* offset to null */
    cmd, &field_cnt,
    &rcv_ct_id, rcv_uim_addr, &rcv_conn_id,
    &src_ct_id, src_uim_addr, &src_conn_id,
    &null_ofs);

  /* sscanf will only set the final %n offset if everything before is OK. */
  if (null_ofs == 0) E_RTN("Conn ok msg: parse error", -1);

  /* Find the connection. */

  /* Assemble a unique connection endpoint ID (used as key for asl). */
  snprintf(rcv_conn_id_str, sizeof(rcv_conn_id_str),
    "%u,%s,%u", rcv_ct_id, rcv_uim_addr, rcv_conn_id);

  memcpy(tst_src_conn.rcv_conn_id_str, rcv_conn_id_str,
    sizeof(tst_src_conn.rcv_conn_id_str));
  asl_node = mul_asl_find(ct->src_conn_asl, &tst_src_conn);
  if (asl_node == NULL) E_RTN("connection not found", -1);
  src_conn = mul_asl_node_key(asl_node);

  /* Sanity checks. */
  if (src_conn->sig != LBMCT_SIG_SRC_CONN) E_RTN(E_BAD_SIG(src_conn), -1);
  if (src_conn->ct != ct) E_RTN("Internal error, ct mismatch", -1);
  ct_src = src_conn->ct_src;
  if (ct_src == NULL) E_RTN("src_conn is corrupted", -1);
  if (ct_src->sig != LBMCT_SIG_CT_SRC) E_RTN(E_BAD_SIG(ct_src), -1);
  
  if (src_conn->state == LBMCT_CONN_STATE_STARTING ||
    src_conn->state == LBMCT_CONN_STATE_RUNNING ||
    src_conn->state == LBMCT_CONN_STATE_ENDING)
  {
    /* Each creq restarts the sequence of crsp retries. */
    src_conn->try_cnt = 0;
    src_conn->pending_tmr_id = -1;  /* Not expecting a tick. */
    (void)tmr_cancel_sync(src_conn->tmr);

    /* Flag connection as closing. */
    src_conn->state = LBMCT_CONN_STATE_ENDING;

    /* Send DRSP handshake. */
    src_conn->try_cnt++;
    err = lbmct_handshake_send_drsp(src_conn);
    if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

    /* Time out waiting for D_OK. */
    err = tmr_schedule(&src_conn->pending_tmr_id, src_conn->tmr,
      lbmct_src_timer_cb, src_conn, ct->active_config.retry_ivl);
    if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
  }
  else {
    lbm_logf(LBM_LOG_ERR, "Warning at %s:%d, received DREQ handshake when src_conn in state %d\n",
      BASENAME(__FILE__), __LINE__, (int)src_conn->state);
  }

  return LBM_OK;
}  /* lbmct_src_handle_handshake_dreq */


/* Called from ct control thread. */
int lbmct_src_handle_handshake_d_ok(lbmct_t *ct,
  lbmct_ctrlr_cmd_src_handshake_t *src_handshake)
{
  const char *msg_data = src_handshake->msg->data;
  char rcv_conn_id_str[LBMCT_RCV_CONN_ID_STR_SZ+1];
  lbmct_src_conn_t tst_src_conn;
  mul_asl_node_t *asl_node;
  lbmct_src_conn_t *src_conn = NULL;
  lbmct_src_t *ct_src = NULL;
  int err;
  /* Locals for parsing the received handshake msg. */
  char cmd[LBMCT_PREFIX_SZ+1];
  unsigned int field_cnt;
  unsigned int rcv_ct_id;
  char rcv_uim_addr[LBM_MSG_MAX_SOURCE_LEN+1];
  unsigned int rcv_conn_id;
  unsigned int src_ct_id;
  char src_uim_addr[LBM_MSG_MAX_SOURCE_LEN+1];
  unsigned int src_conn_id;
  unsigned int end_sqn;
  int null_ofs = 0;

  sscanf(msg_data,
    "%" STRDEF(LBMCT_PREFIX_SZ) "[a-zA-Z0-9_],"  /* cmd */
    "%u,"  /* field_cnt */
    "%u,"  /* rcv_ct_id */
    "%" STRDEF(LBMCT_UIM_ADDR_STR_SZ) "[A-Z0-9:.],"  /* rcv_uim_addr */
    "%u,"  /* rcv_conn_id */
    "%u,"  /* src_ct_id */
    "%" STRDEF(LBMCT_UIM_ADDR_STR_SZ) "[A-Z0-9:.],"  /* src_uim_addr */
    "%u,"  /* src_conn_id */
    "%u"  /* end_sqn */
    "%n",  /* offset to null */
    cmd, &field_cnt,
    &rcv_ct_id, rcv_uim_addr, &rcv_conn_id,
    &src_ct_id, src_uim_addr, &src_conn_id,
    &end_sqn, &null_ofs);

  /* sscanf will only set the final %n offset if everything before is OK. */
  if (null_ofs == 0) E_RTN("Conn ok msg: parse error", -1);

  /* Find the connection. */

  /* Assemble a unique connection endpoint ID (used as key for asl). */
  snprintf(rcv_conn_id_str, sizeof(rcv_conn_id_str),
    "%u,%s,%u", rcv_ct_id, rcv_uim_addr, rcv_conn_id);

  memcpy(tst_src_conn.rcv_conn_id_str, rcv_conn_id_str,
    sizeof(tst_src_conn.rcv_conn_id_str));
  asl_node = mul_asl_find(ct->src_conn_asl, &tst_src_conn);
  if (asl_node == NULL) E_RTN("connection not found", -1);
  src_conn = mul_asl_node_key(asl_node);

  if (src_conn->sig != LBMCT_SIG_SRC_CONN) E_RTN(E_BAD_SIG(src_conn), -1);
  if (src_conn->ct != ct) E_RTN("Internal error, ct mismatch", -1);
  ct_src = src_conn->ct_src;
  if (ct_src == NULL) E_RTN("src_conn is corrupted", -1);
  if (ct_src->sig != LBMCT_SIG_CT_SRC) E_RTN(E_BAD_SIG(ct_src), -1);

  src_conn->peer_info.rcv_end_seq_num = end_sqn;
  src_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_RCV_END_SEQ_NUM;

  if (src_conn->state != LBMCT_CONN_STATE_ENDING) {
    lbm_logf(LBM_LOG_ERR, "Warning at %s:%d, received D_OK handshake when src_conn in state %d\n",
      BASENAME(__FILE__), __LINE__, (int)src_conn->state);
  }

  /* Cancel whatever timer might be running now. */
  src_conn->pending_tmr_id = -1;
  (void)tmr_cancel_sync(src_conn->tmr);

  /* Flag connection as closing. */
  err = lbmct_src_conn_delete(src_conn);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

  return LBM_OK;
}  /* lbmct_src_handle_handshake_d_ok */


/******************************************************************************
 * Here are the functions that implement the ct controller commands.
 ******************************************************************************/


#define LBMCT_SRC_CREATE_CLEANUP_E_RTN(_m) do {\
  if (um_src != NULL) lbm_src_delete(um_src);\
  if (ct_src != NULL) {\
    PRT_VOL32(ct_src->sig) = LBMCT_SIG_DEAD;\
    free(ct_src);\
  }\
  E_RTN(_m, -1);\
} while (0)

int lbmct_ctrlr_cmd_ct_src_create(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_ct_src_create_t *ct_src_create = cmd->cmd_data;
  int err, is_ok;
  lbmct_src_t *ct_src = NULL;
  lbm_topic_t *lbm_topic = NULL;
  lbm_src_t *um_src = NULL;

  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  /* Reallocate topic string so we can keep it. */
  if (ct_src_create->topic_str == NULL) E_RTN("NULL topic_str", -1);

  PRT_MALLOC_SET_N(ct_src, lbmct_src_t, 0x5a, 1);
  ct_src->sig = LBMCT_SIG_CT_SRC;

  ct_src->ct = ct;
  ct_src->ctx = ct->ctx;
  strncpy(ct_src->topic_str, ct_src_create->topic_str,
    sizeof(ct_src->topic_str));
  ct_src->app_src_cb = ct_src_create->src_cb;
  ct_src->app_src_conn_create_cb = ct_src_create->src_conn_create_cb;
  ct_src->app_src_conn_delete_cb = ct_src_create->src_conn_delete_cb;
  ct_src->app_src_clientd = ct_src_create->src_clientd;
  ct_src->conn_list_head = NULL;
  ct_src->src_list_next = NULL;
  ct_src->src_list_prev = NULL;
  ct_src->exiting = 0;

  LBMCT_LIST_ADD(ct->src_list_head, ct_src, src_list);

  /* Create the receiver object. */
  err = lbm_src_topic_alloc(&lbm_topic, ct->ctx, ct_src->topic_str,
    ct_src_create->src_attr);
  if (err != LBM_OK) LBMCT_SRC_CREATE_CLEANUP_E_RTN(lbm_errmsg());

  err = lbm_src_create(&um_src, ct->ctx, lbm_topic, ct_src->app_src_cb,
    ct_src->app_src_clientd, NULL);
  if (err != LBM_OK) LBMCT_SRC_CREATE_CLEANUP_E_RTN(lbm_errmsg());
  ct_src->um_src = um_src;

  /* Insert this ct src into ct_src_asl, keyed by topic string. */
  is_ok = mul_asl_insert_and_retrieve_node(ct->ct_src_asl, ct_src,
    &ct_src->ct_src_asl_node);
  if (is_ok != 1) LBMCT_SRC_CREATE_CLEANUP_E_RTN("ct_src_asl insert failed");

  /* Return the ct source object to app. */
  ct_src_create->ct_src = ct_src;

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_ct_src_create */


#define E_DEL_MSG_RTN(_m) do {\
  (void)lbm_msg_delete(src_handshake->msg);\
  E_RTN(_m, -1);\
} while (0)

/* Function for source-side to handle handshake messages from receiver. */
int lbmct_ctrlr_cmd_src_handshake(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_src_handshake_t *src_handshake = cmd->cmd_data;
  const char *msg_data = NULL;
  size_t msg_len;
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_DEL_MSG_RTN(E_BAD_SIG(ct));

  msg_data = src_handshake->msg->data;
  msg_len = src_handshake->msg->len;
  if (msg_len == 0) E_DEL_MSG_RTN("Empty message");
  if (mul_strnlen(msg_data, msg_len) == msg_len) E_DEL_MSG_RTN("Missing null");

  if (strncmp(LBMCT_CREQ_MSG_PREFIX, msg_data, LBMCT_PREFIX_SZ) == 0) {
    /* Record receiving CREQ in event buffer. */
    ct->recent_events[(ct->num_recent_events ++) % LBMCT_MAX_RECENT_EVENTS] =
      0x01000000 | 0x00000001;
    err = lbmct_src_handle_handshake_creq(ct, src_handshake);
    if (err != LBM_OK) E_DEL_MSG_RTN(lbm_errmsg());
  }
  else if (strncmp(LBMCT_C_OK_MSG_PREFIX, msg_data, LBMCT_PREFIX_SZ) == 0) {
    /* Record receiving C_OK in event buffer. */
    ct->recent_events[(ct->num_recent_events ++) % LBMCT_MAX_RECENT_EVENTS] =
      0x01000000 | 0x00000003;
    err = lbmct_src_handle_handshake_c_ok(ct, src_handshake);
    if (err != LBM_OK) E_DEL_MSG_RTN(lbm_errmsg());
  }
  else if (strncmp(LBMCT_DREQ_MSG_PREFIX, msg_data, LBMCT_PREFIX_SZ) == 0) {
    /* Record receiving DREQ in event buffer. */
    ct->recent_events[(ct->num_recent_events ++) % LBMCT_MAX_RECENT_EVENTS] =
      0x01000000 | 0x00000004;
    err = lbmct_src_handle_handshake_dreq(ct, src_handshake);
    if (err != LBM_OK) E_DEL_MSG_RTN(lbm_errmsg());
  }
  else if (strncmp(LBMCT_D_OK_MSG_PREFIX, msg_data, LBMCT_PREFIX_SZ) == 0) {
    /* Record receiving D_OK in event buffer. */
    ct->recent_events[(ct->num_recent_events ++) % LBMCT_MAX_RECENT_EVENTS] =
      0x01000000 | 0x00000006;
    err = lbmct_src_handle_handshake_d_ok(ct, src_handshake);
    if (err != LBM_OK) E_DEL_MSG_RTN(lbm_errmsg());
  }
  else {
    E_DEL_MSG_RTN("Unrecognized handshake");
  }

  err = lbm_msg_delete(src_handshake->msg);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_src_handshake */


int lbmct_ctrlr_cmd_src_conn_tick(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_src_conn_tick_t *src_conn_tick = cmd->cmd_data;
  lbmct_src_conn_t *src_conn = src_conn_tick->src_conn;
  int err;

  if (src_conn->sig != LBMCT_SIG_SRC_CONN) E_RTN(E_BAD_SIG(src_conn), -1);
  ct = src_conn->ct;
  if (ct == NULL) E_RTN("ct_src is corrupted", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  /* This tick command may have spent some time in the work queue.  If it
   * isn't the tick we are expecting, it is old and should be ignored.
   */
  if (src_conn_tick->tmr_id != src_conn->pending_tmr_id) {
    return LBM_OK;
  }
  /* This is our tick; consume it. */
  src_conn->pending_tmr_id = -1;

  if (src_conn->state == LBMCT_CONN_STATE_STARTING) {
    /* Timed out waiting for C_OK, retry? */
    if (src_conn->try_cnt < ct->active_config.max_tries) {
      /* Retry the CRSP. */
      src_conn->try_cnt++;
      err = lbmct_handshake_send_crsp(src_conn);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

      err = tmr_schedule(&src_conn->pending_tmr_id, src_conn->tmr,
        lbmct_src_timer_cb, src_conn, ct->active_config.retry_ivl);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
    }
    else {
      /* Too many retries, force-delete the connection. */
      lbm_logf(LBM_LOG_WARNING, "Warning at %s:%d, giving up accepting connection from receiver '%s' for topic '%s'\n", BASENAME(__FILE__), __LINE__, src_conn->rcv_conn_id_str, src_conn->ct_src->topic_str);
      src_conn->peer_info.status = LBMCT_CONN_STATUS_BAD_CLOSE;

      err = lbmct_src_conn_delete(src_conn);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
    }
  }

  else if (src_conn->state == LBMCT_CONN_STATE_ENDING) {
    /* Timed out waiting for D_OK, retry? */
    if (src_conn->try_cnt < ct->active_config.max_tries) {
      /* Retry DRSP. */
      src_conn->try_cnt++;
      err = lbmct_handshake_send_drsp(src_conn);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

      err = tmr_schedule(&src_conn->pending_tmr_id, src_conn->tmr,
        lbmct_src_timer_cb, src_conn, ct->active_config.retry_ivl);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
    }
    else {
      /* Too many retries, force-delete the connection. */
      lbm_logf(LBM_LOG_WARNING, "Warning at %s:%d, giving up closing connection from receiver '%s' for topic '%s'\n", BASENAME(__FILE__), __LINE__, src_conn->rcv_conn_id_str, src_conn->ct_src->topic_str);
      src_conn->peer_info.status = LBMCT_CONN_STATUS_BAD_CLOSE;

      err = lbmct_src_conn_delete(src_conn);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
    }
  }

  else {
    lbm_logf(LBM_LOG_ERR, "Warning at %s:%d, received timeout when src_conn in state %d\n",
      BASENAME(__FILE__), __LINE__, (int)src_conn->state);
  }

  return LBM_OK;
}  /* lbmct_src_conn_timeout */


int lbmct_ctrlr_cmd_ct_src_delete(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_ct_src_delete_t *src_delete = cmd->cmd_data;
  lbmct_src_t *ct_src = src_delete->ct_src;
  lbmct_src_conn_t *src_conn = NULL;
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  if (ct_src->sig != LBMCT_SIG_CT_SRC) E_RTN(E_BAD_SIG(ct_src), -1);
  ct_src->exiting = 1;

  /* For each active connection, start it deleting. */
  src_conn = ct_src->conn_list_head;
  while (src_conn != NULL) {
    if (src_conn->state != LBMCT_CONN_STATE_ENDING) {
      /* Flag connection as closing. */
      src_conn->state = LBMCT_CONN_STATE_ENDING;

      /* Cancel whatever timer might be running. */
      src_conn->pending_tmr_id = -1;  /* Not expecting a tick. */
      (void)tmr_cancel_sync(src_conn->tmr);

      /* Send the DRSP handshake. */
      err = lbmct_handshake_send_drsp(src_conn);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

      /* Time out waiting for D_OK. */
      err = tmr_schedule(&src_conn->pending_tmr_id, src_conn->tmr,
        lbmct_src_timer_cb, src_conn, ct->active_config.retry_ivl);
    }

    src_conn = src_conn->conn_list_next;
  }  /* while src_conn */

  /* Remove from source asl.  This prevents any queued commands from accessing
   * the source.  (For example, if some remote receiver has just sent a
   * CREQ handshake.)
   */
  mul_asl_remove_node(ct->ct_src_asl, ct_src->ct_src_asl_node);
  ct_src->ct_src_asl_node = NULL;

  /* If there are no child connections, finish deleting the ct_src.  (Otherwise
   * wait till those child connections are done being deleted.)
   */
  if (ct_src->conn_list_head == NULL) {
    /* Remove from ct. */
    LBMCT_LIST_DEL(ct->src_list_head, ct_src, src_list);

    /* Delete the underlying UM source. */
    lbm_src_delete(ct_src->um_src);
    ct_src->um_src = NULL;

    PRT_VOL32(ct_src->sig) = LBMCT_SIG_DEAD;
    free(ct_src);
  }

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_ct_src_delete */
