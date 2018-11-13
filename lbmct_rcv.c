/* lbmct_rcv.c - Connected Topics code for receiver-side.
 *
 * See http://ultramessaging.github.io/UMExamples/lbmct/c/index.html
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
#include <libgen.h>
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


/* On the receive side, we use the source notification feature
 * receive source_notification_function to trigger creation of a
 * connection to a remote source.  (The create callback happens when
 * the UM receiver creates a delivery controller for the remote source.)
 */
void *lbmct_src_notif_create_cb(const char *source_name, void *clientd)
{
  lbmct_rcv_t *ct_rcv = (lbmct_rcv_t *)clientd;
  lbmct_rcv_conn_t *rcv_conn = NULL;
  lbmct_t *ct = NULL;
  lbmct_ctrlr_cmd_rcv_conn_create_t *rcv_conn_create = NULL;
  size_t source_name_len;
  int err;

  /* Sanity checks. */
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) EL_RTN(E_BAD_SIG(ct_rcv), NULL);
  ct = ct_rcv->ct;
  if (ct == NULL) EL_RTN("ct_rcv is corrupted", NULL);
  if (ct->sig != LBMCT_SIG_CT) EL_RTN(E_BAD_SIG(ct), NULL);
  source_name_len = mul_strnlen(source_name,
    sizeof(rcv_conn_create->source_name));
  if (source_name_len >= sizeof(rcv_conn_create->source_name)) {
    lbm_logf(LBM_LOG_ERR,
      "Error at %s:%d, source_name too long (%s)\n",
      basename(__FILE__), __LINE__, source_name);
    return NULL;
  }

  /* Create the segment for the connection, but give it to the ctrlr thread
   * to initialize.  It is allocated here so that its pointer can be returned
   * as the per-source clientd, and it is initialized in the ctrlr thread so
   * that connection operations are serialized. */
  PRT_MALLOC_SET_N(rcv_conn, lbmct_rcv_conn_t, 0x5a, 1);
  rcv_conn->sig = LBMCT_SIG_RCV_CONN;
  rcv_conn->state = LBMCT_CONN_STATE_PRE_CREATED;

  /* Send command to controller thread.  Do not wait for it to complete. */
  PRT_MALLOC_SET_N(rcv_conn_create, lbmct_ctrlr_cmd_rcv_conn_create_t, 0x5a, 1);
  memcpy(rcv_conn_create->source_name, source_name, source_name_len + 1);
  rcv_conn_create->ct_rcv = ct_rcv;
  rcv_conn_create->rcv_conn = rcv_conn;
  PRT_MUTEX_INIT(rcv_conn->conn_lock);

  err = lbmct_ctrlr_cmd_submit_nowait(ct,
    LBMCT_CTRLR_CMD_TYPE_RCV_CONN_CREATE, rcv_conn_create,
    LBMCT_CTRLR_CMD_NEEDS_FREE);
  if (err != LBM_OK) EL_RTN("cmd submit failed", NULL);

  /* Make our conn the actual src_clientd. */
  return rcv_conn;
}  /* lbmct_src_notif_create_cb */


int lbmct_src_notif_delete_cb(const char *source_name, void *clientd,
  void *src_clientd)
{
  lbmct_rcv_t *ct_rcv = (lbmct_rcv_t *)clientd;
  lbmct_rcv_conn_t *rcv_conn = (lbmct_rcv_conn_t *)src_clientd;
  lbmct_t *ct = NULL;
  lbmct_ctrlr_cmd_rcv_conn_delete_t *rcv_conn_delete = NULL;
  int err;

  /* Sanity checks. */
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) EL_RTN(E_BAD_SIG(ct_rcv), LBM_OK);
  if (rcv_conn->sig != LBMCT_SIG_RCV_CONN) EL_RTN(E_BAD_SIG(rcv_conn), LBM_OK);
  ct = ct_rcv->ct;
  if (ct == NULL) EL_RTN("ct_rcv is corrupted", LBM_OK);
  if (ct->sig != LBMCT_SIG_CT) EL_RTN(E_BAD_SIG(ct), LBM_OK);

  PRT_MALLOC_SET_N(rcv_conn_delete, lbmct_ctrlr_cmd_rcv_conn_delete_t, 0x5a, 1);
  rcv_conn_delete->rcv_conn = rcv_conn;

  /* The connection is about to be deleted, no more timer ticks. */
  rcv_conn->pending_tmr_id = -1;  /* Not expecting a tick. */
  (void)tmr_cancel_ctx_thread(rcv_conn->tmr);

  err = lbmct_ctrlr_cmd_submit_nowait(ct,
    LBMCT_CTRLR_CMD_TYPE_RCV_CONN_DELETE, rcv_conn_delete,
    LBMCT_CTRLR_CMD_NEEDS_FREE);
  if (err != LBM_OK) EL_RTN("cmd submit failed", LBM_OK);

  return LBM_OK;
}  /* lbmct_src_notif_delete_cb */


/* Executed from context thread callback. */
int lbmct_rcv_handle_handshake_crsp(lbmct_rcv_conn_t *rcv_conn, lbm_msg_t *msg)
{
  const char *msg_data = msg->data;
  size_t msg_len = msg->len;
  lbmct_t *ct = NULL;
  lbmct_rcv_t *ct_rcv = NULL;
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
  int metadata_len;
  int metadata_ofs = 0;

  /* Sanity checks. */
  if (rcv_conn->sig != LBMCT_SIG_RCV_CONN) E_RTN(E_BAD_SIG(rcv_conn), -1);
  ct = rcv_conn->ct;
  if (ct == NULL) E_RTN("rcv_conn is corrupted", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  ct_rcv = rcv_conn->ct_rcv;
  if (ct_rcv == NULL) E_RTN("rcv_conn is corrupted", -1);
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);

  sscanf(msg_data,
    "%" STRDEF(LBMCT_PREFIX_SZ) "[a-zA-Z0-9_],"  /* cmd */
    "%u,"  /* field_cnt */
    "%u,"  /* rcv_ct_id */
    "%" STRDEF(LBMCT_UIM_ADDR_STR_SZ) "[A-Z0-9:.],"  /* rcv_uim_addr */
    "%u,"  /* rcv_conn_id */
    "%u,"  /* src_ct_id */
    "%" STRDEF(LBMCT_UIM_ADDR_STR_SZ) "[A-Z0-9:.],"  /* src_uim_addr */
    "%u,"  /* src_conn_id */
    "%u"  /* metadata_len */
    "%n",  /* offset to null */
    cmd, &field_cnt,
    &rcv_ct_id, rcv_uim_addr, &rcv_conn_id,
    &src_ct_id, src_uim_addr, &src_conn_id,
    &metadata_len, &metadata_ofs);

  /* sscanf will only set the final %n offset if everything before is OK. */
  if (metadata_ofs == 0) E_RTN("Conn rsp msg: parse error", -1);
  if (msg_data[metadata_ofs] != '\0') E_RTN("Conn rsp msg: parse error", -1);
  metadata_ofs++;  /* Step past the nul. */
  if (metadata_ofs + metadata_len != msg_len) E_RTN("meta len mismatch", -1);

  /* This message came across the connection associated with this per-source
   * clientd.  However, there can be multiple receivers with connections to
   * this transport session, so only pay attention to this handshake if it
   * is for *this* connection.
   */
  if (rcv_ct_id == ct->ct_id && rcv_conn_id == rcv_conn->rcv_conn_id &&
    strcmp(rcv_uim_addr, rcv_conn->rcv_uim_addr) == 0)
  {
    PRT_MUTEX_LOCK(rcv_conn->conn_lock);

    /* Handshake message is for this connection. */
    if (rcv_conn->state == LBMCT_CONN_STATE_STARTING) {
      /* Don't need to time out the handshake any more. */
      rcv_conn->pending_tmr_id = -1;  /* Not expecting a tick. */

      /* Save connection info from source. */
      rcv_conn->src_ct_id = src_ct_id;
      memcpy(rcv_conn->src_uim_addr, src_uim_addr,
        sizeof(rcv_conn->src_uim_addr));
      rcv_conn->src_conn_id = src_conn_id;

      if (metadata_len > 0) {
        PRT_MALLOC_N(rcv_conn->peer_info.src_metadata, char, metadata_len);
        memcpy(rcv_conn->peer_info.src_metadata, &msg_data[metadata_ofs],
          metadata_len);
      } else {
        rcv_conn->peer_info.src_metadata = NULL;
      }
      rcv_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_SRC_METADATA;
      rcv_conn->peer_info.src_metadata_len = metadata_len;
      rcv_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_SRC_METADATA_LEN;
      rcv_conn->peer_info.rcv_start_seq_num = msg->sequence_number;
      rcv_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_RCV_START_SEQ_NUM;

      rcv_conn->state = LBMCT_CONN_STATE_RUNNING;

      /* Call the application's per-connection clientd create function. */
      rcv_conn->app_conn_clientd =
        (*ct_rcv->app_rcv_conn_create_cb)(rcv_conn, &rcv_conn->peer_info,
        ct_rcv->app_rcv_clientd);
      rcv_conn->app_conn_create_called = 1;
    }  /* if state = starting */

    {
      lbmct_ctrlr_cmd_rcv_send_c_ok_t *rcv_send_c_ok = NULL;

      /* State starting or running, send the connection OK message back. */
      PRT_MALLOC_SET_N(rcv_send_c_ok, lbmct_ctrlr_cmd_rcv_send_c_ok_t, 0x5a, 1);
      rcv_send_c_ok->rcv_conn = rcv_conn;
      err = lbmct_ctrlr_cmd_submit_nowait(ct,
        LBMCT_CTRLR_CMD_TYPE_RCV_SEND_C_OK, rcv_send_c_ok,
        LBMCT_CTRLR_CMD_NEEDS_FREE);
      if (err != LBM_OK) {
        lbm_logf(LBM_LOG_ERR,
          "Error at %s:%d, could not submit send c_ok\n",
          basename(__FILE__), __LINE__);
      }
    }
    rcv_conn->pending_tmr_id = -1;  /* Not expecting a tick. */

    PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);

    (void)tmr_cancel_ctx_thread(rcv_conn->tmr);
  }  /* if handshake for this connection */

  return LBM_OK;
}  /* lbmct_rcv_handle_handshake_crsp */


/* Executed from context thread callback. */
int lbmct_rcv_handle_handshake_drsp(lbmct_rcv_conn_t *rcv_conn, lbm_msg_t *msg)
{
  const char *msg_data = msg->data;
  size_t msg_len = msg->len;
  lbmct_t *ct = NULL;
  lbmct_rcv_t *ct_rcv = NULL;
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

  /* Sanity checks. */
  if (rcv_conn->sig != LBMCT_SIG_RCV_CONN) E_RTN(E_BAD_SIG(rcv_conn), -1);
  ct = rcv_conn->ct;
  if (ct == NULL) E_RTN("rcv_conn is corrupted", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  if (rcv_conn->ct != ct) E_RTN("Internal error, ct mismatch", -1);
  ct_rcv = rcv_conn->ct_rcv;
  if (ct_rcv == NULL) E_RTN("rcv_conn is corrupted", -1);
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);

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
  if (null_ofs == 0) E_RTN("Disc rsp msg: parse error", -1);
  if (msg_data[null_ofs] != '\0') E_RTN("Disc rsp msg: parse error", -1);
  if (null_ofs + 1 != msg_len) E_RTN("handshake len mismatch", -1);

  /* This message came across the connection associated with this per-source
   * clientd.  However, there can be multiple receivers with connections to
   * this transport session, so only pay attention to this handshake if it
   * is for *this* connection.
   */
  if (rcv_ct_id == ct->ct_id && rcv_conn_id == rcv_conn->rcv_conn_id &&
    strcmp(rcv_uim_addr, rcv_conn->rcv_uim_addr) == 0)
  {
    PRT_MUTEX_LOCK(rcv_conn->conn_lock);

    /* Handshake message is for this connection. */
    if (rcv_conn->state == LBMCT_CONN_STATE_STARTING ||
        rcv_conn->state == LBMCT_CONN_STATE_RUNNING ||
        rcv_conn->state == LBMCT_CONN_STATE_ENDING ||
        rcv_conn->state == LBMCT_CONN_STATE_TIME_WAIT) {

      rcv_conn->pending_tmr_id = -1;  /* Not expecting a tick. */

      /* This starts the closing operation. */
      if (rcv_conn->state == LBMCT_CONN_STATE_STARTING ||
        rcv_conn->state == LBMCT_CONN_STATE_RUNNING ||
        rcv_conn->state == LBMCT_CONN_STATE_ENDING)
      {
        if (! (rcv_conn->peer_info.flags &
          LBMCT_PEER_INFO_FLAGS_RCV_END_SEQ_NUM))
        {
          /* First time in here. */
          rcv_conn->peer_info.rcv_end_seq_num = msg->sequence_number;
          rcv_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_RCV_END_SEQ_NUM;
        }

        /* In time wait state, we are waiting for the receiver delivery ctrlr
         * to be deleted, resulting in a call to the per-source clientd delete.
         * This cleans up the connection. */
        rcv_conn->state = LBMCT_CONN_STATE_TIME_WAIT;

        /* Only call the conn_delete callback if the conn_create was called. */
        if (rcv_conn->app_conn_create_called) {
          if (! rcv_conn->app_conn_delete_called) {
            (*ct_rcv->app_rcv_conn_delete_cb)(rcv_conn,
              &rcv_conn->peer_info,
              ct_rcv->app_rcv_clientd,
              rcv_conn->app_conn_clientd);
            rcv_conn->app_conn_delete_called = 1;
          }
        }
      }  /* if state = starting, running */

      {
        lbmct_ctrlr_cmd_rcv_send_d_ok_t *rcv_send_d_ok = NULL;

        /* State starting or running, send the connection OK message back. */
        PRT_MALLOC_SET_N(rcv_send_d_ok, lbmct_ctrlr_cmd_rcv_send_d_ok_t, 0x5a, 1);
        rcv_send_d_ok->rcv_conn = rcv_conn;
        err = lbmct_ctrlr_cmd_submit_nowait(ct,
          LBMCT_CTRLR_CMD_TYPE_RCV_SEND_D_OK, rcv_send_d_ok,
          LBMCT_CTRLR_CMD_NEEDS_FREE);
        if (err != LBM_OK) {
          lbm_logf(LBM_LOG_ERR,
            "Error at %s:%d, could not submit send c_ok\n",
            basename(__FILE__), __LINE__);
        }
      }
    }  /* if state = starting, running, time_wait */
    else {
      lbm_logf(LBM_LOG_ERR, "Warning at %s:%d, received DRSP handshake when rcv_conn in state %d\n",
        basename(__FILE__), __LINE__, (int)rcv_conn->state);
    }
    rcv_conn->pending_tmr_id = -1;  /* Not expecting a tick. */

    PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);

    (void)tmr_cancel_ctx_thread(rcv_conn->tmr);
  }  /* if handshake for this connection */

  return LBM_OK;
}  /* lbmct_rcv_handle_handshake_drsp */


int lbmct_rcv_side_msg_rcv_cb(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
  lbmct_rcv_t *ct_rcv = (lbmct_rcv_t *)clientd;
  lbmct_rcv_conn_t *rcv_conn = (lbmct_rcv_conn_t *)msg->source_clientd;
  lbmct_t *ct = NULL;
  const char *msg_data = msg->data;
  size_t msg_len = msg->len;
  int starting_state;
  int err;

  /* Sanity checks. */
  if (ct_rcv == NULL) EL_RTN("NULL clientd for received msg", LBM_OK);
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) EL_RTN(E_BAD_SIG(ct_rcv), LBM_OK);
  if (rcv_conn == NULL) EL_RTN("NULL source_clientd for received msg", LBM_OK);
  if (rcv_conn->sig != LBMCT_SIG_RCV_CONN) EL_RTN(E_BAD_SIG(rcv_conn), LBM_OK);
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);
  ct = ct_rcv->ct;
  if (ct == NULL) E_RTN("corrupted ct_rcv", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  starting_state = rcv_conn->state;
  if (starting_state == LBMCT_CONN_STATE_PRE_CREATED) {
    lbm_logf(LBM_LOG_INFO,
      "%s:%d, Received UM event type %d on pre-created connection; ignoring\n",
      basename(__FILE__), __LINE__, msg->type);
    return LBM_OK;
  }

  /* Check to see if it is a CT handshake message. */
  if (msg->type == LBM_MSG_DATA && msg->properties != NULL) {
    lbm_uint32_t int_prop_val;
    size_t prop_size = sizeof(int_prop_val);
    int prop_type = LBM_MSG_PROPERTY_INT;

    err = lbm_msg_properties_get(msg->properties, LBMCT_HANDSHAKE_TOPIC_STR,
      &int_prop_val, &prop_type, &prop_size);

    /* If ct message property exists, it's a handshake.
     */
    if (err == LBM_OK) {
      if (msg_len == 0) EL_RTN("Empty handshake", LBM_OK);
      if (mul_strnlen(msg_data, msg_len) == msg_len) EL_RTN("Missing null", LBM_OK);

      if (strncmp(LBMCT_CRSP_MSG_PREFIX, msg_data, LBMCT_PREFIX_SZ) == 0) {
        /* Record receiving CRSP in event buffer. */
        ct->recent_events[(ct->num_recent_events++) % LBMCT_MAX_RECENT_EVENTS] =
          0x02000000 | 0x00000002;
        err = lbmct_rcv_handle_handshake_crsp(rcv_conn, msg);
        if (err != LBM_OK) EL_RTN(lbm_errmsg(), LBM_OK);
      }
      else if (strncmp(LBMCT_DRSP_MSG_PREFIX, msg_data, LBMCT_PREFIX_SZ) == 0) {
        /* Record receiving DRSP in event buffer. */
        ct->recent_events[(ct->num_recent_events++) % LBMCT_MAX_RECENT_EVENTS] =
          0x02000000 | 0x00000005;
        err = lbmct_rcv_handle_handshake_drsp(rcv_conn, msg);
        if (err != LBM_OK) EL_RTN(lbm_errmsg(), LBM_OK);
      }
      else {
        EL_RTN("Unrecognized handshake", LBM_OK);
      }
    }  /* if ct message prop exists */
  }

  /* Want to deliver messages up to and including DRSP, which sets state
   * to TIME_WAIT. */
  if (starting_state == LBMCT_CONN_STATE_RUNNING ||
    starting_state == LBMCT_CONN_STATE_ENDING ||
    rcv_conn->state == LBMCT_CONN_STATE_RUNNING ||
    rcv_conn->state == LBMCT_CONN_STATE_ENDING)
  {
    /* BOS and EOS are no longer reliable; don't deliver. */
    if (msg->type != LBM_MSG_BOS && msg->type != LBM_MSG_EOS) {
      /* Deliver to application, with its connection clientd. */
      msg->source_clientd = rcv_conn->app_conn_clientd;

      (*ct_rcv->app_rcv_cb)(ct_rcv->um_rcv, msg, ct_rcv->app_rcv_clientd);

      msg->source_clientd = rcv_conn;  /* Restore our source clientd. */
    }
  }

  return LBM_OK;
}  /* lbmct_rcv_side_msg_rcv_cb */


/* Public API to create a receiver object. */
int lbmct_rcv_create(lbmct_rcv_t **ct_rcvp, lbmct_t *ct, const char *topic_str,
  lbm_rcv_topic_attr_t *rcv_attr,
  lbm_rcv_cb_proc rcv_cb,
  lbmct_rcv_conn_create_function_cb rcv_conn_create_cb,
  lbmct_rcv_conn_delete_function_cb rcv_conn_delete_cb,
  void *clientd)
{
  lbmct_ctrlr_cmd_ct_rcv_create_t ct_rcv_create;
  int err;

  /* Send a command to the controller thread to create the ct receiver.
   * Wait for the command to complete.
   */
  ct_rcv_create.ct_rcv = NULL;
  ct_rcv_create.topic_str = topic_str;
  ct_rcv_create.rcv_attr = rcv_attr;
  ct_rcv_create.rcv_cb = rcv_cb;
  ct_rcv_create.rcv_conn_create_cb = rcv_conn_create_cb;
  ct_rcv_create.rcv_conn_delete_cb = rcv_conn_delete_cb;
  ct_rcv_create.rcv_clientd = clientd;

  err = lbmct_ctrlr_cmd_submit_and_wait(ct,
    LBMCT_CTRLR_CMD_TYPE_CT_RCV_CREATE, &ct_rcv_create);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

  /* Receiver object is created.  Return it. */
  *ct_rcvp = ct_rcv_create.ct_rcv;

  return LBM_OK;
}  /* lbmct_rcv_create */


/* Public API to delete a receiver object. */
int lbmct_rcv_delete(lbmct_rcv_t *ct_rcv)
{
  lbmct_t *ct = NULL;
  lbmct_ctrlr_cmd_ct_rcv_delete_t ct_rcv_delete;
  int err;

  /* Sanity checks. */
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);
  ct = ct_rcv->ct;
  if (ct == NULL) E_RTN("corrupted ct_rcv", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  ct_rcv_delete.ct_rcv = ct_rcv;

  err = lbmct_ctrlr_cmd_submit_and_wait(ct,
    LBMCT_CTRLR_CMD_TYPE_CT_RCV_DELETE, &ct_rcv_delete);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

  return LBM_OK;
}  /* lbmct_rcv_delete */


/* Called from lbmct_ctrlr_cmd_rcv_conn_delete(). */
int lbmct_rcv_conn_delete(lbmct_rcv_conn_t *rcv_conn)
{
  lbmct_t *ct = NULL;
  lbmct_rcv_t *ct_rcv = NULL;

  /* Sanity checks. */
  if (rcv_conn->sig != LBMCT_SIG_RCV_CONN) E_RTN(E_BAD_SIG(rcv_conn), -1);
  ct = rcv_conn->ct;
  if (ct == NULL) E_RTN("rcv_conn is corrupted", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  ct_rcv = rcv_conn->ct_rcv;
  if (ct_rcv == NULL) E_RTN("rcv_conn is corrupted", -1);
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);

  /* Shouldn't need this since nothing should be accessing the connection
   * at this late date.  But just in case something else is finishing up,
   * wait till that other thing is done.
   */
  PRT_MUTEX_LOCK(rcv_conn->conn_lock);

  /* Only call the conn_delete callback if the conn_create was called. */
  if (rcv_conn->app_conn_create_called) {
    if (! rcv_conn->app_conn_delete_called) {
      (*ct_rcv->app_rcv_conn_delete_cb)(rcv_conn,
        &rcv_conn->peer_info,
        ct_rcv->app_rcv_clientd,
        rcv_conn->app_conn_clientd);
      rcv_conn->app_conn_delete_called = 1;
    }
  }

  ct_rcv = rcv_conn->ct_rcv;
  if (ct_rcv != NULL) {
    if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);
    rcv_conn->ct_rcv = NULL;
  }

  /* Remove peer's metadata. */
  if (rcv_conn->peer_info.flags & LBMCT_PEER_INFO_FLAGS_SRC_METADATA) {
    if (rcv_conn->peer_info.src_metadata != NULL) {
      free(rcv_conn->peer_info.src_metadata);
    }
    rcv_conn->peer_info.flags &= ~LBMCT_PEER_INFO_FLAGS_SRC_METADATA;
    rcv_conn->peer_info.flags &= ~LBMCT_PEER_INFO_FLAGS_SRC_METADATA_LEN;
  }

  /* Remove from ct rcv. */
  LBMCT_LIST_DEL(ct_rcv->conn_list_head, rcv_conn, conn_list);

  /* Don't want any more timers. */
  rcv_conn->pending_tmr_id = -1;  /* Not expecting a tick. */

  /* The parent ct_rcv might be in the process of being deleted.  If so, and
   * this was the last rcv_conn, finish deleting the ct_rcv.
   */
  if (ct_rcv->exiting && ct_rcv->conn_list_head == NULL) {
    /* Remove from ct. */
    LBMCT_LIST_DEL(ct->rcv_list_head, ct_rcv, rcv_list);

    /* Delete the underlying UM source. */
    lbm_rcv_delete(ct_rcv->um_rcv);
    ct_rcv->um_rcv = NULL;

    PRT_VOL32(ct_rcv->sig) = LBMCT_SIG_DEAD;
    free(ct_rcv);
  }
  rcv_conn->pending_tmr_id = -1;  /* Not expecting a tick. */

  PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);

  (void)tmr_cancel_sync(rcv_conn->tmr);
  (void)tmr_delete_sync(rcv_conn->tmr);
  rcv_conn->tmr = NULL;

  /* There should never be anything waiting to get the lock by now! */
  PRT_MUTEX_DELETE(rcv_conn->conn_lock);

  PRT_VOL32(rcv_conn->sig) = LBMCT_SIG_DEAD;
  free(rcv_conn);

  return LBM_OK;
}  /* lbmct_rcv_conn_delete */


/* Called from ct control thread. */
int lbmct_handshake_send_creq(lbmct_rcv_conn_t *rcv_conn)
{
  char creq_msg[LBMCT_CREQ_MSG_SZ+1];
  char dest_addr[LBM_MSG_MAX_SOURCE_LEN+1];
  lbmct_t *ct = rcv_conn->ct;
  int creq_len;
  int err;

  /*  CREQ,field_count,ct_id,rcv_uim_addr,conn_id,topic_str */
  creq_len = snprintf(creq_msg, sizeof(creq_msg),
    "%s,6,%u,%s,%u,%s", LBMCT_CREQ_MSG_PREFIX,
    ct->ct_id, rcv_conn->rcv_uim_addr, rcv_conn->rcv_conn_id,
    rcv_conn->ct_rcv->topic_str);
  creq_len++;  /* Include final NULL. */

  snprintf(dest_addr, sizeof(dest_addr), "SOURCE:%s",
    rcv_conn->peer_info.rcv_source_name);

  if (! (ct->active_config.test_bits & LBMCT_TEST_BITS_NO_CREQ)) {
    /* Record sending CREQ in event buffer. */
    ct->recent_events[(ct->num_recent_events ++) % LBMCT_MAX_RECENT_EVENTS] =
      0x04000000 | 0x00000001;
    err = lbm_unicast_immediate_message(ct->ctx, dest_addr,
      LBMCT_HANDSHAKE_TOPIC_STR, creq_msg, creq_len, LBM_MSG_FLUSH);
    /* The UIM send can fail due to "CoreApi-9901-02: target SOURCE type:
     * transport not found".  This is a race condition where a delivery
     * controller is created and the per-source clientd create is called,
     * which enqueues a conn create command, but before that command can
     * execute, the transport session dies and the TR cache is cleared,
     * so that when this send creq function is called, that source is no
     * longer in the topic cache, and the send to "SOURCE:..." fails.
     */
    if (err != LBM_OK) {
      lbm_logf(LBM_LOG_NOTICE,
        "%s:%d, error sending CREQ to %s: '%s'\n",
        basename(__FILE__), __LINE__, dest_addr, lbm_errmsg());

      rcv_conn->state = LBMCT_CONN_STATE_TIME_WAIT;

      /* The final rcv_conn deletion will happen when the per-source clientd
       * delete is called.
       */
    }
  }
  else {
    lbm_logf(LBM_LOG_NOTICE,
      "LBMCT_TEST_BITS_NO_CREQ at %s:%d, skipping send of '%s' to %s\n",
      basename(__FILE__), __LINE__, creq_msg, dest_addr);
  }

  return LBM_OK;
}  /* lbmct_handshake_send_creq */


/* Called from ct control thread. */
int lbmct_handshake_send_dreq(lbmct_rcv_conn_t *rcv_conn)
{
  lbmct_rcv_t *ct_rcv = NULL;
  lbmct_t *ct = NULL;
  lbm_rcv_t *um_rcv = NULL;
  char dreq_msg[LBMCT_DREQ_MSG_SZ+1];
  int dreq_len;
  int err;

  /* Sanity checks. */
  if (rcv_conn->sig != LBMCT_SIG_RCV_CONN) E_RTN(E_BAD_SIG(rcv_conn), -1);
  ct_rcv = rcv_conn->ct_rcv;
  if (ct_rcv == NULL) E_RTN("rcv_conn is corrupted", -1);
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);
  ct = rcv_conn->ct;
  if (ct == NULL) E_RTN("ct_rcv is corrupted", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  um_rcv = ct_rcv->um_rcv;

  /* dreq,field_count,rcv_ct_id,rcv_uim_addr,rcv_ct_id,
   *   src_ct_id,src_uim_addr,src_conn_id<nul>
   */
  dreq_len = snprintf(dreq_msg, sizeof(dreq_msg),
    "%s,10,%u,%s,%u,%u,%s,%u", LBMCT_DREQ_MSG_PREFIX,
    ct->ct_id, rcv_conn->rcv_uim_addr, rcv_conn->rcv_conn_id,
    rcv_conn->src_ct_id, rcv_conn->src_uim_addr, rcv_conn->src_conn_id);
  dreq_len++;  /* Include final NULL. */

  if (! (ct->active_config.test_bits & LBMCT_TEST_BITS_NO_DREQ)) {
    /* Record sending DREQ in event buffer. */
    ct->recent_events[(ct->num_recent_events ++) % LBMCT_MAX_RECENT_EVENTS] =
      0x04000000 | 0x00000004;
    err = lbm_unicast_immediate_message(ct->ctx, rcv_conn->src_uim_addr,
      LBMCT_HANDSHAKE_TOPIC_STR, dreq_msg, dreq_len, LBM_MSG_FLUSH);
    if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
  }
  else {
    lbm_logf(LBM_LOG_NOTICE,
      "LBMCT_TEST_BITS_NO_DREQ at %s:%d, skipping send of '%s' to %s\n",
      basename(__FILE__), __LINE__, dreq_msg, rcv_conn->src_uim_addr);
  }

  return LBM_OK;
}  /* lbmct_handshake_send_dreq */


/* UM timer callback for ct receiver.  Submit "tick" command. */
int lbmct_rcv_timer_cb(tmr_t *tmr, lbm_context_t *ctx, const void *clientd)
{
  lbmct_rcv_conn_t *rcv_conn = (lbmct_rcv_conn_t *)clientd;
  lbmct_t *ct = NULL;
  lbmct_ctrlr_cmd_rcv_conn_tick_t *rcv_conn_tick = NULL;
  int err;

  /* Sanity checks. */
  ct = rcv_conn->ct;
  if (ct == NULL) EL_RTN("rcv_conn is corrupted", LBM_OK);
  if (ct->sig != LBMCT_SIG_CT) EL_RTN(E_BAD_SIG(ct), LBM_OK);

  PRT_MUTEX_LOCK(rcv_conn->conn_lock);

  /* Only send tick if we are still expecting this timer instance. */
  if (tmr->id == rcv_conn->pending_tmr_id) {
    /* Send tick command. */
    PRT_MALLOC_SET_N(rcv_conn_tick, lbmct_ctrlr_cmd_rcv_conn_tick_t, 0x5a, 1);
    rcv_conn_tick->tmr_id = tmr->id;
    rcv_conn_tick->rcv_conn = rcv_conn;

    err = lbmct_ctrlr_cmd_submit_nowait(ct,
      LBMCT_CTRLR_CMD_TYPE_RCV_CONN_TICK, rcv_conn_tick,
      LBMCT_CTRLR_CMD_NEEDS_FREE);
    if (err != LBM_OK) EL_RTN(lbm_errmsg(), LBM_OK);
  }

  PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);

  return LBM_OK;
}  /* lbmct_rcv_timer_cb */


/******************************************************************************
 * Here are the functions that implement the ct controller commands.
 ******************************************************************************/


/* Triggered from timer callback. */
int lbmct_ctrlr_cmd_rcv_conn_tick(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_rcv_conn_tick_t *rcv_conn_tick = cmd->cmd_data;
  lbmct_rcv_conn_t *rcv_conn = rcv_conn_tick->rcv_conn;
  lbmct_rcv_t *ct_rcv = NULL;
  int err;

  /* Sanity checks. */
  if (rcv_conn->sig != LBMCT_SIG_RCV_CONN) E_RTN(E_BAD_SIG(rcv_conn), -1);
  ct_rcv = rcv_conn->ct_rcv;
  if (ct_rcv == NULL) E_RTN("corrupted rcv_conn_create cmd", -1);
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);

  /* Take connection lock to protect from UM callbacks that need to access
   * the conn structure.  In the code below, be careful to release the lock
   * before doing anything that might block, or might detect an condition that
   * requires returning and error.
   */
  PRT_MUTEX_LOCK(rcv_conn->conn_lock);

  /* This tick command may have spent some time in the work queue.  If it
   * isn't the tick we are expecting, it is old and should be ignored.
   */
  if (rcv_conn_tick->tmr_id != rcv_conn->pending_tmr_id) {
    PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);
    return LBM_OK;
  }
  /* This is our tick; consume it. */
  rcv_conn->pending_tmr_id = -1;

  if (rcv_conn->state == LBMCT_CONN_STATE_STARTING) {
    /* Back off initial fast initial tries. */
    if (rcv_conn->curr_creq_timeout < ct->active_config.retry_ivl) {
      if (rcv_conn->curr_creq_timeout > 0) {
        rcv_conn->curr_creq_timeout *= 10;  /* Backoff retry time. */
      } else {
        rcv_conn->curr_creq_timeout = ct->active_config.retry_ivl / 10;
      }
      if (rcv_conn->curr_creq_timeout > ct->active_config.retry_ivl) {
        rcv_conn->curr_creq_timeout = ct->active_config.retry_ivl;
      }
    }
lbm_logf(LBM_LOG_NOTICE, "DEBUG at %s:%d, curr_creq_timeout=%d, retry_ivl=%d, try_cnt=%d\n", basename(__FILE__), __LINE__, rcv_conn->curr_creq_timeout, rcv_conn->ct->active_config.retry_ivl, rcv_conn->try_cnt);

    if (rcv_conn->try_cnt < ct->active_config.max_tries) {
      /* (Re-)try the CREQ. */
      rcv_conn->try_cnt++;
      PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);

      err = lbmct_handshake_send_creq(rcv_conn);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

      /* Timeout waiting for CRSP. */
      err = tmr_schedule(&rcv_conn->pending_tmr_id, rcv_conn->tmr,
        lbmct_rcv_timer_cb, rcv_conn, rcv_conn->curr_creq_timeout);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
    }
    else {
      /* Too many retries, force-delete the conn.  No need to call user's
       * conn delete callback because we never got any response and did not
       * call the user's conn create callback. */
      lbm_logf(LBM_LOG_WARNING, "Warning at %s:%d, giving up connecting to source '%s' for topic '%s'\n", basename(__FILE__), __LINE__, rcv_conn->peer_info.rcv_source_name, ct_rcv->topic_str);

      /* Postpone final deletion until per-source clientd deletion.
       * But do tell the user the connection is down.
       */
      rcv_conn->state = LBMCT_CONN_STATE_TIME_WAIT;

      PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);
    }  /* if retry max exceeded */
  }  /* if state = starting */

  else if (rcv_conn->state == LBMCT_CONN_STATE_ENDING) {
    /* Timed out waiting for DRSP, retry? (conn_lock is held) */
    if (rcv_conn->try_cnt < ct->active_config.max_tries) {
      /* Retry the DREQ. */
      rcv_conn->try_cnt++;
      PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);

      err = lbmct_handshake_send_dreq(rcv_conn);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

      /* Time out waiting for DRSP. */
      err = tmr_schedule(&rcv_conn->pending_tmr_id, rcv_conn->tmr,
        lbmct_rcv_timer_cb, rcv_conn, ct->active_config.retry_ivl);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
    }
    else {
      /* Too many retries, force-delete the connection. */
      lbm_logf(LBM_LOG_WARNING, "Warning at %s:%d, giving up closing connection to source '%s' for topic '%s'\n", basename(__FILE__), __LINE__, rcv_conn->peer_info.rcv_source_name, ct_rcv->topic_str);
      rcv_conn->peer_info.status = LBMCT_CONN_STATUS_BAD_CLOSE;

      /* Postpone final deletion until per-source clientd deletion.
       * But do tell the user the connection is down.
       */
      rcv_conn->state = LBMCT_CONN_STATE_TIME_WAIT;

      /* Only call the conn_delete callback if the conn_create was called. */
      if (rcv_conn->app_conn_create_called) {
        if (! rcv_conn->app_conn_delete_called) {
          (*ct_rcv->app_rcv_conn_delete_cb)(rcv_conn,
            &rcv_conn->peer_info,
            ct_rcv->app_rcv_clientd,
            rcv_conn->app_conn_clientd);
          rcv_conn->app_conn_delete_called = 1;
        }
      }

      PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);
    }  /* if retry max exceeded */
  }  /* if state = ending */

  else {  /* State not starting or ending. */
    /* (conn_lock is held) */
    PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);

    /* Although rare, it is *possible* for a timeout to be called in a state
     * that doesn't need a timer.  For example, a CRSP might be received and
     * enqueued, but before it is processed, the "starting" timer fires and
     * is enqueued.
     */
    lbm_logf(LBM_LOG_INFO, "Info at %s:%d, received timeout when rcv_conn in state %d\n",
      basename(__FILE__), __LINE__, (int)rcv_conn->state);
  }

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_rcv_conn_tick */


#define LBMCT_RCV_CREATE_CLEANUP_E_RTN(_m) do {\
  if (rcv_attr != NULL) lbm_rcv_topic_attr_delete(rcv_attr);\
  if (um_rcv != NULL) lbm_rcv_delete(um_rcv);\
  if (ct_rcv != NULL) {\
    PRT_VOL32(ct_rcv->sig) = LBMCT_SIG_DEAD;\
    free(ct_rcv);\
  }\
  E_RTN(_m, -1);\
} while (0)

/* Triggered from application call to lbmct_rcv_create(). */
int lbmct_ctrlr_cmd_ct_rcv_create(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_ct_rcv_create_t *ct_rcv_create = cmd->cmd_data;
  lbmct_rcv_t *ct_rcv = NULL;
  lbm_topic_t *lbm_topic = NULL;
  lbm_rcv_t *um_rcv = NULL;
  size_t opt_size;
  lbm_rcv_topic_attr_t *rcv_attr = NULL;
  lbm_rcv_src_notification_func_t src_notif_func;  /* config option */
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  if (ct_rcv_create->topic_str == NULL) E_RTN("NULL topic_str", -1);

  PRT_MALLOC_SET_N(ct_rcv, lbmct_rcv_t, 0x5a, 1);
  ct_rcv->sig = LBMCT_SIG_CT_RCV;

  ct_rcv->ct = ct;
  ct_rcv->ctx = ct->ctx;
  strncpy(ct_rcv->topic_str, ct_rcv_create->topic_str,
    sizeof(ct_rcv->topic_str));
  ct_rcv->app_rcv_cb = ct_rcv_create->rcv_cb;
  ct_rcv->app_rcv_conn_create_cb = ct_rcv_create->rcv_conn_create_cb;
  ct_rcv->app_rcv_conn_delete_cb = ct_rcv_create->rcv_conn_delete_cb;
  ct_rcv->app_rcv_clientd = ct_rcv_create->rcv_clientd;
  ct_rcv->conn_list_head = NULL;
  ct_rcv->rcv_list_next = NULL;
  ct_rcv->rcv_list_prev = NULL;
  ct_rcv->exiting = 0;

  LBMCT_LIST_ADD(ct->rcv_list_head, ct_rcv, rcv_list);

  /* If the user passed in a receiver attribute, we want to add to it. */
  if (ct_rcv_create->rcv_attr != NULL) {
    /* Don't allow caller to use source_notification_function. */
    opt_size= sizeof(src_notif_func);
    err = lbm_rcv_topic_attr_getopt(ct_rcv_create->rcv_attr,
      "source_notification_function", &src_notif_func, &opt_size);
    if (err != LBM_OK) LBMCT_RCV_CREATE_CLEANUP_E_RTN(lbm_errmsg());
    if (src_notif_func.create_func != NULL) LBMCT_RCV_CREATE_CLEANUP_E_RTN("ct_rcv may not use config option source_notification_function");

    /* Make a copy of user's attribute so we can add our src notif funct. */
    err = lbm_rcv_topic_attr_dup(&rcv_attr, ct_rcv_create->rcv_attr);
    if (err != LBM_OK) LBMCT_RCV_CREATE_CLEANUP_E_RTN(lbm_errmsg());

  } else {
    /* User didn't pass in an attribute, create one. */
    err = lbm_rcv_topic_attr_create(&rcv_attr);
    if (err != LBM_OK) LBMCT_RCV_CREATE_CLEANUP_E_RTN(lbm_errmsg());
  }

  /* Set up receiver per-source clientd create/delete callbacks. */
  src_notif_func.create_func = lbmct_src_notif_create_cb;
  src_notif_func.delete_func = lbmct_src_notif_delete_cb;
  src_notif_func.clientd = ct_rcv;
  err = lbm_rcv_topic_attr_setopt(rcv_attr, "source_notification_function",
    &src_notif_func, sizeof(src_notif_func));
  if (err != LBM_OK) LBMCT_RCV_CREATE_CLEANUP_E_RTN(lbm_errmsg());

  /* Create the UM receiver object. */
  err = lbm_rcv_topic_lookup(&lbm_topic, ct->ctx, ct_rcv->topic_str, rcv_attr);
  if (err != LBM_OK) LBMCT_RCV_CREATE_CLEANUP_E_RTN(lbm_errmsg());

  err = lbm_rcv_create(&um_rcv, ct->ctx, lbm_topic,
    lbmct_rcv_side_msg_rcv_cb, ct_rcv, NULL);
  if (err != LBM_OK) LBMCT_RCV_CREATE_CLEANUP_E_RTN(lbm_errmsg());
  ct_rcv->um_rcv = um_rcv;

  lbm_rcv_topic_attr_delete(rcv_attr);

  /* Return the ct receiver object to app. */
  ct_rcv_create->ct_rcv = ct_rcv;

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_ct_rcv_create */


/* Internal function (not public API) to create the rcv_conn object.
 * Note: memory for the rcv connection object was allocated in
 * lbmct_src_notif_create_cb().  This was done so that it could be returned
 * as the per-source clientd.  But this is the function that actually
 * does the work of initializing the connection.
 */
int lbmct_ctrlr_cmd_rcv_conn_create(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_rcv_conn_create_t *rcv_conn_create = cmd->cmd_data;
  lbmct_rcv_conn_t *rcv_conn = NULL;
  lbmct_rcv_t *ct_rcv = NULL;
  struct in_addr ip;
  char ip_str[INET_ADDRSTRLEN];  /* Large enough to hold strerror_r(). */
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  rcv_conn = rcv_conn_create->rcv_conn;
  if (rcv_conn == NULL) E_RTN("corrupted rcv_conn_create cmd", -1);
  if (rcv_conn->sig != LBMCT_SIG_RCV_CONN) E_RTN(E_BAD_SIG(rcv_conn), -1);
  ct_rcv = rcv_conn_create->ct_rcv;
  if (ct_rcv == NULL) E_RTN("corrupted rcv_conn_create cmd", -1);
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);

  err = tmr_create(&rcv_conn->tmr, ct->ctx);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

  PRT_MUTEX_LOCK(rcv_conn->conn_lock);

  rcv_conn->ct = ct;
  rcv_conn->rcv_conn_id = ct->next_conn_id;
  ct->next_conn_id ++;

  rcv_conn->ct_rcv = ct_rcv;
  rcv_conn->state = LBMCT_CONN_STATE_STARTING;
  rcv_conn->src_uim_addr[0] = '\0';
  rcv_conn->conn_list_next = NULL;
  rcv_conn->conn_list_prev = NULL;
  rcv_conn->app_conn_create_called = 0;
  rcv_conn->app_conn_delete_called = 0;
  rcv_conn->try_cnt = 0;

  rcv_conn->peer_info.flags = 0;
  rcv_conn->peer_info.status = LBMCT_CONN_STATUS_OK;
  rcv_conn->peer_info.rcv_metadata = ct->metadata;
  rcv_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_RCV_METADATA;
  rcv_conn->peer_info.rcv_metadata_len = ct->metadata_len;
  rcv_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_RCV_METADATA_LEN;
  memcpy(rcv_conn->peer_info.rcv_source_name, rcv_conn_create->source_name,
    sizeof(rcv_conn->peer_info.rcv_source_name));
  rcv_conn->peer_info.flags |= LBMCT_PEER_INFO_FLAGS_RCV_SOURCE_NAME;

  /* Add this connection to the receiver's list. */
  LBMCT_LIST_ADD(ct_rcv->conn_list_head, rcv_conn, conn_list);

  /* Assemble UIM address for this ct. */
  ip.s_addr = (in_addr_t)ct->local_uim_addr.ip_addr;
  (void)inet_ntop(AF_INET, &ip, ip_str, sizeof(ip_str));
  snprintf(rcv_conn->rcv_uim_addr, sizeof(rcv_conn->rcv_uim_addr),
    "TCP:%u:%s:%u",
    ct->local_uim_addr.domain_id, ip_str, ct->local_uim_addr.port);

  /* Set up for retries.  First try is done in timer tick. */
  rcv_conn->try_cnt = 0;

  PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);
 
  /* Short time delay for first CREQ. */
  rcv_conn->curr_creq_timeout = ct->active_config.delay_creq;
  err = tmr_schedule(&rcv_conn->pending_tmr_id, rcv_conn->tmr,
    lbmct_rcv_timer_cb, rcv_conn, rcv_conn->curr_creq_timeout);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_rcv_conn_create */


int lbmct_ctrlr_cmd_ct_rcv_delete(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_ct_rcv_delete_t *rcv_delete = cmd->cmd_data;
  lbmct_rcv_t *ct_rcv = rcv_delete->ct_rcv;
  lbmct_rcv_conn_t *rcv_conn = NULL;
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);
  ct_rcv->exiting = 1;

  /* Step through each connection associated with this ct receiver and start
   * the delete process.
   */
  rcv_conn = ct_rcv->conn_list_head;
  while (rcv_conn != NULL) {
    PRT_MUTEX_LOCK(rcv_conn->conn_lock);

    if (rcv_conn->state == LBMCT_CONN_STATE_STARTING ||
      rcv_conn->state == LBMCT_CONN_STATE_RUNNING)
    {
      /* Flag connection as closing. */
      rcv_conn->state = LBMCT_CONN_STATE_ENDING;

      /* Set up for retries. */
      rcv_conn->try_cnt = 0;

      rcv_conn->try_cnt++;
      rcv_conn->pending_tmr_id = -1;  /* Not expecting a tick. */

      PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);

      (void)tmr_cancel_sync(rcv_conn->tmr);

      /* Exchange DREQ/DRSP/D_OK handshake. */
      err = lbmct_handshake_send_dreq(rcv_conn);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

      /* Time out waiting for DRSP. */
      err = tmr_schedule(&rcv_conn->pending_tmr_id, rcv_conn->tmr,
        lbmct_rcv_timer_cb, rcv_conn, ct->active_config.retry_ivl);
      if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
    }
    else {
      PRT_MUTEX_UNLOCK(rcv_conn->conn_lock);

      lbm_logf(LBM_LOG_NOTICE,
        "%s:%d, deleting conn in state %d, skipping DREQ\n",
        basename(__FILE__), __LINE__, rcv_conn->state);
    }

    rcv_conn = rcv_conn->conn_list_next;
  }  /* while rcv_conn */

  /* If there are no child connections, finish deleting the ct_rcv.  (Otherwise
   * wait till those child connections are done being deleted.)
   */
  if (ct_rcv->conn_list_head == NULL) {
    /* Finish deleting the ct rcv. */

    /* Remove from ct. */
    LBMCT_LIST_DEL(ct->rcv_list_head, ct_rcv, rcv_list);

    /* Delete the underlying UM receiver. */
    lbm_rcv_delete(ct_rcv->um_rcv);
    ct_rcv->um_rcv = NULL;

    PRT_VOL32(ct_rcv->sig) = LBMCT_SIG_DEAD;
    free(ct_rcv);
  }

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_ct_rcv_delete */


/* This command is sent when the per-source delete is called. */
int lbmct_ctrlr_cmd_rcv_conn_delete(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_rcv_conn_delete_t *rcv_conn_delete = cmd->cmd_data;
  lbmct_rcv_conn_t *rcv_conn = rcv_conn_delete->rcv_conn;
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  if (rcv_conn->sig != LBMCT_SIG_RCV_CONN) E_RTN(E_BAD_SIG(rcv_conn), -1);

  if (rcv_conn->state != LBMCT_CONN_STATE_TIME_WAIT) {
    /* Tell app about premature delete. */
    rcv_conn->peer_info.status = LBMCT_CONN_STATUS_BAD_CLOSE;
  }

  /* Do the work of deleting the connection object. */
  err = lbmct_rcv_conn_delete(rcv_conn);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_rcv_conn_delete */


/* Called from ct control thread. */
int lbmct_ctrlr_cmd_rcv_send_c_ok(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_rcv_send_c_ok_t *rcv_send_c_ok = cmd->cmd_data;
  lbmct_rcv_conn_t *rcv_conn = rcv_send_c_ok->rcv_conn;
  lbmct_rcv_t *ct_rcv = NULL;
  lbm_rcv_t *um_rcv = NULL;
  char *c_ok_msg = NULL;
  size_t c_ok_msg_len = -1;
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  if (rcv_conn->sig != LBMCT_SIG_RCV_CONN) E_RTN(E_BAD_SIG(rcv_conn), -1);
  ct_rcv = rcv_conn->ct_rcv;
  if (ct_rcv == NULL) E_RTN("rcv_conn is corrupted", -1);
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);
  ASSRT(rcv_conn->peer_info.flags & LBMCT_PEER_INFO_FLAGS_RCV_START_SEQ_NUM);
  ASSRT(rcv_conn->peer_info.flags & LBMCT_PEER_INFO_FLAGS_RCV_METADATA);
  ASSRT(rcv_conn->peer_info.flags & LBMCT_PEER_INFO_FLAGS_RCV_METADATA_LEN);

  um_rcv = ct_rcv->um_rcv;

  PRT_MALLOC_N(c_ok_msg, char, LBMCT_C_OK_MSG_BASE_SZ + ct->metadata_len + 1);
  /* c_ok,field_count,rcv_ct_id,rcv_uim_addr,rcv_ct_id,
   *   src_ct_id,src_uim_addr,src_conn_id,start_sqn,
   *   meta_len<nul>metadata
   */
  c_ok_msg_len = snprintf(c_ok_msg, LBMCT_C_OK_MSG_BASE_SZ,
    "%s,10,%u,%s,%u,%u,%s,%u,%u,%lu", LBMCT_C_OK_MSG_PREFIX,
    ct->ct_id, rcv_conn->rcv_uim_addr, rcv_conn->rcv_conn_id,
    rcv_conn->src_ct_id, rcv_conn->src_uim_addr, rcv_conn->src_conn_id,
    rcv_conn->peer_info.rcv_start_seq_num, ct->metadata_len);
  c_ok_msg_len++;  /* Include final NULL. */
  if (ct->metadata != NULL && ct->metadata_len > 0) {
    memcpy(&c_ok_msg[c_ok_msg_len], ct->metadata, ct->metadata_len);
    c_ok_msg_len += ct->metadata_len;
  }

  if (! (ct->active_config.test_bits & LBMCT_TEST_BITS_NO_C_OK)) {
    /* Record sending C_OK in event buffer. */
    ct->recent_events[(ct->num_recent_events ++) % LBMCT_MAX_RECENT_EVENTS] =
      0x04000000 | 0x00000003;
    err = lbm_unicast_immediate_message(ct->ctx, rcv_conn->src_uim_addr,
      LBMCT_HANDSHAKE_TOPIC_STR, c_ok_msg, c_ok_msg_len, LBM_MSG_FLUSH);
    if (err != LBM_OK) { free(c_ok_msg); E_RTN(lbm_errmsg(), -1); }
  }
  else {
    lbm_logf(LBM_LOG_NOTICE,
      "LBMCT_TEST_BITS_NO_C_OK at %s:%d, skipping send of '%s' to %s\n",
      basename(__FILE__), __LINE__, c_ok_msg, rcv_conn->src_uim_addr);
  }
  free(c_ok_msg);

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_rcv_send_c_ok */


/* Called from ct control thread. */
int lbmct_ctrlr_cmd_rcv_send_d_ok(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_rcv_send_d_ok_t *rcv_send_d_ok = cmd->cmd_data;
  lbmct_rcv_conn_t *rcv_conn = rcv_send_d_ok->rcv_conn;
  lbmct_rcv_t *ct_rcv = NULL;
  lbm_rcv_t *um_rcv = NULL;
  char d_ok_msg[LBMCT_D_OK_MSG_SZ];
  int dreq_len;
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  if (rcv_conn->sig != LBMCT_SIG_RCV_CONN) E_RTN(E_BAD_SIG(rcv_conn), -1);
  ct_rcv = rcv_conn->ct_rcv;
  if (ct_rcv == NULL) E_RTN("rcv_conn is corrupted", -1);
  if (ct_rcv->sig != LBMCT_SIG_CT_RCV) E_RTN(E_BAD_SIG(ct_rcv), -1);
  ASSRT(rcv_conn->peer_info.flags & LBMCT_PEER_INFO_FLAGS_RCV_END_SEQ_NUM);

  um_rcv = ct_rcv->um_rcv;

  /* d_ok,field_count,rcv_ct_id,rcv_uim_addr,rcv_ct_id,
   *   src_ct_id,src_uim_addr,src_conn_id,end_sqn<nul>
   */
  dreq_len = snprintf(d_ok_msg, sizeof(d_ok_msg),
    "%s,10,%u,%s,%u,%u,%s,%u,%u", LBMCT_D_OK_MSG_PREFIX,
    ct->ct_id, rcv_conn->rcv_uim_addr, rcv_conn->rcv_conn_id,
    rcv_conn->src_ct_id, rcv_conn->src_uim_addr, rcv_conn->src_conn_id,
    rcv_conn->peer_info.rcv_end_seq_num);
  dreq_len++;  /* Include final NULL. */

  if (! (ct->active_config.test_bits & LBMCT_TEST_BITS_NO_D_OK)) {
    /* Record sending D_OK in event buffer. */
    ct->recent_events[(ct->num_recent_events ++) % LBMCT_MAX_RECENT_EVENTS] =
      0x04000000 | 0x00000006;
    err = lbm_unicast_immediate_message(ct->ctx, rcv_conn->src_uim_addr,
      LBMCT_HANDSHAKE_TOPIC_STR, d_ok_msg, dreq_len, LBM_MSG_FLUSH);
    if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
  }
  else {
    lbm_logf(LBM_LOG_NOTICE,
      "LBMCT_TEST_BITS_NO_D_OK at %s:%d, skipping send of '%s' to %s\n",
      basename(__FILE__), __LINE__, d_ok_msg, rcv_conn->src_uim_addr);
  }

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_rcv_send_d_ok */
