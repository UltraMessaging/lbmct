/* main.cc
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

#include "gtest/gtest.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
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

/* Global (used for clientd testing). */
char *null_ptr = NULL;
lbm_context_t *ctx1 = NULL;
lbm_context_t *ctx2 = NULL;

char src_clientd[] = "SrcClientd";
char rcv_clientd[] = "RcvClientd";
char src_conn_clientd[] = "SrcConnClientd";
char rcv_conn_clientd[] = "RcvConnClientd";

char src_clientd2[] = "SrcClientd2";
char rcv_clientd2[] = "RcvClientd2";
char src_clientd3[] = "SrcClientd3";
char rcv_clientd3[] = "RcvClientd3";
char src_clientd4[] = "SrcClientd4";
char rcv_clientd4[] = "RcvClientd4";
char src_clientd5[] = "SrcClientd5";
char rcv_clientd5[] = "RcvClientd5";

prt_sem_t sync_sem;
int sync_sem_cnt;
int num_domain_ids = 0;
int domain_ids[2] = {0, 0};

unsigned long long test_start_time;

#define NUM_LOGS 64
#define LOG_SZ 511
prt_mutex_t log_lock;
char log_buffer[NUM_LOGS][LOG_SZ+1];
int log_cnt = 0;

prt_mutex_t msg_lock;
char msg_buffer[NUM_LOGS][LOG_SZ+1];
int msg_cnt = 0;


#define LBM_ERR(_e) do {\
  if ((_e) != LBM_OK) {\
    fprintf(stderr, "LBM_ERR failed at %s:%d (%s)\n", BASENAME(__FILE__), __LINE__, lbm_errmsg());\
    fflush(stderr);\
    abort();\
  }\
} while (0)


int test_log_cb(int level, const char *message, void *clientd)
{
  int i;
  unsigned long long log_time;
  int elapsed_time;

  MSEC_CLOCK(log_time);
  elapsed_time = (int)(log_time - test_start_time);

  /* num_domain_ids is at 2 during the test, so this part gets skipped.
   * So this part is active during initialization only. */
  if (num_domain_ids < 2) {
    /* Get numbers in the message ID. */
    int id1 = 0;
    int id2 = 0;
    int ofs = 0;
    char *s = strstr((char *)message, "-");  /* Skip to first dash. */
    if (s != NULL) {
      (void)sscanf(s, "-%9u-%9u:%n", &id1, &id2, &ofs);
    }

    /* Set domain ID? */
    if (id1 == 6259 && id2 == 7) {
      s = strstr((char *)message, "in Domain ID ");
      if (s != NULL) {
        ofs = 0;
        (void)sscanf(s, "in Domain ID %9u.%n",
          &domain_ids[num_domain_ids], &ofs);
        if (ofs != 0) {
          printf("domain_ids[%d]=%d\n",
            num_domain_ids, domain_ids[num_domain_ids]);
          num_domain_ids ++;
        }
      }  /* if 'in Domain ID ' found */
    }  /* if 6259-7 */
  }  /* if num_domain_ids < 2 */

  if (level == LBM_LOG_INFO || strstr(message, "DEBUG") != NULL) {
    /* Don't record info or debug messages. */
    printf("[](%d) %d ms %s", level, elapsed_time, message);  fflush(stdout);
  } else {
    PRT_MUTEX_LOCK(log_lock);
    i = log_cnt;
    log_cnt ++;
    PRT_MUTEX_UNLOCK(log_lock);

    snprintf(log_buffer[i % NUM_LOGS], LOG_SZ,
      "[%d](%d) %d ms %s", i, level, elapsed_time, message);
    printf("%s", log_buffer[i % NUM_LOGS]);  fflush(stdout);
  }

  return 0;
}  /* test_log_cb */


int test_rcv_cb(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
  int i;
  unsigned long long log_time;
  int elapsed_time;
  lbm_msg_properties_iter_t *prop_iter;
  char prop_str[1024];
  int err;

  MSEC_CLOCK(log_time);
  elapsed_time = (int)(log_time - test_start_time);

  PRT_MUTEX_LOCK(msg_lock);
  i = msg_cnt;
  msg_cnt ++;
  PRT_MUTEX_UNLOCK(msg_lock);

  if (msg->type == LBM_MSG_DATA) {
    prop_str[0] = '\0';  /* Empty the prop string. */
    if (msg->properties != NULL) {
      snprintf(prop_str, sizeof(prop_str), ", props=");

      err = lbm_msg_properties_iter_create(&prop_iter);
      LBM_ERR(err);
      err = lbm_msg_properties_iter_first(prop_iter, msg->properties);
      LBM_ERR(err);

      do {
        ASSRT(prop_iter->type == LBM_MSG_PROPERTY_INT);
        snprintf(&prop_str[strlen(prop_str)], sizeof(prop_str)-strlen(prop_str),
          "%s:%d,", prop_iter->name, *((lbm_uint32_t *)(prop_iter->data)));
        err = lbm_msg_properties_iter_next(prop_iter);
      } while (err == LBM_OK);

      prop_str[strlen(prop_str)-1] = '.';  /* Replace trailing ',' with '.'. */

      err = lbm_msg_properties_iter_delete(prop_iter);
      LBM_ERR(err);
    }  /* if properties */

    snprintf(msg_buffer[i % NUM_LOGS], LOG_SZ, "{%d} %d ms test_rcv_cb: type=%d, sqn=%d, source='%s', properties=%s, clientd='%s', source_clientd='%s', data='%s'%s\n",
       i, elapsed_time, msg->type, msg->sequence_number, msg->source,
       (msg->properties == NULL) ? "(nil)" : "(non-nil)",
       clientd, msg->source_clientd, msg->data, prop_str);
  }  /* if msg type data */
  else {
    snprintf(msg_buffer[i % NUM_LOGS], LOG_SZ, "{%d} %d ms test_rcv_cb: type=%d, sqn=%d, source='%s', properties=%s, clientd='%s', source_clientd='%s'\n",
       i, elapsed_time, msg->type, msg->sequence_number, msg->source,
       (msg->properties == NULL) ? "(nil)" : "(non-nil)",
       clientd, msg->source_clientd);
  }
  printf("%s", msg_buffer[i % NUM_LOGS]);  fflush(stdout);

  return 0;
}  /* test_rcv_cb */


char *peer_info_2_str(lbmct_peer_info_t *peer_info)
{
  static char msg_buf[10000];  /* way bigger than it needs to be. */

  msg_buf[0] = '\0';   /* Empty the buffer. */

  sprintf(&msg_buf[strlen(msg_buf)], " status=%d", peer_info->status);

  sprintf(&msg_buf[strlen(msg_buf)], ", flags=0x%x", peer_info->flags);

  if (peer_info->flags & LBMCT_PEER_INFO_FLAGS_SRC_METADATA) {
    if (peer_info->src_metadata == NULL || peer_info->src_metadata_len == 0) {
      sprintf(&msg_buf[strlen(msg_buf)], ", src_metadata=none");
    } else {
      sprintf(&msg_buf[strlen(msg_buf)], ", src_metadata='%s'",
        peer_info->src_metadata);
    }
  }
  else {
    sprintf(&msg_buf[strlen(msg_buf)], ", no src_metadata");
  }

  if (peer_info->flags & LBMCT_PEER_INFO_FLAGS_RCV_METADATA) {
    if (peer_info->rcv_metadata == NULL || peer_info->rcv_metadata_len == 0) {
      sprintf(&msg_buf[strlen(msg_buf)], ", rcv_metadata=none");
    } else {
      sprintf(&msg_buf[strlen(msg_buf)], ", rcv_metadata='%s'",
        peer_info->rcv_metadata);
    }
  }
  else {
    sprintf(&msg_buf[strlen(msg_buf)], ", no rcv_metadata");
  }

  if (peer_info->flags & LBMCT_PEER_INFO_FLAGS_RCV_SOURCE_NAME)
    sprintf(&msg_buf[strlen(msg_buf)], ", rcv_source_name='%s'", peer_info->rcv_source_name);
  else
    sprintf(&msg_buf[strlen(msg_buf)], ", no rcv_source_name");

  if (peer_info->flags & LBMCT_PEER_INFO_FLAGS_RCV_START_SEQ_NUM)
    sprintf(&msg_buf[strlen(msg_buf)], ", rcv_start_seq_num=%u", peer_info->rcv_start_seq_num);
  else
    sprintf(&msg_buf[strlen(msg_buf)], ", no rcv_start_seq_num");

  if (peer_info->flags & LBMCT_PEER_INFO_FLAGS_RCV_END_SEQ_NUM)
    sprintf(&msg_buf[strlen(msg_buf)], ", rcv_end_seq_num=%u", peer_info->rcv_end_seq_num);
  else
    sprintf(&msg_buf[strlen(msg_buf)], ", no rcv_end_seq_num");

  return msg_buf;
}  /* peer_info_2_str */


void *test_src_conn_create_cb(lbmct_src_conn_t *src_conn,
  lbmct_peer_info_t *peer_info, void *src_clientd)
{
  lbm_logf(LBM_LOG_NOTICE,
    "test_src_conn_create_cb, clientd='%s', peer: %s\n",
    (char *)src_clientd, peer_info_2_str(peer_info));
  sync_sem_cnt ++;  PRT_SEM_POST(sync_sem);
  return src_conn_clientd;
}  /* test_src_conn_create_cb */


void test_src_conn_delete_cb(lbmct_src_conn_t *src_conn,
  lbmct_peer_info_t *peer_info, void *src_clientd, void *conn_clientd)
{
  lbm_logf(LBM_LOG_NOTICE,
    "test_src_conn_delete_cb, clientd='%s', conn_clientd='%s', peer: %s\n",
    (char *)src_clientd, (char *)conn_clientd, peer_info_2_str(peer_info));
  ASSERT_EQ(src_clientd, src_clientd);
  ASSERT_EQ(src_conn_clientd, conn_clientd);
}  /* test_src_conn_delete_cb */


/* When there is a race and the src and rcv callbacks happen at the same
 * time, let the src win.
 */
void *test_rcv_conn_create_cb(lbmct_rcv_conn_t *rcv_conn,
  lbmct_peer_info_t *peer_info, void *rcv_clientd)
{
  SLEEP_MSEC(2);
  lbm_logf(LBM_LOG_NOTICE,
    "test_rcv_conn_create_cb, clientd='%s', peer: %s\n",
    (char *)rcv_clientd, peer_info_2_str(peer_info));
  sync_sem_cnt ++;  PRT_SEM_POST(sync_sem);
  return rcv_conn_clientd;
}  /* test_rcv_conn_create_cb */


void test_rcv_conn_delete_cb(lbmct_rcv_conn_t *rcv_conn,
  lbmct_peer_info_t *peer_info, void *rcv_clientd, void *conn_clientd)
{
  lbm_logf(LBM_LOG_NOTICE,
    "test_rcv_conn_delete_cb, clientd='%s', conn_clientd='%s', peer: %s\n",
    (char *)rcv_clientd, (char *)conn_clientd, peer_info_2_str(peer_info));
  ASSERT_EQ(rcv_clientd, rcv_clientd);
  ASSERT_EQ(rcv_conn_clientd, conn_clientd);
}  /* test_rcv_conn_delete_cb */


TEST(Ct,CtRetryExceed2) {
  lbmct_t *s_ct = NULL;
  lbmct_t *r_ct = NULL;
  lbmct_src_t *ct_src;
  lbmct_rcv_t *ct_rcv;
  lbmct_config_t ct_config;
  int err;

  signal(SIGPIPE, SIG_IGN);
  SLEEP_MSEC(100);
  log_cnt = 0;
  msg_cnt = 0;
  MSEC_CLOCK(test_start_time);
  ASSERT_EQ(0, sync_sem_cnt);

  /* Speed up timeouts. */
  ct_config.flags = 0;
  ct_config.retry_ivl = 500;
  ct_config.flags |= LBMCT_CT_CONFIG_FLAGS_RETRY_IVL;
  ct_config.max_tries = 3;
  ct_config.flags |= LBMCT_CT_CONFIG_FLAGS_MAX_TRIES;

  err = lbmct_create(&s_ct, ctx1, &ct_config, "Meta s_ct", 10);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_create(&r_ct, ctx2, NULL, "Meta r_ct", 10);
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* Let's drop the c_ok msgs, make sure the source cleans up.
   */
  r_ct->active_config.test_bits |= LBMCT_TEST_BITS_NO_C_OK;

  err = lbmct_src_create(&ct_src, s_ct, "CtRetryExceed2", NULL, NULL,
    test_src_conn_create_cb, test_src_conn_delete_cb, src_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(10);

  err = lbmct_rcv_create(&ct_rcv, r_ct, "CtRetryExceed2", NULL, test_rcv_cb,
    test_rcv_conn_create_cb, test_rcv_conn_delete_cb, rcv_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(300);

  ASSERT_NE(null_ptr, strstr(log_buffer[0], "test_rcv_conn_create_cb, clientd='RcvClientd', peer:  status=0, flags=0x3f, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct', rcv_source_name='TCP:"));
  ASSERT_NE(null_ptr, strstr(log_buffer[1], "LBMCT_TEST_BITS_NO_C_OK"));
  ASSERT_EQ(2, log_cnt);

  SLEEP_MSEC(2000);
  PRT_SEM_WAIT(sync_sem);  sync_sem_cnt --;
  ASSERT_EQ(0, sync_sem_cnt);

  ASSERT_NE(null_ptr, strstr(log_buffer[2], "LBMCT_TEST_BITS_NO_C_OK"));
  ASSERT_NE(null_ptr, strstr(log_buffer[3], "LBMCT_TEST_BITS_NO_C_OK"));
  ASSERT_NE(null_ptr, strstr(log_buffer[4], "giving up accepting connection from receiver "));
  ASSERT_EQ(5, log_cnt);

  err = lbmct_src_delete(ct_src);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(500);
  /* The source doesn't think the connection doesn't even exist.  However,
   * the deletion of the underlying UM source causes the rcv per-source
   * delete callback, abruptly deletes the connection without DREQ. */
  ASSERT_NE(null_ptr, strstr(log_buffer[5], "test_rcv_conn_delete_cb, clientd='RcvClientd', conn_clientd='RcvConnClientd', peer:  status=-1, flags=0x3f, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct', rcv_source_name='TCP:"));
  ASSERT_EQ(6, log_cnt);

  err = lbmct_rcv_delete(ct_rcv);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(200);

  ASSERT_EQ(6, log_cnt);

  err = lbmct_delete(s_ct);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_delete(r_ct);
  ASSERT_EQ(0, err) << lbm_errmsg();

  ASSERT_EQ(6, log_cnt);
}  /* CtRetryExceed2 */


TEST(Ct,CtEchoTest) {
  lbmct_t *ct = NULL;
  lbmct_ctrlr_cmd_test_t test;
  int err;

  signal(SIGPIPE, SIG_IGN);
  SLEEP_MSEC(100);
  log_cnt = 0;
  msg_cnt = 0;
  MSEC_CLOCK(test_start_time);
  ASSERT_EQ(0, sync_sem_cnt);

  /* Test ct creation and deletion. */
  err = lbmct_create(&ct, ctx1, NULL, "Hi", 3);
  ASSERT_EQ(0, err) << lbm_errmsg();

  ASSERT_EQ(LBMCT_CT_CONFIG_DEFAULT_TEST_BITS, ct->active_config.test_bits);
  ASSERT_EQ(LBMCT_CT_CONFIG_DEFAULT_DOMAIN_ID, ct->active_config.domain_id);
  ASSERT_EQ(LBMCT_CT_CONFIG_DEFAULT_DELAY_CREQ, ct->active_config.delay_creq);
  ASSERT_EQ(LBMCT_CT_CONFIG_DEFAULT_MAX_TRIES, ct->active_config.max_tries);
  ASSERT_EQ(LBMCT_CT_CONFIG_DEFAULT_RETRY_IVL, ct->active_config.retry_ivl);

  sprintf(test.test_str, "Testing...");
  test.test_err = LBM_OK;
  err = lbmct_ctrlr_cmd_submit_and_wait(ct, LBMCT_CTRLR_CMD_TYPE_TEST, &test);
  ASSERT_EQ(0, err) << lbm_errmsg();
  ASSERT_STREQ("Testing...: OK.", test.test_str);
  ASSERT_EQ(0, log_cnt);

  sprintf(test.test_str, "Failing...");
  test.test_err = -1;
  err = lbmct_ctrlr_cmd_submit_and_wait(ct, LBMCT_CTRLR_CMD_TYPE_TEST, &test);
  ASSERT_EQ(-1, err);
  ASSERT_NE(null_ptr, strstr(lbm_errmsg(), "Test at "));
  ASSERT_NE(null_ptr, strstr(lbm_errmsg(), "Failing..."));
  ASSERT_EQ(0, log_cnt);

  lbmct_debug_dump(ct, "ct");
  ASSERT_NE(null_ptr, strstr(log_buffer[0], "lbmct_debug_dump: 'ct', num_recent_events=2"));
  ASSERT_NE(null_ptr, strstr(log_buffer[1], "event[0]=0x00000001"));
  ASSERT_NE(null_ptr, strstr(log_buffer[2], "event[1]=0x00000001"));
  ASSERT_EQ(3, log_cnt);

  /* When built with DEBUG, don't test this.  (DEBUG causes this test to
   * break into the debugger.)
   */
#ifndef DEBUG
  sprintf(test.test_str, "Failing...");
  test.test_err = -1;
  err = lbmct_ctrlr_cmd_submit_nowait(ct, LBMCT_CTRLR_CMD_TYPE_TEST, &test,
    LBMCT_CTRLR_CMD_NO_FREE);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(50);  /* Give it a chance to execute. */
  ASSERT_NE(null_ptr, strstr(log_buffer[3], "Test at "));
  ASSERT_NE(null_ptr, strstr(log_buffer[3], "Failing..."));
  ASSERT_EQ(4, log_cnt);
#endif

  err = lbmct_delete(ct);
  ASSERT_EQ(0, err) << lbm_errmsg();

#ifndef DEBUG
  ASSERT_EQ(4, log_cnt);
#else
  ASSERT_EQ(3, log_cnt);
#endif
}  /* Ct,CtEchoTest */


TEST(Ct,CtRetryExceed1) {
  lbmct_t *s_ct = NULL;
  lbmct_t *r_ct = NULL;
  lbmct_src_t *ct_src;
  lbmct_rcv_t *ct_rcv;
  lbmct_config_t ct_config;
  int err;

  signal(SIGPIPE, SIG_IGN);
  SLEEP_MSEC(100);
  log_cnt = 0;
  msg_cnt = 0;
  MSEC_CLOCK(test_start_time);
  ASSERT_EQ(0, sync_sem_cnt);

  /* Speed up timeouts. */
  ct_config.flags = 0;
  ct_config.retry_ivl = 500;
  ct_config.flags |= LBMCT_CT_CONFIG_FLAGS_RETRY_IVL;
  ct_config.max_tries = 3;
  ct_config.flags |= LBMCT_CT_CONFIG_FLAGS_MAX_TRIES;

  err = lbmct_create(&s_ct, ctx1, &ct_config, "Meta s_ct", 10);
  ASSERT_EQ(0, err) << lbm_errmsg();
  /* Make sure the receiver gives up first (for predictable order of logs). */
  ct_config.retry_ivl = 490;
  err = lbmct_create(&r_ct, ctx2, &ct_config, "Meta r_ct", 10);
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* Let's drop the crsp msgs, make sure both clean up.
   */
  s_ct->active_config.test_bits |= LBMCT_TEST_BITS_NO_CRSP;

  err = lbmct_src_create(&ct_src, s_ct, "CtRetryExceed1", NULL, NULL,
    test_src_conn_create_cb, test_src_conn_delete_cb, src_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(10);

  err = lbmct_rcv_create(&ct_rcv, r_ct, "CtRetryExceed1", NULL, test_rcv_cb,
    test_rcv_conn_create_cb, test_rcv_conn_delete_cb, rcv_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(200);

  ASSERT_NE(null_ptr, strstr(log_buffer[0], "LBMCT_TEST_BITS_NO_CRSP"));
  ASSERT_NE(null_ptr, strstr(log_buffer[1], "LBMCT_TEST_BITS_NO_CRSP"));
  ASSERT_EQ(2, log_cnt);

  SLEEP_MSEC(2000);
  ASSERT_EQ(0, sync_sem_cnt);

  ASSERT_NE(null_ptr, strstr(log_buffer[2], "LBMCT_TEST_BITS_NO_CRSP"));
  ASSERT_NE(null_ptr, strstr(log_buffer[3], "giving up connecting to source "));
  ASSERT_NE(null_ptr, strstr(log_buffer[4], "LBMCT_TEST_BITS_NO_CRSP"));
  ASSERT_NE(null_ptr, strstr(log_buffer[5], "LBMCT_TEST_BITS_NO_CRSP"));
  ASSERT_NE(null_ptr, strstr(log_buffer[6], "giving up accepting connection from receiver "));
  ASSERT_EQ(7, log_cnt);

  ASSERT_EQ(0, msg_cnt);

  err = lbmct_src_delete(ct_src);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(200);
  /* The source doesn't think the connection doesn't even exist.  However,
   * the deletion of the underlying UM source causes the rcv per-source
   * delete callback, abruptly deletes the connection without DREQ. */
  ASSERT_EQ(7, log_cnt);

  err = lbmct_rcv_delete(ct_rcv);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(200);

  ASSERT_EQ(7, log_cnt);

  err = lbmct_delete(s_ct);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_delete(r_ct);
  ASSERT_EQ(0, err) << lbm_errmsg();

  ASSERT_EQ(7, log_cnt);
  ASSERT_EQ(0, msg_cnt);
}  /* CtRetryExceed1 */


TEST(Ct,CtCreateDeleteTest) {
  lbmct_t *ct = NULL;
  lbmct_src_t *ct_src;
  lbmct_rcv_t *ct_rcv;
  lbm_src_topic_attr_t *sattr = NULL;
  lbm_rcv_topic_attr_t *rattr = NULL;
  lbmct_config_t ct_config;
  lbm_ulong_t opt_val;
  size_t opt_len;
  int err;

  signal(SIGPIPE, SIG_IGN);
  SLEEP_MSEC(100);
  log_cnt = 0;
  msg_cnt = 0;
  MSEC_CLOCK(test_start_time);
  ASSERT_EQ(0, sync_sem_cnt);

  /* Test setting configs. These settings are not intended to *do* anything,
   * just verify that the settings make it into the active config.
   */
  ct_config.flags = 0;
  ct_config.domain_id = 1;
  ct_config.flags |= LBMCT_CT_CONFIG_FLAGS_DOMAIN_ID;
  ct_config.retry_ivl = 2000;
  ct_config.flags |= LBMCT_CT_CONFIG_FLAGS_RETRY_IVL;
  ct_config.max_tries = 3;
  ct_config.flags |= LBMCT_CT_CONFIG_FLAGS_MAX_TRIES;
  ct_config.test_bits = 4;
  ct_config.flags |= LBMCT_CT_CONFIG_FLAGS_TEST_BITS;

  /* Test ct creation and deletion. */
  err = lbmct_create(&ct, ctx1, &ct_config, "Hi", 3);  /* Metadata is "Hi". */
  ASSERT_EQ(0, err) << lbm_errmsg();

  ASSERT_EQ(1, ct->active_config.domain_id);
  ASSERT_EQ(2000, ct->active_config.retry_ivl);
  ASSERT_EQ(3, ct->active_config.max_tries);
  ASSERT_EQ(4, ct->active_config.test_bits);

  /* Ready to create source. */

  /* Set meaningless attribute to verify it makes it to the source. */
  err = lbm_src_topic_attr_create(&sattr);
  ASSERT_EQ(0, err) << lbm_errmsg();

  opt_val = 1234;
  err = lbm_src_topic_attr_setopt(sattr,
    "resolver_advertisement_sustain_interval", &opt_val, sizeof(opt_val));
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* Create the connected source. */
  err = lbmct_src_create(&ct_src, ct, "CtCreateDeleteTest", sattr, NULL,
    test_src_conn_create_cb, test_src_conn_delete_cb, src_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbm_src_topic_attr_delete(sattr);
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* Verify the previous attribute made it. */
  opt_val = 0;
  opt_len = sizeof(opt_val);
  err = lbm_src_getopt(ct_src->um_src,
    "resolver_advertisement_sustain_interval", &opt_val, &opt_len);
  ASSERT_EQ(0, err) << lbm_errmsg();
  ASSERT_EQ(1234, opt_val);

  /* Ready to create receiver. */

  /* Set meaningless attribute to verify it makes it to the source. */
  err = lbm_rcv_topic_attr_create(&rattr);
  ASSERT_EQ(0, err) << lbm_errmsg();

  opt_val = 4321;
  err = lbm_rcv_topic_attr_setopt(rattr,
    "resolver_query_sustain_interval", &opt_val, sizeof(opt_val));
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* Make the receiver a different topic so that they don't try to connect.
   * We're just trying to test creation and deletion of the main objects;
   * connection create/delete happens in subsequent tests.
   */
  err = lbmct_rcv_create(&ct_rcv, ct, "CtCreateDeleteTest2", rattr,
    test_rcv_cb, test_rcv_conn_create_cb, test_rcv_conn_delete_cb, rcv_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbm_rcv_topic_attr_delete(rattr);
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* Verify the previous attribute made it. */
  opt_val = 0;
  opt_len = sizeof(opt_val);
  err = lbm_rcv_getopt(ct_rcv->um_rcv,
    "resolver_query_sustain_interval", &opt_val, &opt_len);
  ASSERT_EQ(0, err) << lbm_errmsg();
  ASSERT_EQ(4321, opt_val);

  /* Now delete everything. */

  err = lbmct_src_delete(ct_src);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbmct_rcv_delete(ct_rcv);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbmct_delete(ct);
  ASSERT_EQ(0, err) << lbm_errmsg();

  ASSERT_EQ(0, log_cnt);
  ASSERT_EQ(0, msg_cnt);
}  /* Ct,CtCreateDeleteTest */


TEST(Ct,RetryExceed3) {
  lbmct_t *s_ct = NULL;
  lbmct_t *r_ct = NULL;
  lbmct_src_t *ct_src;
  lbmct_rcv_t *ct_rcv;
  lbmct_config_t ct_config;
  int err;

  signal(SIGPIPE, SIG_IGN);
  SLEEP_MSEC(100);
  log_cnt = 0;
  msg_cnt = 0;
  MSEC_CLOCK(test_start_time);
  ASSERT_EQ(0, sync_sem_cnt);

  /* Speed up timeouts. */
  ct_config.flags = 0;
  ct_config.retry_ivl = 500;
  ct_config.flags |= LBMCT_CT_CONFIG_FLAGS_RETRY_IVL;
  ct_config.max_tries = 3;
  ct_config.flags |= LBMCT_CT_CONFIG_FLAGS_MAX_TRIES;

  err = lbmct_create(&s_ct, ctx1, &ct_config, "Meta s_ct", 10);
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* Make sure the receiver gives up first (for predictable order of logs). */
  ct_config.retry_ivl = 490;
  err = lbmct_create(&r_ct, ctx2, &ct_config, "Meta r_ct", 10);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbmct_src_create(&ct_src, s_ct, "RetryExceed3", NULL, NULL,
    test_src_conn_create_cb, test_src_conn_delete_cb, src_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(10);

  err = lbmct_rcv_create(&ct_rcv, r_ct, "RetryExceed3", NULL, test_rcv_cb,
    test_rcv_conn_create_cb, test_rcv_conn_delete_cb, rcv_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* Wait for src and rcv connect events. */
  PRT_SEM_WAIT(sync_sem);  sync_sem_cnt --;
  PRT_SEM_WAIT(sync_sem);  sync_sem_cnt --;
  ASSERT_EQ(0, sync_sem_cnt);

  ASSERT_NE(null_ptr, strstr(log_buffer[0], "test_rcv_conn_create_cb, clientd='RcvClientd', peer:  status=0, flags=0x3f, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct', rcv_source_name='TCP:"));
  ASSERT_NE(null_ptr, strstr(log_buffer[1], "test_src_conn_create_cb, clientd='SrcClientd', peer:  status=0, flags=0x2f, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct', no rcv_source_name,"));
  ASSERT_EQ(2, log_cnt);

  ASSERT_NE(null_ptr, strstr(msg_buffer[0], "test_rcv_cb: type=0, sqn=0, source='TCP:"));
  ASSERT_NE(null_ptr, strstr(msg_buffer[0], "clientd='RcvClientd', source_clientd='RcvConnClientd', data='CRSP,"));
  ASSERT_EQ(1, msg_cnt);

  /* Drop all DRSPs so that both sides have to time out and force delete.
   */
  s_ct->active_config.test_bits |= LBMCT_TEST_BITS_NO_DRSP;
  err = lbmct_rcv_delete(ct_rcv);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(3000);
  r_ct->active_config.test_bits &= ~LBMCT_TEST_BITS_NO_DRSP;

  ASSERT_NE(null_ptr, strstr(log_buffer[2], "LBMCT_TEST_BITS_NO_DRSP"));
  ASSERT_NE(null_ptr, strstr(log_buffer[3], "LBMCT_TEST_BITS_NO_DRSP"));
  ASSERT_NE(null_ptr, strstr(log_buffer[4], "LBMCT_TEST_BITS_NO_DRSP"));
  ASSERT_NE(null_ptr, strstr(log_buffer[5], "giving up closing connection to source "));
  ASSERT_NE(null_ptr, strstr(log_buffer[6], "test_rcv_conn_delete_cb, clientd='RcvClientd', conn_clientd='RcvConnClientd', peer:  status=-1, flags=0x3f, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct', rcv_source_name='TCP:"));
  ASSERT_NE(null_ptr, strstr(log_buffer[7], "LBMCT_TEST_BITS_NO_DRSP"));
  ASSERT_NE(null_ptr, strstr(log_buffer[8], "LBMCT_TEST_BITS_NO_DRSP"));
  ASSERT_NE(null_ptr, strstr(log_buffer[9], "giving up closing connection from receiver"));
  ASSERT_NE(null_ptr, strstr(log_buffer[10], "test_src_conn_delete_cb, clientd='SrcClientd', conn_clientd='SrcConnClientd', peer:  status=-1, flags=0x2f, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct', no rcv_source_name, rcv_start_seq_num=0, no rcv_end_seq_num"));
  ASSERT_EQ(11, log_cnt);

  err = lbmct_src_delete(ct_src);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(50);

  err = lbmct_delete(s_ct);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_delete(r_ct);
  ASSERT_EQ(0, err) << lbm_errmsg();

  ASSERT_EQ(11, log_cnt);
  ASSERT_EQ(1, msg_cnt);
}  /* RetryExceed3 */


TEST(Ct,CtMultiStream) {
  lbmct_t *ct1 = NULL;
  lbmct_t *ct2 = NULL;
  lbmct_src_t *ct_src1;
  lbmct_src_t *ct_src2;
  lbmct_src_t *ct_src3;
  lbmct_src_t *ct_src4;
  lbmct_src_t *ct_src5;
  lbmct_rcv_t *ct_rcv1;
  lbmct_rcv_t *ct_rcv2;
  lbmct_rcv_t *ct_rcv3;
  lbmct_rcv_t *ct_rcv4;
  lbmct_rcv_t *ct_rcv5;
  int i;
  int err;

  signal(SIGPIPE, SIG_IGN);
  SLEEP_MSEC(100);
  log_cnt = 0;
  msg_cnt = 0;
  MSEC_CLOCK(test_start_time);
  ASSERT_EQ(0, sync_sem_cnt);

  err = lbmct_create(&ct1, ctx1, NULL, "Meta ct1", 10);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbmct_create(&ct2, ctx2, NULL, "Meta ct2", 10);
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* First stream: 1 src, 2 rcv. */

  err = lbmct_src_create(&ct_src1, ct1, "CtMultiStream1", NULL, NULL,
    test_src_conn_create_cb, test_src_conn_delete_cb, src_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbmct_rcv_create(&ct_rcv1, ct1, "CtMultiStream1", NULL, test_rcv_cb,
    test_rcv_conn_create_cb, test_rcv_conn_delete_cb, rcv_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(20);

  err = lbmct_rcv_create(&ct_rcv2, ct2, "CtMultiStream1", NULL, test_rcv_cb,
    test_rcv_conn_create_cb, test_rcv_conn_delete_cb, rcv_clientd2);

  SLEEP_MSEC(20);

  /* Second stream: 2 src, 1 rcv. */

  err = lbmct_rcv_create(&ct_rcv3, ct1, "CtMultiStream2", NULL, test_rcv_cb,
    test_rcv_conn_create_cb, test_rcv_conn_delete_cb, rcv_clientd3);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbmct_src_create(&ct_src2, ct1, "CtMultiStream2", NULL, NULL,
    test_src_conn_create_cb, test_src_conn_delete_cb, src_clientd2);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(20);

  err = lbmct_src_create(&ct_src3, ct2, "CtMultiStream2", NULL, NULL,
    test_src_conn_create_cb, test_src_conn_delete_cb, src_clientd3);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(20);

  /* Third stream: 2 src, 2 rcv */

  err = lbmct_src_create(&ct_src4, ct1, "CtMultiStream3", NULL, NULL,
    test_src_conn_create_cb, test_src_conn_delete_cb, src_clientd4);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbmct_rcv_create(&ct_rcv4, ct1, "CtMultiStream3", NULL, test_rcv_cb,
    test_rcv_conn_create_cb, test_rcv_conn_delete_cb, rcv_clientd4);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(20);

  err = lbmct_src_create(&ct_src5, ct2, "CtMultiStream3", NULL, NULL,
    test_src_conn_create_cb, test_src_conn_delete_cb, src_clientd5);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(20);

  err = lbmct_rcv_create(&ct_rcv5, ct2, "CtMultiStream3", NULL, test_rcv_cb,
    test_rcv_conn_create_cb, test_rcv_conn_delete_cb, rcv_clientd5);
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* Wait for src and rcv connect events. */
  for (i = 0; i < 16; i++) {
    PRT_SEM_WAIT(sync_sem);  sync_sem_cnt --;
  }
  ASSERT_EQ(0, sync_sem_cnt);

  SLEEP_MSEC(20);

  /*ASSERT_NE(null_ptr, strstr(log_buffer[0], "test_rcv_conn_create_cb, clientd='RcvClientd', peer:  status=0, flags=0x3f, src_metadata='Meta ct1', rcv_metadata='Meta ct1', rcv_source_name='TCP:"));
   *ASSERT_NE(null_ptr, strstr(log_buffer[1], "test_src_conn_create_cb, clientd='SrcClientd', peer:  status=0, flags=0x2f, src_metadata='Meta ct1', rcv_metadata='Meta ct1', no rcv_source_name,"));
   *ASSERT_NE(null_ptr, strstr(log_buffer[2], "test_rcv_conn_create_cb, clientd='RcvClientd2', peer:  status=0, flags=0x3f, src_metadata='Meta ct1', rcv_metadata='Meta ct2', rcv_source_name='TCP:"));
   *ASSERT_NE(null_ptr, strstr(log_buffer[3], "test_src_conn_create_cb, clientd='SrcClientd', peer:  status=0, flags=0x2f, src_metadata='Meta ct1', rcv_metadata='Meta ct2', no rcv_source_name,"));
   *ASSERT_NE(null_ptr, strstr(log_buffer[4], "test_rcv_conn_create_cb, clientd='RcvClientd3', peer:  status=0, flags=0x3f, src_metadata='Meta ct1', rcv_metadata='Meta ct1', rcv_source_name='TCP:"));
   *ASSERT_NE(null_ptr, strstr(log_buffer[5], "test_src_conn_create_cb, clientd='SrcClientd2', peer:  status=0, flags=0x2f, src_metadata='Meta ct1', rcv_metadata='Meta ct1', no rcv_source_name,"));
   *ASSERT_NE(null_ptr, strstr(log_buffer[6], "test_rcv_conn_create_cb, clientd='RcvClientd3', peer:  status=0, flags=0x3f, src_metadata='Meta ct2', rcv_metadata='Meta ct1', rcv_source_name='TCP:"));
   *ASSERT_NE(null_ptr, strstr(log_buffer[7], "test_src_conn_create_cb, clientd='SrcClientd3', peer:  status=0, flags=0x2f, src_metadata='Meta ct2', rcv_metadata='Meta ct1', no rcv_source_name,"));
   *ASSERT_NE(null_ptr, strstr(log_buffer[8], "test_rcv_conn_create_cb, clientd='RcvClientd4', peer:  status=0, flags=0x3f, src_metadata='Meta ct1', rcv_metadata='Meta ct1', rcv_source_name='TCP:"));
   *ASSERT_NE(null_ptr, strstr(log_buffer[9], "test_src_conn_create_cb, clientd='SrcClientd4', peer:  status=0, flags=0x2f, src_metadata='Meta ct1', rcv_metadata='Meta ct1', no rcv_source_name,"));
   *ASSERT_NE(null_ptr, strstr(log_buffer[10], "test_rcv_conn_create_cb, clientd='RcvClientd4', peer:  status=0, flags=0x3f, src_metadata='Meta ct2', rcv_metadata='Meta ct1', rcv_source_name='TCP:"));
   *ASSERT_NE(null_ptr, strstr(log_buffer[11], "test_src_conn_create_cb, clientd='SrcClientd5', peer:  status=0, flags=0x2f, src_metadata='Meta ct2', rcv_metadata='Meta ct1', no rcv_source_name,"));
   */

  /* The last 4 logs (2 src, 2 rcv) are from the final receiver create.  The
   * order of the 2 src logs is not predictable.  Same with the 2 rcv logs.
   */
  /*ASSERT_NE(null_ptr, strstr(log_buffer[12], "test_rcv_conn_create_cb, "));
   *ASSERT_NE(null_ptr, strstr(log_buffer[13], "test_rcv_conn_create_cb, "));
   *ASSERT_NE(null_ptr, strstr(log_buffer[14], "test_src_conn_create_cb, "));
   *ASSERT_NE(null_ptr, strstr(log_buffer[15], "test_src_conn_create_cb, "));
   */

  /* The exact order of the logs is not predictable.  Visually check that
   * they are correct.
   */
  ASSERT_EQ(16, log_cnt);
  /* The exact order of the messages is not predictable.  Visually check that
   * they are correct.
   */
  ASSERT_EQ(11, msg_cnt);

  /* Send messages. */

  err = lbm_src_send(lbmct_src_get_um_src(ct_src1), "msg0", 5, LBM_MSG_FLUSH);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(10);
  ASSERT_EQ(13, msg_cnt);

  err = lbm_src_send(lbmct_src_get_um_src(ct_src2), "msg1", 5, LBM_MSG_FLUSH);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(10);
  ASSERT_EQ(14, msg_cnt);

  err = lbm_src_send(lbmct_src_get_um_src(ct_src3), "msg2", 5, LBM_MSG_FLUSH);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(10);
  ASSERT_EQ(15, msg_cnt);

  err = lbm_src_send(lbmct_src_get_um_src(ct_src4), "msg3", 5, LBM_MSG_FLUSH);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(10);
  ASSERT_EQ(17, msg_cnt);

  err = lbm_src_send(lbmct_src_get_um_src(ct_src5), "msg4", 5, LBM_MSG_FLUSH);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(10);
  ASSERT_EQ(19, msg_cnt);

  ASSERT_EQ(16, log_cnt);

  /* Shut everything down. */

  err = lbmct_src_delete(ct_src1);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_src_delete(ct_src2);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_src_delete(ct_src3);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_src_delete(ct_src4);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_src_delete(ct_src5);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbmct_rcv_delete(ct_rcv1);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_rcv_delete(ct_rcv2);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_rcv_delete(ct_rcv3);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_rcv_delete(ct_rcv4);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_rcv_delete(ct_rcv5);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(200);
  ASSERT_LE(32, log_cnt);
  ASSERT_EQ(30, msg_cnt);

  err = lbmct_delete(ct1);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbmct_delete(ct2);
  ASSERT_EQ(0, err) << lbm_errmsg();

  ASSERT_LE(32, log_cnt);
  ASSERT_EQ(30, msg_cnt);
}  /* CtMultiStream */


int test_src_cb(lbm_src_t *src, int event, void *ed, void *clientd)
{
  lbm_logf(LBM_LOG_NOTICE,
    "test_src_cb, clientd='%s', event=%d\n",
    (char *)src_clientd, event);

  return LBM_OK;
}  /* test_src_cb */


TEST(Ct,CtSimpleMessages1) {
  lbmct_t *s_ct = NULL;
  lbmct_t *r_ct = NULL;
  lbmct_src_t *ct_src;
  lbmct_rcv_t *ct_rcv;
  lbmct_config_t ct_config;
  lbm_src_send_ex_info_t send_ex_info;
  lbm_msg_properties_t *msg_props;
  lbm_uint32_t int_prop_val;
  int err;

  signal(SIGPIPE, SIG_IGN);
  SLEEP_MSEC(100);
  log_cnt = 0;
  msg_cnt = 0;
  MSEC_CLOCK(test_start_time);
  ASSERT_EQ(0, sync_sem_cnt);

  err = lbm_msg_properties_create(&msg_props);
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* Set up message property for sent messages. */

  int_prop_val = 9876;
  err = lbm_msg_properties_set(msg_props, "TstProp",
    &int_prop_val, LBM_MSG_PROPERTY_INT, sizeof(int_prop_val));
  ASSERT_EQ(0, err) << lbm_errmsg();

  send_ex_info.flags = LBM_SRC_SEND_EX_FLAG_PROPERTIES;
  send_ex_info.properties = msg_props;

  /* Create ct objects. */

  /* When they want to race, make receiver timeout before source. */
  ct_config.flags = LBMCT_CT_CONFIG_FLAGS_RETRY_IVL;
  ct_config.retry_ivl = 1200;

  err = lbmct_create(&s_ct, ctx1, &ct_config, "Meta s_ct", 10);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_create(&r_ct, ctx2, NULL, NULL, 0);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbmct_src_create(&ct_src, s_ct, "CtSimpleMessages1", NULL, test_src_cb,
    test_src_conn_create_cb, test_src_conn_delete_cb, src_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(10);

  /* Let's drop the crsp, but get it later during the retry.  Also, make
   * sure receiver times out first. */
  s_ct->active_config.test_bits |= LBMCT_TEST_BITS_NO_CRSP;
  err = lbmct_rcv_create(&ct_rcv, r_ct, "CtSimpleMessages1", NULL, test_rcv_cb,
    test_rcv_conn_create_cb, test_rcv_conn_delete_cb, rcv_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(100);
  s_ct->active_config.test_bits &= ~LBMCT_TEST_BITS_NO_CRSP;

  /* This message is sent before handshakes are done; not be delivered. */
  err = lbm_src_send_ex(lbmct_src_get_um_src(ct_src), "msg0", 5, LBM_MSG_FLUSH,
    &send_ex_info);
  ASSERT_EQ(0, err) << lbm_errmsg();

  /* Wait for src and rcv connect events. */
  PRT_SEM_WAIT(sync_sem);  sync_sem_cnt --;
  PRT_SEM_WAIT(sync_sem);  sync_sem_cnt --;
  ASSERT_EQ(0, sync_sem_cnt);

  ASSERT_NE(null_ptr, strstr(log_buffer[0], "test_src_cb, clientd='SrcClientd', event=1"));
  ASSERT_NE(null_ptr, strstr(log_buffer[1], "LBMCT_TEST_BITS_NO_CRSP"));
  ASSERT_NE(null_ptr, strstr(log_buffer[2], "test_rcv_conn_create_cb, clientd='RcvClientd', peer:  status=0, flags=0x3f, src_metadata='Meta s_ct', rcv_metadata=none, rcv_source_name='TCP:"));
  ASSERT_NE(null_ptr, strstr(log_buffer[3], "test_src_conn_create_cb, clientd='SrcClientd', peer:  status=0, flags=0x2f, src_metadata='Meta s_ct', rcv_metadata=none, no rcv_source_name,"));
  ASSERT_EQ(4, log_cnt);

  ASSERT_NE(null_ptr, strstr(msg_buffer[0], "test_rcv_cb: type=0, sqn=1, source='TCP:"));
  ASSERT_NE(null_ptr, strstr(msg_buffer[0], "clientd='RcvClientd', source_clientd='RcvConnClientd', data='CRSP,"));
  ASSERT_EQ(1, msg_cnt);

  err = lbm_src_send_ex(lbmct_src_get_um_src(ct_src), "msg1", 5, LBM_MSG_FLUSH,
    &send_ex_info);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(10);
  ASSERT_EQ(4, log_cnt);

  ASSERT_NE(null_ptr, strstr(msg_buffer[1], "test_rcv_cb: type=0, sqn=2, source='TCP:"));
  ASSERT_NE(null_ptr, strstr(msg_buffer[1], "clientd='RcvClientd', source_clientd='RcvConnClientd', data='msg1', props=TstProp:9876."));
  ASSERT_EQ(2, msg_cnt);

  /* Drop the D_OK so that the DRSP has to retry. */
  r_ct->active_config.test_bits |= LBMCT_TEST_BITS_NO_D_OK;
  err = lbmct_src_delete(ct_src);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(200);
  r_ct->active_config.test_bits &= ~LBMCT_TEST_BITS_NO_D_OK;
  ASSERT_NE(null_ptr, strstr(log_buffer[4], "test_rcv_conn_delete_cb, clientd='RcvClientd', conn_clientd='RcvConnClientd', peer:  status=0, flags=0x7f, src_metadata='Meta s_ct', rcv_metadata=none, rcv_source_name='TCP:"));
  ASSERT_NE(null_ptr, strstr(log_buffer[5], "LBMCT_TEST_BITS_NO_D_OK"));
  ASSERT_EQ(6, log_cnt);

  SLEEP_MSEC(1100);  /* DRSP retries. */

  ASSERT_NE(null_ptr, strstr(log_buffer[6], "test_src_conn_delete_cb, clientd='SrcClientd', conn_clientd='SrcConnClientd', peer:  status=0, flags=0x6f, src_metadata='Meta s_ct', rcv_metadata=none, no rcv_source_name,"));
  ASSERT_EQ(7, log_cnt);

  ASSERT_NE(null_ptr, strstr(msg_buffer[2], "test_rcv_cb: type=0, sqn=3, source='TCP:"));
  ASSERT_NE(null_ptr, strstr(msg_buffer[2], "clientd='RcvClientd', source_clientd='RcvConnClientd', data='DRSP,"));
  ASSERT_EQ(3, msg_cnt);

  err = lbmct_rcv_delete(ct_rcv);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(200);

  err = lbmct_delete(s_ct);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_delete(r_ct);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbm_msg_properties_delete(msg_props);
  ASSERT_EQ(0, err) << lbm_errmsg();

  ASSERT_EQ(7, log_cnt);
  ASSERT_EQ(3, msg_cnt);
}  /* CtSimpleMessages1 */


/* Invert the order of creates/deletes. */
TEST(Ct,CtSimpleMessages2) {
  lbmct_t *s_ct = NULL;
  lbmct_t *r_ct = NULL;
  lbmct_src_t *ct_src;
  lbmct_rcv_t *ct_rcv;
  lbmct_config_t ct_config;
  int err;

  signal(SIGPIPE, SIG_IGN);
  SLEEP_MSEC(100);
  log_cnt = 0;
  msg_cnt = 0;
  MSEC_CLOCK(test_start_time);
  ASSERT_EQ(0, sync_sem_cnt);

  /* When they want to race, make receiver timeout before source. */
  ct_config.flags = LBMCT_CT_CONFIG_FLAGS_RETRY_IVL;
  ct_config.retry_ivl = 1300;

  err = lbmct_create(&s_ct, ctx1, &ct_config, NULL, 0);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_create(&r_ct, ctx2, NULL, "Meta r_ct", 10);
  ASSERT_EQ(0, err) << lbm_errmsg();

  err = lbmct_rcv_create(&ct_rcv, r_ct, "CtSimpleMessages2", NULL, test_rcv_cb,
    test_rcv_conn_create_cb, test_rcv_conn_delete_cb, rcv_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(10);

  /* Let's drop the creq (but get it later during the retry). */
  r_ct->active_config.test_bits |= LBMCT_TEST_BITS_NO_CREQ;
  err = lbmct_src_create(&ct_src, s_ct, "CtSimpleMessages2", NULL, NULL,
    test_src_conn_create_cb, test_src_conn_delete_cb, src_clientd);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(100);
  r_ct->active_config.test_bits &= ~LBMCT_TEST_BITS_NO_CREQ;

  /* This message should not be delivered. */
  err = lbm_src_send(lbmct_src_get_um_src(ct_src), "msg0", 5, LBM_MSG_FLUSH);
  ASSERT_EQ(0, err) << lbm_errmsg();

  ASSERT_NE(null_ptr, strstr(log_buffer[0], "LBMCT_TEST_BITS_NO_CREQ"));
  ASSERT_EQ(1, log_cnt);  /* Connection not created yet; needs retry. */

  /* Wait for src and rcv connect events. */
  PRT_SEM_WAIT(sync_sem);  sync_sem_cnt --;
  PRT_SEM_WAIT(sync_sem);  sync_sem_cnt --;
  ASSERT_EQ(0, sync_sem_cnt);
  ASSERT_NE(null_ptr, strstr(log_buffer[1], "test_rcv_conn_create_cb, clientd='RcvClientd', peer:  status=0, flags=0x3f, src_metadata=none, rcv_metadata='Meta r_ct', rcv_source_name='TCP:"));
  ASSERT_NE(null_ptr, strstr(log_buffer[2], "test_src_conn_create_cb, clientd='SrcClientd', peer:  status=0, flags=0x2f, src_metadata=none, rcv_metadata='Meta r_ct', no rcv_source_name,"));
  ASSERT_EQ(3, log_cnt);

  ASSERT_NE(null_ptr, strstr(msg_buffer[0], "test_rcv_cb: type=0, sqn=1, source='TCP:"));
  ASSERT_NE(null_ptr, strstr(msg_buffer[0], "clientd='RcvClientd', source_clientd='RcvConnClientd', data='CRSP,"));
  ASSERT_EQ(1, msg_cnt);

  err = lbm_src_send(lbmct_src_get_um_src(ct_src), "msg1", 5, LBM_MSG_FLUSH);
  ASSERT_EQ(0, err) << lbm_errmsg();
  SLEEP_MSEC(10);
  ASSERT_EQ(3, log_cnt);

  ASSERT_NE(null_ptr, strstr(msg_buffer[1], "test_rcv_cb: type=0, sqn=2, source='TCP:"));
  ASSERT_NE(null_ptr, strstr(msg_buffer[1], "properties=(nil), clientd='RcvClientd', source_clientd='RcvConnClientd', data='msg1'"));
  ASSERT_EQ(2, msg_cnt);

  /* Make sure that receiving a message in "ENDING" state still delivers it.
   * (Also tests retry of DREQ.)
   */
  r_ct->active_config.test_bits |= LBMCT_TEST_BITS_NO_DREQ;

  err = lbmct_rcv_delete(ct_rcv);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(20);  /* Give dreq a chance to be sent (and dropped). */
  err = lbm_src_send(lbmct_src_get_um_src(ct_src), "msg2", 5, LBM_MSG_FLUSH);
  ASSERT_EQ(0, err) << lbm_errmsg();

  r_ct->active_config.test_bits &= ~LBMCT_TEST_BITS_NO_DREQ;

  SLEEP_MSEC(1000);
  ASSERT_NE(null_ptr, strstr(log_buffer[3], "LBMCT_TEST_BITS_NO_DREQ"));
  ASSERT_NE(null_ptr, strstr(log_buffer[4], "test_rcv_conn_delete_cb, clientd='RcvClientd', conn_clientd='RcvConnClientd', peer:  status=0, flags=0x7f, src_metadata=none, rcv_metadata='Meta r_ct"));
  ASSERT_NE(null_ptr, strstr(log_buffer[5], "test_src_conn_delete_cb, clientd='SrcClientd', conn_clientd='SrcConnClientd', peer:  status=0, flags=0x6f, src_metadata=none, rcv_metadata='Meta r_ct"));
  ASSERT_EQ(6, log_cnt);

  ASSERT_NE(null_ptr, strstr(msg_buffer[2], "test_rcv_cb: type=0, sqn=3, source='TCP:"));
  ASSERT_NE(null_ptr, strstr(msg_buffer[2], "properties=(nil), clientd='RcvClientd', source_clientd='RcvConnClientd', data='msg2'"));
  ASSERT_NE(null_ptr, strstr(msg_buffer[3], "test_rcv_cb: type=0, sqn=4, source='TCP:"));
  ASSERT_NE(null_ptr, strstr(msg_buffer[3], "clientd='RcvClientd', source_clientd='RcvConnClientd', data='DRSP,"));
  ASSERT_EQ(4, msg_cnt);

  err = lbmct_src_delete(ct_src);
  ASSERT_EQ(0, err) << lbm_errmsg();

  SLEEP_MSEC(200);

  err = lbmct_delete(s_ct);
  ASSERT_EQ(0, err) << lbm_errmsg();
  err = lbmct_delete(r_ct);
  ASSERT_EQ(0, err) << lbm_errmsg();

  ASSERT_EQ(6, log_cnt);
  ASSERT_EQ(4, msg_cnt);
}  /* CtSimpleMessages2 */


int main(int argc, char **argv) {
  lbm_context_attr_t *ctx_attr;
  const char *cfg1, *cfg2;
  int err;

  ::testing::InitGoogleTest(&argc, argv);

  if (argc == 1) {
    cfg1 = "test.cfg";
    cfg2 = cfg1;
  }
  else if (argc == 2) {
    cfg1 = argv[1];
    cfg2 = cfg1;
  }
  else if (argc == 3) {
    cfg1 = argv[1];
    cfg2 = argv[2];
  }
  else {
    printf("unrecognized command line\n");
    exit(1);
  }

  /* Not sure why I need to do this with MacOS.  Not sure why some of the
   * tests generates a sigpipe on Mac but not Linux.
   */
  signal(SIGPIPE, SIG_IGN);

  PRT_MUTEX_INIT(log_lock);
  PRT_MUTEX_INIT(msg_lock);

  MSEC_CLOCK(test_start_time);
  /* Set up UM error logger callback. */
  err = lbm_log(test_log_cb, NULL);
  LBM_ERR(err);

  err = lbm_config(cfg1);
  LBM_ERR(err);

  /* Create a pair of contexts, one for publisher, the other for subscriber. */

  /* On context request ports, disable Nagle's alg (turn on NODELAY).  Also
   * turn on MIM to verify that a MIM messsge doesn't cause a problem.
   */
  err = lbm_context_attr_create(&ctx_attr);
  LBM_ERR(err);
  err = lbm_context_attr_str_setopt(ctx_attr,
    "response_tcp_nodelay", "1");
  LBM_ERR(err);

  err = lbm_context_create(&ctx1, ctx_attr, NULL, NULL);
  LBM_ERR(err);

  err = lbm_context_attr_delete(ctx_attr);
  LBM_ERR(err);

  /* If different configs, assume DRO. */
  if (cfg1 != cfg2) {
    SLEEP_MSEC(1500);  /* Let ctx1 discover its domain ID. */

    err = lbm_config(cfg2);
    LBM_ERR(err);
  }

  err = lbm_context_attr_create(&ctx_attr);
  LBM_ERR(err);
  err = lbm_context_attr_str_setopt(ctx_attr,
    "response_tcp_nodelay", "1");
  LBM_ERR(err);

  err = lbm_context_create(&ctx2, ctx_attr, NULL, NULL);
  LBM_ERR(err);

  /* If different configs, assume DRO. */
  if (cfg1 != cfg2) {
    SLEEP_MSEC(1500);  /* Let ctx2 discover its domain ID. */
  }

  err = lbm_context_attr_delete(ctx_attr);
  LBM_ERR(err);

  num_domain_ids = 2;  /* Freeze domain ID discovery. */

  /* Start MIM session going in ctx2. */
  /*err = lbm_multicast_immediate_message(ctx2,
   *  "ct_test", "0", 2, LBM_MSG_FLUSH);
   *LBM_ERR(err);
   */

  PRT_SEM_INIT(sync_sem, 0);  /* Init count to 0. */
  sync_sem_cnt = 0;

  err = RUN_ALL_TESTS();

  PRT_SEM_DELETE(sync_sem);

  (void)lbm_context_delete(ctx1);
  (void)lbm_context_delete(ctx2);

  return 0;
}  /* main */
