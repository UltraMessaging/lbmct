/* lbmct_private.h - internal definitions for Connected Topics.  Not for
 * application use.
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

#ifndef LBMCT_PRIVATE_H
#define LBMCT_PRIVATE_H

/* This include file uses definitions from these include files. */

#include <signal.h>
#include "lbmct.h"
#include "tmr.h"

/* Make use of some internal UM APIs which are not officially part of our
 * public API.
 */
#include "lbm_internal.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* The following 2 macros manage a simple linked list.  The first parameter
 * is the list head pointer.  The second parameter is a node instance.  The
 * third parameter is a link name prefix.  The node instance is assumed to be
 * a structure pointer that has fields named <link name prefix>_prev and
 * <link name prefix>_next.
 * THESE MACROS ARE NOT THREAD SAFE!
 */

/* Add a node to a linked list. */
#define LBMCT_LIST_ADD(_head, _node, _prefix) do {\
  ASSRT(_node->_prefix ## _next == NULL);\
  (_node)->_prefix ## _prev = NULL;\
  (_node)->_prefix ## _next = (_head);\
  if ((_head) != NULL)\
    (_head)->_prefix ## _prev = (_node);\
  (_head) = (_node);\
} while (0);

/* Delete a node from a linked list. */
#define LBMCT_LIST_DEL(_head, _node, _prefix) do {\
  if (_node->_prefix ## _next != NULL)\
    _node->_prefix ## _next->_prefix ## _prev = _node->_prefix ## _prev;\
  if (_node->_prefix ## _prev != NULL)\
    _node->_prefix ## _prev->_prefix ## _next = _node->_prefix ## _next;\
  else _head = _node->_prefix ## _next;\
  _node->_prefix ## _next = NULL;\
  _node->_prefix ## _prev = NULL;\
} while (0);


/* Helper macro to give a better error description when an object
 * signature is bad.
 */
#define E_BAD_SIG(_o) (/*abort()???,*/ ((_o)->sig == LBMCT_SIG_DEAD) ?\
  ("Bad access to deleted " #_o) :\
  ("Bad access to corrupted " #_o)\
)


/* Macro to make it easier to use sscanf().  See:
 *   https://stackoverflow.com/questions/25410690
 */
#define STRDEF2(_s) #_s
#define STRDEF(_s) STRDEF2(_s)


/* Handshake message formats.  All messages are null-terminated ASCII strings.
 * For handshae messages that contain user metadata, the metadata is appended
 * after the message string's null, and is in binary form.  Note that since
 * the ct system does not understand the internal format of the metadata,
 * the binary representation is not normalized according to the host endian
 * architecture.  It is the application's responsibility to perform any
 * needed conversions.
 *
 * Also note that the second field in each message is an integer which
 * tells the number of comma-separated fields are in the message.  This
 * refers to only the ASCII string part of the message.  If the metadata is
 * appended, the field count does not include it.  Also, note that the
 * topic string itself might contain commas (user chooses the topic name).
 * The field count can be used to prevent the message parser from
 * interpreting a topic string with commas as being multiple fields.
 */


/* Worst-case UIM address string size:  (See lbmct_ctx_uim_addr_t)
 * TCP:dddddddddd:iii.iii.iii.iii:ppppp
 * 123456789012345678901234567890123456 36+nul+3pad=40 (5*8)
 * where d=domain ID, i=IP addr, p=port
 * (Each field can be smaller than worst-case.)
 */
#define LBMCT_UIM_ADDR_STR_SZ 40

/* Worst-case context ID string size:  (See lbmct_ctx_id_t)
 * rrrrrrrrrr,TCP:dddddddddd:iii.iii.iii.iii:ppppp
 * 12345678901234567890123456789012345678901234567 47+nul=48 (6*8)
 * where r=random num, d=domain ID, i=IP addr, p=port
 * (Each field can be smaller than worst-case.)
 */
#define LBMCT_CTX_ID_STR_SZ 48

/* Worst-case connection endpoint ID size:
 * rrrrrrrrrr,dddddddddd:iii.iii.iii.iii:ppppp,tttttttttt
 * 123456789012345678901234567890123456789012345678901234 54+nul+1pad=56 (7*8)
 * where r=random num, d=domain ID, i=IP addr, p=port, t=topic ID.
 * (Each field can be smaller than worst-case.)
 */
#define LBMCT_RCV_CONN_ID_STR_SZ 56

/* Worst-case conn ID string size:
 * rrrrrrrrrr,dddddddddd:iii.iii.iii.iii:ppppp,tttttttttt,s...
 * 1234567890123456789012345678901234567890123456789012345 55+256+nul=312 (39*8)
 * where r=random num, d=domain ID, i=IP addr, p=port, t=topic ID, s=topic str.
 * (Each field can be smaller than worst-case.)
 */
#define LBMCT_CONN_ID_STR_SZ 304


/* Handshake messages have a 4-byte ASCII message "type" field. */
#define LBMCT_PREFIX_SZ 4

/* Connect request, sent by receive-side to source-side as UIM
 *   CREQ,field_count,rcv_ct_id,rcv_uim_addr,rcv_conn_id,topic_str<nul>
 *     CREQ = literal string.
 *     field_count = number of comma-separate fields in this message (6).
 * (The following 4 fields compose a connection ID)
 *     rcv_ct_id = random number for this instance of the ct layer.
 *     rcv_uim_addr = domain_id:ip_addr:port to send a UIM to the receiver
 *                    (used mostly to further identify the "ct" instance).
 *     rcv_conn_id = integer assigned to the receive-side of the new connection.
 *     topic_str = topic string.
 */
#define LBMCT_CREQ_MSG_PREFIX "CREQ"
#define LBMCT_CREQ_MSG_SZ (LBMCT_PREFIX_SZ+1+10+1+LBMCT_CONN_ID_STR_SZ+1)

/* Connect response, sent by source-side to receive-side on the data transport
 *   CRSP,field_count,rcv_ct_id,rcv_uim_addr,rcv_ct_id,src_ct_id,src_uim_addr,src_conn_id,meta_len<nul>metadata
 * The metadata is pure binary (not C string) and is meta_len bytes.
 * (See CREQ for field definitions, substituting src/rcv as appropriate.)
 */
#define LBMCT_CRSP_MSG_PREFIX "CRSP"
#define LBMCT_CRSP_MSG_BASE_SZ (LBMCT_PREFIX_SZ+1+10+1+\
  2*LBMCT_CONN_ID_STR_SZ+1+10+1)

/* Connect OK, send from receive-side to source-side as UIM
 *   C_OK,field_count,rcv_ct_id,rcv_uim_addr,rcv_ct_id,src_ct_id,src_uim_addr,src_conn_id,start_sqn,meta_len<nul>metadata
 * The metadata is pure binary (not C string) and is meta_len bytes.
 * Same format as CRSP except for command string and the addition of the
 * receiver's start sequence number (of the CRSP handshake).
 */
#define LBMCT_C_OK_MSG_PREFIX "C_OK"
#define LBMCT_C_OK_MSG_BASE_SZ (LBMCT_PREFIX_SZ+1+10+1+\
  2*LBMCT_CONN_ID_STR_SZ+1+10+1+10+1)

/* Disconnect request, sent by receive-side to source-side as UIM
 *   DREQ,field_count,rcv_ct_id,rcv_uim_addr,rcv_ct_id,src_ct_id,src_uim_addr,src_conn_id<nul>
 */
#define LBMCT_DREQ_MSG_PREFIX "DREQ"
#define LBMCT_DREQ_MSG_SZ (LBMCT_PREFIX_SZ+1+10+1+\
  2*LBMCT_CONN_ID_STR_SZ+1)

/* Disconnect response, sent by source-side to receive-side on the data transport
 *   DRSP,field_count,rcv_ct_id,rcv_uim_addr,rcv_ct_id,src_ct_id,src_uim_addr,src_conn_id<nul>
 */
#define LBMCT_DRSP_MSG_PREFIX "DRSP"
#define LBMCT_DRSP_MSG_SZ (LBMCT_PREFIX_SZ+1+10+1+\
  2*LBMCT_CONN_ID_STR_SZ+1)

/* Disconnect OK, sent by receiver-side to source-side as UIM
 *   DRSP,field_count,rcv_ct_id,rcv_uim_addr,rcv_ct_id,src_ct_id,src_uim_addr,src_conn_id,end_sqn<nul>
 */
#define LBMCT_D_OK_MSG_PREFIX "D_OK"
#define LBMCT_D_OK_MSG_SZ (LBMCT_PREFIX_SZ+1+10+1+\
  2*LBMCT_CONN_ID_STR_SZ+1+10+1)


/* Some common data structures. */

/* A context UIM address, in proper string form, is passed as a target to
 * lbm_unicast_immediate_message().
 * See LBMCT_UIM_ADDR_STR_SZ
 */
typedef struct lbmct_ctx_uim_addr_t_stct {
  lbm_uint32_t domain_id;  /* host order */
  lbm_uint_t ip_addr;  /* binary */
  lbm_uint16_t port;  /* host order */
} lbmct_ctx_uim_addr_t;

/* A context ID uniquely identifies a context in a UM network across space
 * and time.
 * See LBMCT_CTX_ID_STR_SZ
 */
typedef struct lbmct_ctx_id_t_stct {
  lbm_uint32_t rand_num;  /* host order */
  lbmct_ctx_uim_addr_t uim_addr;
} lbmct_ctx_id_t;


/* Signature values for various (long-lived) structures.  These values are
 * placed in the corresponding structure's "sig" field at the end of the
 * structures, and are checked elsewhere in the code to make sure the
 * structure is valid.  Prior to being freed, the "sig" field is written
 * with the "DEAD" value.  This catches bugs like double-freeing, or accessing
 * a freed structure.
 */
#define LBMCT_SIG_CT 0x01010101  /* 16,843,009 */
#define LBMCT_SIG_CT_RCV 0x02020202  /* 33,686,018 */
#define LBMCT_SIG_CT_SRC 0x03030303  /* 50,529,027 */
#define LBMCT_SIG_CTRLR_CMD 0x04040404  /* 67,372,036 */
#define LBMCT_SIG_SRC_CONN 0x05050505  /* 84,215,045 */
#define LBMCT_SIG_RCV_CONN 0x06060606  /* 101,058,054 */
#define LBMCT_SIG_DEAD 0xdeaddead  /* 3,735,936,685 */

/* I set all malloced structures to 0x5a as a recognizable uninitialized
 * pattern.  0x5a5a5a5a=1,515,870,810; 0x5a5a=23,130; 0x5a=90.
 */

enum lbmct_conn_state {
  LBMCT_CONN_STATE_NONE = 1,
  LBMCT_CONN_STATE_PRE_CREATED,
  LBMCT_CONN_STATE_STARTING,
  LBMCT_CONN_STATE_RUNNING,
  LBMCT_CONN_STATE_ENDING,
  LBMCT_CONN_STATE_TIME_WAIT
};

enum lbmct_ctrlr_state {
  LBMCT_CTRLR_STATE_NONE = 1,
  LBMCT_CTRLR_STATE_STARTING,
  LBMCT_CTRLR_STATE_RUNNING,
  LBMCT_CTRLR_STATE_EXITING
};


/* CT handshake topic: "LbmCt.h": 7+nul=8
 */
#define LBMCT_HANDSHAKE_TOPIC_STR "LbmCt.h"
#define LBMCT_HANDSHAKE_TOPIC_STR_SZ sizeof(LBMCT_HANDSHAKE_TOPIC_STR)


/******************************************************************************/
/* Definitions for ct controller command interface. */

typedef struct lbmct_ctrlr_cmd_t_stct lbmct_ctrlr_cmd_t;  /* Forward def. */

/* When a command is done, it calls a completion callback. */
typedef int (*lbmct_ctrlr_cmd_complete_cb_func) (lbmct_ctrlr_cmd_t *cmd);

#define LBMCT_CTRLR_NUM_CMD_NODES 32
enum lbmct_ctrlr_cmd_type {
  LBMCT_CTRLR_CMD_TYPE_TEST = 1,
  LBMCT_CTRLR_CMD_TYPE_QUIT,
  LBMCT_CTRLR_CMD_TYPE_SRC_CONN_TICK,
  LBMCT_CTRLR_CMD_TYPE_CT_SRC_CREATE,
  LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE,
  LBMCT_CTRLR_CMD_TYPE_CT_SRC_DELETE,
  LBMCT_CTRLR_CMD_TYPE_RCV_CONN_TICK,
  LBMCT_CTRLR_CMD_TYPE_CT_RCV_CREATE,
  LBMCT_CTRLR_CMD_TYPE_RCV_CONN_CREATE,
  LBMCT_CTRLR_CMD_TYPE_CT_RCV_DELETE,
  LBMCT_CTRLR_CMD_TYPE_RCV_CONN_DELETE,
  LBMCT_CTRLR_CMD_TYPE_RCV_SEND_C_OK,
  LBMCT_CTRLR_CMD_TYPE_RCV_SEND_D_OK,
};

enum lbmct_ctrlr_cmd_disposition {
  LBMCT_CTRLR_CMD_NEEDS_FREE,  /* Cmd should be freed by ct ctrlr. */
  LBMCT_CTRLR_CMD_NO_FREE  /* Cmd should *not* be freed by ct ctrlr. */
};

/* Command structure for communicating with the ct_ctrlr thread.  The
 * "cmd_data" void pointer points at one of the lbmct_ctrlr_cmd_* structs.
 */
struct lbmct_ctrlr_cmd_t_stct {
  enum lbmct_ctrlr_cmd_type cmd_type;
  void *cmd_data;  /* points at command-type-specific structures (above). */
  enum lbmct_ctrlr_cmd_disposition cmd_disposition;  /* Who freees cmd_data. */
  int cmd_err;
  char *cmd_errmsg;
  lbmct_t *ct;
  lbmct_ctrlr_cmd_complete_cb_func complete_cb;
  prt_sem_t complete_sem;
  lbm_uint32_t sig;  /* LBMCT_SIG_CTRLR_CMD */
};

/*
 * Structures specific to each ct_ctrlr thread command type.
 */

/* Commands in lbmct.c */

/* Used for unit tests only. */
typedef struct {
  /* Input/output Field. */
  char test_str[80];
  /* Input field. */
  int test_err;
} lbmct_ctrlr_cmd_test_t;

/* Triggered from application call to lbmct_delete(). */
typedef struct {
  int dummy;
} lbmct_ctrlr_cmd_quit_t;


/* Commands in lbmct_src.c */

/* Triggered from timer callback. */
typedef struct {
  lbmct_src_conn_t *src_conn;
  int tmr_id;
} lbmct_ctrlr_cmd_src_conn_tick_t;


/* Triggered from application call to lbmct_src_create(). */
typedef struct {
  /* Output fields */
  lbmct_src_t *ct_src;
  /* Input fields */
  const char *topic_str;
  lbm_src_topic_attr_t *src_attr;
  lbm_src_cb_proc src_cb;
  lbmct_src_conn_create_function_cb src_conn_create_cb;
  lbmct_src_conn_delete_function_cb src_conn_delete_cb;
  void *src_clientd;
} lbmct_ctrlr_cmd_ct_src_create_t;

/* Triggered when source side receives a handshake message from rcv. */
typedef struct {
  lbm_msg_t *msg;  /* "Retained" message. */
} lbmct_ctrlr_cmd_src_handshake_t;

/* Triggered from application call to lbmct_src_delete(). */
typedef struct {
  /* Output fields (none) */
  /* Input fields */
  lbmct_src_t *ct_src;
} lbmct_ctrlr_cmd_ct_src_delete_t;


/* Commands in lbmct_rcv.c */

/* Triggered from timer callback. */
typedef struct {
  lbmct_rcv_conn_t *rcv_conn;
  int tmr_id;
} lbmct_ctrlr_cmd_rcv_conn_tick_t;

/* Triggered from application call to lbmct_rcv_create(). */
typedef struct {
  /* Output fields */
  lbmct_rcv_t *ct_rcv;
  /* Input fields */
  const char *topic_str;
  lbm_rcv_topic_attr_t *rcv_attr;
  lbm_rcv_cb_proc rcv_cb;
  lbmct_rcv_conn_create_function_cb rcv_conn_create_cb;
  lbmct_rcv_conn_delete_function_cb rcv_conn_delete_cb;
  void *rcv_clientd;
} lbmct_ctrlr_cmd_ct_rcv_create_t;

/* Triggered from per-source clientd create callback. */
typedef struct {
  /* Output fields (none) */
  /* Input fields */
  lbmct_rcv_conn_t *rcv_conn;  /* caller mallocs the segment. */
  char source_name[LBM_MSG_MAX_SOURCE_LEN+1];
  lbmct_rcv_t *ct_rcv;
} lbmct_ctrlr_cmd_rcv_conn_create_t;

/* Triggered from application call to lbmct_rcv_delete(). */
typedef struct {
  /* Output fields (none) */
  /* Input fields */
  lbmct_rcv_t *ct_rcv;
} lbmct_ctrlr_cmd_ct_rcv_delete_t;

/* Triggered from per-source clientd delete callback (DC delete). */
typedef struct {
  /* Output fields (none) */
  /* Input fields */
  lbmct_rcv_conn_t *rcv_conn;
} lbmct_ctrlr_cmd_rcv_conn_delete_t;

/* Triggered from receive code (running in context thread) to send handshake. */
typedef struct {
  /* Output fields (none) */
  /* Input fields */
  lbmct_rcv_conn_t *rcv_conn;
} lbmct_ctrlr_cmd_rcv_send_c_ok_t;

/* Triggered from receive code (running in context thread) to send handshake. */
typedef struct {
  /* Output fields (none) */
  /* Input fields */
  lbmct_rcv_conn_t *rcv_conn;
} lbmct_ctrlr_cmd_rcv_send_d_ok_t;


/*******************************************************************************
 * Structures for primary objects.
 */

#define LBMCT_MAX_RECENT_EVENTS 256

/* This is the Connected Topic controller.  Create one per context. */
struct lbmct_t_stct {
  lbm_uint32_t ct_id;  /* Random number to avoid exit/restart confusion. */
  lbmct_ctx_uim_addr_t local_uim_addr;
  lbm_context_t *ctx;
  lbmct_config_t user_config;
  lbmct_config_t active_config;  /* "flags" field not used. */
  char *metadata;
  size_t metadata_len;
  lbmct_src_t *src_list_head;  /* List of ct sources */
  lbmct_rcv_t *rcv_list_head;  /* List of ct receivers */
  lbm_rcv_t *um_handshake_rcv;  /* UM receiver for peer handshake messages. */
  lbm_uint32_t next_conn_id;  /* Receive-side connection ID for new conn. */
  mul_asl_t *ct_src_asl;
  mul_asl_t *src_conn_asl;

  lbm_tl_queue_t *ctrlr_cmd_free_tlq;  /* Available command structures. */
  lbm_tl_queue_t *ctrlr_cmd_work_tlq;  /* Work queue for ctrlr thread. */
  prt_thread_t ctrlr_thread_id;  /* Thread Id for ctrlr thread. */
  enum lbmct_ctrlr_state ctrlr_state;  /* State of controller. */
  lbm_uint32_t recent_events[LBMCT_MAX_RECENT_EVENTS];
  lbm_uint32_t num_recent_events;

  /* The source-side uses a message prop when sending handshake msgs so
   * that the receiver can differentiate between ct handshake and user msgs.
   */
  lbm_msg_properties_t *msg_props;
  lbm_src_send_ex_info_t send_ex_info;

  lbm_uint32_t sig;  /* LBMCT_SIG_CT */
};  /* lbmct_t_stct */

/* Connected Topic source object. */
struct lbmct_src_t_stct {
  lbmct_src_t *src_list_prev;
  lbmct_src_t *src_list_next;
  lbmct_t *ct;
  mul_asl_node_t *ct_src_asl_node; /* node of this conn in ct->ct_src_asl */
  lbm_context_t *ctx;
  char topic_str[LBM_MSG_MAX_TOPIC_LEN+1];
  lbm_src_t *um_src;
  lbm_src_cb_proc app_src_cb;
  lbmct_src_conn_create_function_cb app_src_conn_create_cb;
  lbmct_src_conn_delete_function_cb app_src_conn_delete_cb;
  void *app_src_clientd;
  lbmct_src_conn_t *conn_list_head;
  int exiting;  /* 0=object is active, 1=object is exiting. */
  lbm_uint32_t sig;  /* LBMCT_SIG_CT_SRC */
};  /* lbmct_src_t_stct */

/* Connected Topic source-side connection object. */
struct lbmct_src_conn_t_stct {
    /* List of "src_conn"s for the ct_src.
     * See lbmct_src_t_stct::conn_list_head. */
  lbmct_src_conn_t *conn_list_prev;
  lbmct_src_conn_t *conn_list_next;
    /* Parent objects. */
  lbmct_t *ct;
  lbmct_src_t *ct_src;
    /* For retrying handshake control messages. */
  tmr_t *tmr;  /* Timer for connection-related timeouts. */
  int pending_tmr_id;  /* ID for a specific schedule instance. */
  int try_cnt;

  lbmct_peer_info_t peer_info;
  mul_asl_node_t *src_conn_asl_node; /* node of this conn in ct->src_conn_asl */
  char src_uim_addr[LBMCT_UIM_ADDR_STR_SZ+1];
  lbm_uint32_t src_conn_id;
  void *app_conn_clientd;  /* Per-connection clientd supplied by the app. */
  enum lbmct_conn_state state;
  int app_conn_create_called;
  int app_conn_delete_called;
  lbm_uint32_t rcv_ct_id;
  char rcv_uim_addr[LBMCT_UIM_ADDR_STR_SZ+1];
  char rcv_source_name[LBM_MSG_MAX_SOURCE_LEN+1];
  lbm_uint32_t rcv_conn_id;
  char rcv_conn_id_str[LBMCT_RCV_CONN_ID_STR_SZ+1];
  lbm_uint32_t sig;  /* LBMCT_SIG_SRC_CONN */
};  /* lbmct_src_conn_t_stct */

/* Connected Topic receiver object. */
struct lbmct_rcv_t_stct {
  lbmct_rcv_t *rcv_list_prev;
  lbmct_rcv_t *rcv_list_next;
  lbmct_t *ct;
  lbm_context_t *ctx;
  char topic_str[LBM_MSG_MAX_TOPIC_LEN+1];
  lbm_rcv_t *um_rcv;
  lbm_rcv_cb_proc app_rcv_cb;
  lbmct_rcv_conn_create_function_cb app_rcv_conn_create_cb;
  lbmct_rcv_conn_delete_function_cb app_rcv_conn_delete_cb;
  void *app_rcv_clientd;
  lbmct_rcv_conn_t *conn_list_head;
  int exiting;  /* 0=object is active, 1=object is exiting. */
  lbm_uint32_t sig;  /* LBMCT_SIG_CT_RCV */
};  /* lbmct_rcv_t_stct */

/* Connected Topic receive-side connection object. */
struct lbmct_rcv_conn_t_stct {
  prt_mutex_t conn_lock;
    /* List of "rcv_conn"s for the ct_rcv.
     * See lbmct_rcv_t_stct::conn_list_head. */
  lbmct_rcv_conn_t *conn_list_prev;  /* Connection list for receiver. */
  lbmct_rcv_conn_t *conn_list_next;  /* See lbmct_rcv_t_stct::conn_list_head. */
    /* Parent objects. */
  lbmct_t *ct;
  lbmct_rcv_t *ct_rcv;
    /* For retrying handshake control messages. */
  tmr_t *tmr;  /* Timer for connection-related timeouts. */
  int pending_tmr_id;  /* ID for a specific schedule instance. */
  int try_cnt;
  int curr_creq_timeout;   /* In ms. */

  lbmct_peer_info_t peer_info;
  char rcv_uim_addr[LBMCT_UIM_ADDR_STR_SZ+1];
  lbm_uint32_t rcv_conn_id;
  void *app_conn_clientd;  /* Per-connection clientd supplied by the app. */
  enum lbmct_conn_state state;
  int app_conn_create_called;
  int app_conn_delete_called;
  unsigned int src_ct_id;
  char src_uim_addr[LBMCT_UIM_ADDR_STR_SZ+1];
  unsigned int src_conn_id;
  char src_dest_addr[LBM_MSG_MAX_SOURCE_LEN+1];
  lbm_uint32_t sig;  /* LBMCT_SIG_RCV_CONN */
};  /* lbmct_rcv_conn_t_stct */


/* Prototypes for lbmct.c */
LBMCT_API int lbmct_ctx_uim_addr(lbm_context_t *ctx, lbmct_ctx_uim_addr_t *uim_addr,
  int domain_id);
LBMCT_API int lbmct_rcv_conn_create(lbmct_rcv_conn_t **rcv_conn, lbmct_rcv_t *ct_rcv);
LBMCT_API int lbmct_rcv_conn_delete(lbmct_rcv_conn_t *rcv_conn);
LBMCT_API int lbmct_ctrlr_cmd_submit_and_wait(lbmct_t *ct,
  enum lbmct_ctrlr_cmd_type cmd_type, void *cmd_data);
LBMCT_API int lbmct_ctrlr_cmd_submit_nowait(lbmct_t *ct,
  enum lbmct_ctrlr_cmd_type cmd_type, void *cmd_data,
  enum lbmct_ctrlr_cmd_disposition cmd_disposition);

/* Prototypes for lbmct_src.c */
LBMCT_API int lbmct_handshake_rcv_create(lbmct_t *ct);
LBMCT_API int lbmct_ctrlr_cmd_src_conn_tick(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd);
LBMCT_API int lbmct_ctrlr_cmd_ct_src_create(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd);
LBMCT_API int lbmct_ctrlr_cmd_src_handshake(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd);
LBMCT_API int lbmct_ctrlr_cmd_ct_src_delete(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd);

/* Prototypes for lbmct_rcv.c */
LBMCT_API int lbmct_ctrlr_cmd_rcv_conn_tick(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd);
LBMCT_API int lbmct_ctrlr_cmd_ct_rcv_create(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd);
LBMCT_API int lbmct_ctrlr_cmd_rcv_conn_create(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd);
LBMCT_API int lbmct_ctrlr_cmd_ct_rcv_delete(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd);
LBMCT_API int lbmct_ctrlr_cmd_rcv_conn_delete(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd);
LBMCT_API int lbmct_ctrlr_cmd_rcv_send_c_ok(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd);
LBMCT_API int lbmct_ctrlr_cmd_rcv_send_d_ok(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif  /* LBMCT_PRIVATE_H */
