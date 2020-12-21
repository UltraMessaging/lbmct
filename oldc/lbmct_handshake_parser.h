/* lbmct_handshake_parser.h - Connected Topics code wire message parsing
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

#define MAGIC 2064878592  /* 7b 13 8c 00  (random bytes from random.org) */
#define MSG_TYPE_NONE 0
#define MSG_TYPE_CREQ 1
#define MSG_TYPE_CRSP 2
#define MSG_TYPE_COK  3
#define MSG_TYPE_DREQ 4
#define MSG_TYPE_DRSP 5
#define MSG_TYPE_DOK  6
#define MSG_TYPE_DFIN 7

typedef struct lbmct_handshake_parser_s {
  sb_t *temp_str;  /* Temporary work area. */

  char *msg_metadata;
  int msg_metadata_alloc_size;
  int msg_metadata_len;

  int msg_type;  /* MSG_TYPE_... */
  int rcv_ct_id;
  int rcv_domain_id;
  int rcv_ip_addr;
  int rcv_request_port;
  int rcv_conn_id;
  char *topic_str;
  char *rcv_conn_key;
  int src_ct_id;
  int src_domain_id;
  int src_ip_addr;
  int src_request_port;
  int src_conn_id;
  int rcv_start_seq_num;
  int rcv_end_seq_num;

  lbm_uint32_t sig;  /* LBMCT_SIG_HANDSHAKE_PARSER */
} lbmct_handshake_parser_t;
