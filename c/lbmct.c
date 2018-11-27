/* lbmct.c - Connected Topics code common to source-side and receiver-side.
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
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif
#include <lbm/lbm.h>
#include "lbmct.h"
#include "lbmct_private.h"
#include "prt.h"


/* Forward references. */
PRT_THREAD_ENTRYPOINT lbmct_ctrlr(void *arg);


/* Utility function to determine the UIM address of a given context. */
int lbmct_ctx_uim_addr(lbm_context_t *ctx, lbmct_ctx_uim_addr_t *uim_addr,
  int domain_id)
{
  size_t optlen;
  int request_tcp_bind_request_port = 0;  /* network order */
  lbm_uint16_t request_port;  /* network order */
  lbm_ipv4_address_mask_t  request_tcp_interface;
  int err;

  /* Make sure user has a request port. */
  optlen = sizeof(request_tcp_bind_request_port);
  err = lbm_context_getopt(ctx, "request_tcp_bind_request_port",
    &request_tcp_bind_request_port, &optlen);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
  if (request_tcp_bind_request_port == 0) E_RTN("context request_tcp_bind_request_port must not be 0", -1);

  /* Get request port number. */
  optlen = sizeof(request_port);
  err = lbm_context_getopt(ctx, "request_tcp_port", &request_port, &optlen);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
  uim_addr->port = ntohs(request_port);

  uim_addr->domain_id = domain_id;

  /* Get the IP address. */
  optlen = sizeof(request_tcp_interface);
  err = lbm_context_getopt(ctx, "request_tcp_interface",
    &request_tcp_interface, &optlen);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

  if (request_tcp_interface.addr == INADDR_ANY) {
    /* No unicast interface specified, so any "should" work.  Use the
       multicast resolver interface, since that defaults to *something*. */
    optlen = sizeof(request_tcp_interface);
    err = lbm_context_getopt(ctx, "resolver_multicast_interface",
      &request_tcp_interface, &optlen);
    if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);
  }
  uim_addr->ip_addr = request_tcp_interface.addr;

  return LBM_OK;
}  /* lbmct_ctx_uim_addr */


/* Verify user's config structure and copy to active. */
int lbmct_process_config(lbmct_t *ct)
{
  lbm_uint32_t test_flag;

  /* Set default values. */
  ct->active_config.test_bits = LBMCT_CT_CONFIG_DEFAULT_TEST_BITS;
  ct->active_config.domain_id = LBMCT_CT_CONFIG_DEFAULT_DOMAIN_ID;
  ct->active_config.delay_creq = LBMCT_CT_CONFIG_DEFAULT_DELAY_CREQ;
  ct->active_config.retry_ivl = LBMCT_CT_CONFIG_DEFAULT_RETRY_IVL;
  ct->active_config.max_tries = LBMCT_CT_CONFIG_DEFAULT_MAX_TRIES;

  /* Extract each supplied option.  */
  test_flag = 0x00000001;
  while (test_flag != 0) {
    /* If caller supplied this flag bit, use the corresponding value. */
    if (ct->user_config.flags & test_flag) {
      switch (test_flag) {
      case LBMCT_CT_CONFIG_FLAGS_TEST_BITS:
        ct->active_config.test_bits = ct->user_config.test_bits;
        break;
      case LBMCT_CT_CONFIG_FLAGS_DOMAIN_ID:
        ct->active_config.domain_id = ct->user_config.domain_id;
        break;
      case LBMCT_CT_CONFIG_FLAGS_DELAY_CREQ:
        ct->active_config.delay_creq = ct->user_config.delay_creq;
        break;
      case LBMCT_CT_CONFIG_FLAGS_RETRY_IVL:
        ct->active_config.retry_ivl = ct->user_config.retry_ivl;
        break;
      case LBMCT_CT_CONFIG_FLAGS_MAX_TRIES:
        ct->active_config.max_tries = ct->user_config.max_tries;
        break;
      default:
        ;  /* Ignore unrecognized bits for backwards compatibility. */
      }  /* switch */
    }

    /* Shifting left to overflow is undefined according to standard. */
    test_flag &= 0x7fffffff;  /* Avoid shift overflow. */
    test_flag <<= 1;  /* Check the next bit. */
  }  /* while */

  return LBM_OK;
}  /* lbmct_process_config */


/* Delete the ct controller queues (work and free). */
int lbmct_cmd_q_delete(lbmct_t *ct)
{
  lbmct_ctrlr_cmd_t *cmd = NULL;
  int cmd_cnt = 0;

  if (ct->ctrlr_cmd_free_tlq != NULL) {
    while (lbm_tl_queue_dequeue(ct->ctrlr_cmd_free_tlq, (void **)&cmd, 0) ==
      LBM_OK && cmd != NULL)
    {
      cmd_cnt ++;
      PRT_SEM_DELETE(cmd->complete_sem);
      PRT_VOL32(ct->sig) = LBMCT_SIG_DEAD;
      free(cmd);
    }
    lbm_tl_queue_delete(ct->ctrlr_cmd_free_tlq);
    ct->ctrlr_cmd_free_tlq = NULL;
  }

  if (ct->ctrlr_cmd_work_tlq != NULL) {
    while (lbm_tl_queue_dequeue(ct->ctrlr_cmd_work_tlq, (void **)&cmd, 0) ==
      LBM_OK && cmd != NULL)
    {
      cmd_cnt ++;
      PRT_SEM_DELETE(cmd->complete_sem);
      PRT_VOL32(ct->sig) = LBMCT_SIG_DEAD;
      free(cmd);
    }
    lbm_tl_queue_delete(ct->ctrlr_cmd_work_tlq);
    ct->ctrlr_cmd_work_tlq = NULL;
  }

  if (cmd_cnt != LBMCT_CTRLR_NUM_CMD_NODES) E_RTN("bad cmd_cnt", -1);

  return LBM_OK;
}  /* lbmct_cmd_q_delete */


/* This enhanced error macro is because various cleanup should be done
 * before returnning.
 */
#define E_CLEAN_CMD_Q_RTN(_m) do {\
  char *_lbm_errmsg = strdup(_m);  ENULL(_lbm_errmsg);\
  lbmct_cmd_q_delete(ct);\
  lbm_seterrf(LBM_EINVAL, "Error at %s:%d, '%s'",\
      BASENAME(__FILE__), __LINE__, _lbm_errmsg);\
  free(_lbm_errmsg);\
  return(-1);\
} while (0)

/* Create the ct controller queues (work and free). */
int lbmct_cmd_q_create(lbmct_t *ct)
{
  lbmct_ctrlr_cmd_t *cmd = NULL;
  int i;
  int err;

  ct->ctrlr_cmd_work_tlq = lbm_tl_queue_create();
  if (ct->ctrlr_cmd_work_tlq == NULL)
    E_CLEAN_CMD_Q_RTN("lbm_tl_queue_create failed");
  ct->ctrlr_cmd_free_tlq = lbm_tl_queue_create();
  if (ct->ctrlr_cmd_free_tlq == NULL)
    E_CLEAN_CMD_Q_RTN("lbm_tl_queue_create failed");

  for (i = 0; i < LBMCT_CTRLR_NUM_CMD_NODES; i++) {
    PRT_MALLOC_SET_N(cmd, lbmct_ctrlr_cmd_t, 0x5a, 1);
    cmd->sig = LBMCT_SIG_CTRLR_CMD;

    cmd->ct = ct;
    PRT_SEM_INIT(cmd->complete_sem, 0);  /* Init count to 0. */

    err = lbm_tl_queue_enqueue(ct->ctrlr_cmd_free_tlq, cmd);
    if (err != LBM_OK) E_CLEAN_CMD_Q_RTN("enqueue failed");
  }  /* for */

  return LBM_OK;
}  /* lbmct_cmd_q_create */


/********************* Compare functions for ASLs. ********************/

/* For ct->ct_src_asl -- key off topic string. */
int lbmct_ct_src_compare(void *lhs, void *rhs)
{
  lbmct_src_t *lhs_ct_src = (lbmct_src_t *)lhs;
  lbmct_src_t *rhs_ct_src = (lbmct_src_t *)rhs;

  return strncmp(lhs_ct_src->topic_str,
    rhs_ct_src->topic_str,
    sizeof(lhs_ct_src->topic_str));
}  /* lbmct_ct_src_compare */


/* for ct->src_conn_asl -- key off conn endpoint ID string. */
int lbmct_src_conn_compare(void *lhs, void *rhs)
{
  lbmct_src_conn_t *lhs_src_conn = (lbmct_src_conn_t *)lhs;
  lbmct_src_conn_t *rhs_src_conn = (lbmct_src_conn_t *)rhs;

  return strncmp(lhs_src_conn->rcv_conn_id_str,
    rhs_src_conn->rcv_conn_id_str,
    sizeof(lhs_src_conn->rcv_conn_id_str));
}  /* lbmct_src_conn_compare */


/* Source-side (UIM) message receive callback. */
int lbmct_src_side_msg_rcv_cb(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
  lbmct_t *ct = (lbmct_t *)clientd;
  lbmct_ctrlr_cmd_src_handshake_t *src_handshake = NULL;
  int err;

  err = lbm_msg_retain(msg);
  if (err != LBM_OK) EL_RTN(lbm_errmsg(), LBM_OK);

  PRT_MALLOC_SET_N(src_handshake, lbmct_ctrlr_cmd_src_handshake_t, 0x5a, 1);
  src_handshake->msg = msg;

  err = lbmct_ctrlr_cmd_submit_nowait(ct,
    LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE, src_handshake,
    LBMCT_CTRLR_CMD_NEEDS_FREE);
  if (err != LBM_OK) EL_RTN("cmd submit failed", LBM_OK);

  return LBM_OK;
}  /* lbmct_src_side_msg_rcv_cb */


/* This enhanced error macro is because once lbmct_src_handshake_rcv_create()
 * creates an attribute, it must be deleted before return.
 */
#define E_DEL_ATTR_RTN(_m) do {\
  char *_lbm_errmsg = strdup(_m);  ENULL(_lbm_errmsg);\
  free(_lbm_errmsg);\
  if (rcv_attr != NULL) lbm_rcv_topic_attr_delete(rcv_attr);\
  lbm_seterrf(LBM_EINVAL, "Error at %s:%d, '%s'",\
      BASENAME(__FILE__), __LINE__, _lbm_errmsg);\
  return(-1);\
} while (0)

/* Create the source-side (UIM) hanshake receiver. */
int lbmct_src_handshake_rcv_create(lbmct_t *ct)
{
  lbm_rcv_topic_attr_t *rcv_attr = NULL;
  lbm_ulong_t opt_val;
  lbm_topic_t *lbm_topic;
  int err;

  /* Disable queries for the LBMCT_HANDSHAKE_TOPIC_STR receiver. */
  err = lbm_rcv_topic_attr_create(&rcv_attr);
  if (err != LBM_OK) E_RTN(lbm_errmsg(), -1);

  opt_val = 0;
  err = lbm_rcv_topic_attr_setopt(rcv_attr,
    "resolver_query_minimum_initial_interval", &opt_val, sizeof(opt_val));
  if (err != LBM_OK) E_DEL_ATTR_RTN(lbm_errmsg());

  opt_val = 0;
  err = lbm_rcv_topic_attr_setopt(rcv_attr,
    "resolver_query_maximum_initial_interval", &opt_val, sizeof(opt_val));
  if (err != LBM_OK) E_DEL_ATTR_RTN(lbm_errmsg());

  opt_val = 0;
  err = lbm_rcv_topic_attr_setopt(rcv_attr,
    "resolver_query_sustain_interval", &opt_val, sizeof(opt_val));
  if (err != LBM_OK) E_DEL_ATTR_RTN(lbm_errmsg());

  opt_val = 0;
  err = lbm_rcv_topic_attr_setopt(rcv_attr,
    "resolution_number_of_sources_query_threshold", &opt_val, sizeof(opt_val));
  if (err != LBM_OK) E_DEL_ATTR_RTN(lbm_errmsg());

  /* Create the LBMCT_HANDSHAKE_TOPIC_STR receiver. */
  err = lbm_rcv_topic_lookup(&lbm_topic, ct->ctx, LBMCT_HANDSHAKE_TOPIC_STR,
    rcv_attr);
  if (err != LBM_OK) E_DEL_ATTR_RTN(lbm_errmsg());

  err = lbm_rcv_create(&ct->um_handshake_rcv, ct->ctx, lbm_topic,
    lbmct_src_side_msg_rcv_cb, ct, NULL);
  if (err != LBM_OK) E_DEL_ATTR_RTN(lbm_errmsg());

  lbm_rcv_topic_attr_delete(rcv_attr);

  return LBM_OK;
}  /* lbmct_src_handshake_rcv_create */


/* This enhanced error macro is because various cleanup should be done
 * before returnning.
 */
#define E_CLEAN_CT_RTN(_m) do {\
  char *_lbm_errmsg = strdup(_m);  ENULL(_lbm_errmsg);\
  if (ct != NULL) {\
    if (ct->metadata != NULL) free(ct->metadata);\
    if (ct->msg_props != NULL) lbm_msg_properties_delete(ct->msg_props);\
    if (ct->um_handshake_rcv != NULL) lbm_rcv_delete(ct->um_handshake_rcv);\
    lbmct_cmd_q_delete(ct);\
    if (ct->src_conn_asl != NULL) mul_asl_delete(ct->src_conn_asl);\
    if (ct->ct_src_asl != NULL) mul_asl_delete(ct->ct_src_asl);\
    PRT_VOL32(ct->sig) = LBMCT_SIG_DEAD;\
    free(ct);\
  }\
  lbm_seterrf(LBM_EINVAL, "Error at %s:%d, '%s'",\
      BASENAME(__FILE__), __LINE__, _lbm_errmsg);\
  free(_lbm_errmsg);\
  return(-1);\
} while (0)

/* Public API to create a Connected Topic controller object. */
int lbmct_create(lbmct_t **ctp, lbm_context_t *ctx, lbmct_config_t *config,
  const char *metadata, size_t metadata_len)
{
  lbmct_t *ct = NULL;
  lbm_uint32_t int_prop_val;
  int err;

  if (metadata_len > 0 && metadata == NULL) E_RTN("Null metadata", -1);

  PRT_MALLOC_SET_N(ct, lbmct_t, 0x5a, 1);
  ct->sig = LBMCT_SIG_CT;

  ct->ct_id = mul_random_range(1,0xFFFFFFFF);  /* Unique across exit/restart. */
  ct->ctx = ctx;
  ct->src_list_head = NULL;
  ct->rcv_list_head = NULL;
  ct->metadata = NULL;
  ct->metadata_len = 0;
  ct->um_handshake_rcv = NULL;
  ct->ctrlr_cmd_work_tlq = NULL;
  ct->ctrlr_cmd_free_tlq = NULL;
  ct->next_conn_id = 0;
  ct->ct_src_asl = NULL;
  ct->src_conn_asl = NULL;
  ct->msg_props = NULL;
  ct->num_recent_events = 0;

  /* Process config items. */
  ct->user_config.flags = 0;  /* By default, no config options. */
  if (config != NULL) {
    ct->user_config = *config;  /* Copy in user's config options. */
  }
  err = lbmct_process_config(ct);  /* set active config. */
  if (err != LBM_OK) E_CLEAN_CT_RTN(lbm_errmsg());

  /* Save the user's metadata. */
  if (metadata != NULL && metadata_len > 0) {
    PRT_MALLOC_N(ct->metadata, char, metadata_len);
    memcpy(ct->metadata, metadata, metadata_len);
    ct->metadata_len = metadata_len;
  }

  err = lbmct_ctx_uim_addr(ct->ctx, &ct->local_uim_addr,
    ct->active_config.domain_id);
  if (err != LBM_OK) E_CLEAN_CT_RTN(lbm_errmsg());

  /* The source-side uses a message prop when sending handshake msgs so
   * that the receiver can differentiate between ct handshake and user msgs.
   */
  err = lbm_msg_properties_create(&ct->msg_props);
  if (err != LBM_OK) E_CLEAN_CT_RTN(lbm_errmsg());
  int_prop_val = 1;
  err = lbm_msg_properties_set(ct->msg_props, LBMCT_HANDSHAKE_TOPIC_STR,
    &int_prop_val, LBM_MSG_PROPERTY_INT, sizeof(int_prop_val));
  if (err != LBM_OK) E_CLEAN_CT_RTN(lbm_errmsg());
  ct->send_ex_info.flags = LBM_SRC_SEND_EX_FLAG_PROPERTIES;
  ct->send_ex_info.properties = ct->msg_props;

  /* Create ASLs to keep track of ct sources and receivers. */
  ct->ct_src_asl = mul_asl_create(lbmct_ct_src_compare, NULL, NULL);
  ENULL(ct->ct_src_asl);
  ct->src_conn_asl = mul_asl_create(lbmct_src_conn_compare, NULL, NULL);
  ENULL(ct->src_conn_asl);

  /* Create work queue for controller thread. */
  err = lbmct_cmd_q_create(ct);
  if (err != LBM_OK) E_CLEAN_CT_RTN(lbm_errmsg());

  ct->ctrlr_state = LBMCT_CTRLR_STATE_STARTING;
  PRT_THREAD_CREATE(err, ct->ctrlr_thread_id, lbmct_ctrlr, ct);
  if (err != LBM_OK) E_CLEAN_CT_RTN("error creating CT thread");

  /* Create UM receiver for UIM handshake messages from ct receivers. */
  err = lbmct_src_handshake_rcv_create(ct);
  if (err != LBM_OK) E_CLEAN_CT_RTN(lbm_errmsg());

  *ctp = ct;
  return LBM_OK;
}  /* lbmct_create */


/* Public API to delete a Connected Topic controller object. */
int lbmct_delete(lbmct_t *ct)
{
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);
  if (ct->src_list_head != NULL) E_RTN("Must delete sources", -1);
  if (ct->rcv_list_head != NULL) E_RTN("Must delete receivers", -1);

  lbm_rcv_delete(ct->um_handshake_rcv);
  ct->um_handshake_rcv = NULL;

  /* Shut down the ctrlr thread. */
  err = lbmct_ctrlr_cmd_submit_nowait(ct,
    LBMCT_CTRLR_CMD_TYPE_QUIT, NULL, LBMCT_CTRLR_CMD_NO_FREE);
  if (err != LBM_OK) EL("quit cmd submit failed");
  PRT_THREAD_JOIN(ct->ctrlr_thread_id);

  if (ct->metadata != NULL) {
    free(ct->metadata);
    ct->metadata = NULL;
  }

  if (ct->msg_props != NULL) lbm_msg_properties_delete(ct->msg_props);

  lbmct_cmd_q_delete(ct);

  mul_asl_delete(ct->src_conn_asl);
  mul_asl_delete(ct->ct_src_asl);

  PRT_VOL32(ct->sig) = LBMCT_SIG_DEAD;
  free(ct);

  return LBM_OK;
}  /* lbmct_delete */


/* Public API to dump debug output to log. */
void lbmct_debug_dump(lbmct_t *ct, const char *msg)
{
  lbm_uint32_t i;
  lbm_uint32_t num_to_print;

  lbm_logf(LBM_LOG_NOTICE,
    "lbmct_debug_dump: '%s', num_recent_events=%u\n",
    msg, ct->num_recent_events);

  if (ct->num_recent_events < LBMCT_MAX_RECENT_EVENTS) {
    i = 0;
    num_to_print = ct->num_recent_events;
  } else {
    i = ct->num_recent_events % LBMCT_MAX_RECENT_EVENTS;
    num_to_print = LBMCT_MAX_RECENT_EVENTS;
  }

  while (num_to_print > 0) {
    lbm_logf(LBM_LOG_NOTICE, "  event[%u]=0x%08x\n",
      ct->num_recent_events - num_to_print,
      ct->recent_events[i % LBMCT_MAX_RECENT_EVENTS]);
    i ++;
    num_to_print --;
  }
}  /* lbmct_debug_dump */


/*******************************************************************************
 * The functions below assist in sending commands to the tc ctrlr thread and
 * getting results back out.
 ******************************************************************************/


/* The following two helper functions let a caller submit a command to the
 * work queue and do a blocking wait for command completion.
 */
int lbmct_ctrlr_cmd_complete_wakeup_cb(lbmct_ctrlr_cmd_t *cmd)
{
  /* Sanity checks. */
  if (cmd->sig != LBMCT_SIG_CTRLR_CMD) E_RTN(E_BAD_SIG(cmd), -1);

  PRT_SEM_POST(cmd->complete_sem);

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_complete_wakeup_cb */


int lbmct_ctrlr_cmd_submit_and_wait(lbmct_t *ct,
  enum lbmct_ctrlr_cmd_type cmd_type, void *cmd_data)
{
  lbmct_ctrlr_cmd_t *cmd;
  int cmd_err;
  char *cmd_errmsg;
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  err = lbm_tl_queue_dequeue(ct->ctrlr_cmd_free_tlq, (void **)&cmd, 1);
  if (err != LBM_OK) E_RTN("dequeue failed", -1);

  cmd->cmd_type = cmd_type;
  cmd->complete_cb = lbmct_ctrlr_cmd_complete_wakeup_cb;
  cmd->cmd_data = cmd_data;
  cmd->cmd_disposition = LBMCT_CTRLR_CMD_NO_FREE;
  cmd->cmd_err = LBM_OK;
  cmd->cmd_errmsg = NULL;

  err = lbm_tl_queue_enqueue(ct->ctrlr_cmd_work_tlq, cmd);
  if (err != LBM_OK) E_RTN("enqueue failed", -1);

  /* Wait for the ctrlr thread to finish doing the command. */
  PRT_SEM_WAIT(cmd->complete_sem);

  /* Grab the status and error message before returning the cmd to free pool. */
  cmd_err = cmd->cmd_err;
  cmd_errmsg = cmd->cmd_errmsg;

  err = lbm_tl_queue_enqueue(ct->ctrlr_cmd_free_tlq, cmd);
  if (err != LBM_OK) E_RTN("enqueue failed", -1);

  /* If the ctrlr thread returned an error message, it needs to be freed. */
  if (cmd_errmsg != NULL) {
    lbm_seterrf(LBM_EINVAL, "Error at %s:%d, '%s'",
      BASENAME(__FILE__), __LINE__, cmd_errmsg);
    free(cmd_errmsg);
  }

  return cmd_err;
}  /* lbmct_ctrlr_cmd_submit_and_wait */


/* The following two helper functions let a caller submit a command to the
 * work queue and not wait for completion.
 */
int lbmct_ctrlr_cmd_nowait_cb(lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_t *ct = NULL;
  int err;

  /* Sanity checks. */
  if (cmd->sig != LBMCT_SIG_CTRLR_CMD) E_RTN(E_BAD_SIG(cmd), -1);
  ct = cmd->ct;
  if (ct == NULL) E_RTN("cmd is corrupted", -1);
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  /* Nobody's waiting, log error messages. */
  if (cmd->cmd_errmsg != NULL) {
    EL(cmd->cmd_errmsg);
    free(cmd->cmd_errmsg);
  }

  if (cmd->cmd_disposition == LBMCT_CTRLR_CMD_NEEDS_FREE) {
    free(cmd->cmd_data);
  }

  err = lbm_tl_queue_enqueue(ct->ctrlr_cmd_free_tlq, cmd);
  if (err != LBM_OK) E_RTN("enqueue failed", -1);

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_nowait_cb */


int lbmct_ctrlr_cmd_submit_nowait(lbmct_t *ct,
  enum lbmct_ctrlr_cmd_type cmd_type, void *cmd_data,
  enum lbmct_ctrlr_cmd_disposition cmd_disposition)
{
  lbmct_ctrlr_cmd_t *cmd;
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  err = lbm_tl_queue_dequeue(ct->ctrlr_cmd_free_tlq, (void **)&cmd, 1);
  if (err != LBM_OK) E_RTN("dequeue failed", -1);

  cmd->cmd_type = cmd_type;
  cmd->complete_cb = lbmct_ctrlr_cmd_nowait_cb;
  cmd->cmd_data = cmd_data;
  cmd->cmd_disposition = cmd_disposition;
  cmd->cmd_err = LBM_OK;
  cmd->cmd_errmsg = NULL;

  err = lbm_tl_queue_enqueue(ct->ctrlr_cmd_work_tlq, cmd);
  if (err != LBM_OK) E_RTN("enqueue failed", -1);

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_submit_nowait */


/******************************************************************************
 * Here are the functions that implement the generic ct controller commands.
 * The commands which are specific to sources and receivers are found in
 * lbmct_src.c and lbmct_rcv.c.
 ******************************************************************************/


/* The "test" command is used for unit testing, nor normal operation. */
int lbmct_ctrlr_cmd_test(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_test_t *test = cmd->cmd_data;

  if (test->test_err != LBM_OK) {
    lbm_seterrf(LBM_EINVAL, "Test at %s:%d, %s",
      BASENAME(__FILE__), __LINE__, test->test_str);
    return test->test_err;
  } else {
    /* Overwrite the "test_str" input bugffer with the result.  But snprintf()
     * cannot safely overwrite an input buffer!  So make a temp copy first.
     */
    char *temp_str = strdup(test->test_str);
    snprintf(test->test_str, sizeof(test->test_str), "%s: OK.", temp_str);
    free(temp_str);
  }

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_test */


/* The quit command is used to exit the ct controller thread (ct delete). */
int lbmct_ctrlr_cmd_quit(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) E_RTN(E_BAD_SIG(ct), -1);

  ct->ctrlr_state = LBMCT_CTRLR_STATE_EXITING;

  return LBM_OK;
}  /* lbmct_ctrlr_cmd_quit */


/******************************************************************************
 * Here's the thread that runs the ct controller.
 ******************************************************************************/


/* This enhanced error macro is because PRT_THREAD_EXIT must be used
 * instead of return.
 */
#define EL_THREAD_EXIT(_m) do {\
  char *_lbm_errmsg = strdup(_m);  ENULL(_lbm_errmsg);\
  free(_lbm_errmsg);\
  ct->ctrlr_state = LBMCT_CTRLR_STATE_EXITING;\
  lbm_logf(LBM_LOG_ERR, "Error at %s:%d, '%s'\n",\
      BASENAME(__FILE__), __LINE__, _lbm_errmsg);\
  PRT_THREAD_EXIT;\
} while (0)

PRT_THREAD_ENTRYPOINT lbmct_ctrlr(void *arg)
{
  lbmct_t *ct = (lbmct_t *)arg;
  lbmct_ctrlr_cmd_t *cmd;
  int err;

  /* Sanity checks. */
  if (ct->sig != LBMCT_SIG_CT) EL_THREAD_EXIT(E_BAD_SIG(ct));

  ct->ctrlr_state = LBMCT_CTRLR_STATE_RUNNING;

  while (ct->ctrlr_state != LBMCT_CTRLR_STATE_EXITING) {
    err = lbm_tl_queue_dequeue(ct->ctrlr_cmd_work_tlq, (void **)&cmd, 1);
    if (err != LBM_OK) EL_THREAD_EXIT(lbm_errmsg());

    /* Sanity checks. */
    if (cmd->sig != LBMCT_SIG_CTRLR_CMD) EL(E_BAD_SIG(cmd));
    else if (cmd->ct != ct) EL("Bad ct in command");
    else {
      /* For debugging, remember the past LBMCT_MAX_RECENT_CMDS cmd types. */
      ct->recent_events[(ct->num_recent_events ++) % LBMCT_MAX_RECENT_EVENTS] =
         (lbm_uint32_t)cmd->cmd_type;

      switch(cmd->cmd_type) {
      case LBMCT_CTRLR_CMD_TYPE_TEST:
        err = lbmct_ctrlr_cmd_test(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_QUIT:
        err = lbmct_ctrlr_cmd_quit(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_SRC_CONN_TICK:
        err = lbmct_ctrlr_cmd_src_conn_tick(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_CT_SRC_CREATE:
        err = lbmct_ctrlr_cmd_ct_src_create(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE:
        err = lbmct_ctrlr_cmd_src_handshake(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_CT_SRC_DELETE:
        err = lbmct_ctrlr_cmd_ct_src_delete(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_RCV_CONN_TICK:
        err = lbmct_ctrlr_cmd_rcv_conn_tick(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_CT_RCV_CREATE:
        err = lbmct_ctrlr_cmd_ct_rcv_create(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_RCV_CONN_CREATE:
        err = lbmct_ctrlr_cmd_rcv_conn_create(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_CT_RCV_DELETE:
        err = lbmct_ctrlr_cmd_ct_rcv_delete(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_RCV_CONN_DELETE:
        err = lbmct_ctrlr_cmd_rcv_conn_delete(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_RCV_SEND_C_OK:
        err = lbmct_ctrlr_cmd_rcv_send_c_ok(ct, cmd);
        break;

      case LBMCT_CTRLR_CMD_TYPE_RCV_SEND_D_OK:
        err = lbmct_ctrlr_cmd_rcv_send_d_ok(ct, cmd);
        break;

      default:
        err = -1;
        lbm_seterrf(LBM_EINVAL, "Error at %s:%d, cmd_type=%d",
          BASENAME(__FILE__), __LINE__, cmd->cmd_type);
      }  /* switch cmd_type */

      /* Pass back command execution status. */
      cmd->cmd_err = err;
      if (err == LBM_OK) {
        cmd->cmd_errmsg = NULL;
      }
      else {
        cmd->cmd_errmsg = strdup(lbm_errmsg());
      }

      /* Tell caller the command is done. */
      err = (cmd->complete_cb)(cmd);  /* Return command response. */
      if (err != LBM_OK) EL(lbm_errmsg());  /* Treat as warning. */
    }
  }  /* while not exiting */

  ct->ctrlr_state = LBMCT_CTRLR_STATE_EXITING;
  PRT_THREAD_EXIT;
}  /* lbmct_ctrlr */
