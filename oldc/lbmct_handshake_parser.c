/* lbmct_handshake_parser.c - Connected Topics code wire message parsing
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
#include "lbmct_handshake_parser.h"


/* Constructor. */
ERR_F lbmct_handshake_parser(lbmct_handshake_parser_t **rtn, int metadata_size)
{
  err_t *err;
  lbmct_handshake_parser_t *this = NULL;

  PRT_MALLOC_SET_N(this, lbmct_handshake_parser_t, 0x5a, 1);
  this->sig = LBMCT_SIG_HANDSHAKE_PARSER;

  /* Multi-use string work buffer. Start at an arbitrary size; during use it
   * can grow (realloc) as needed. */
  err = sb_constructor(&this->temp_str, 256);
  if (err) {
    free(this);
    return err_rethrow(__FILE__, __LINE__, err, err->code, NULL);
  }

  /* Remote metadata storage. Start at a size a 10% larger than local
   * metadata; during use it can grow (realloc) as needed. */
  this->msg_metadata_alloc_size = (11*metadata_size)/10;
  PRT_MALLOC_N(this->msg_metadata, char, this->msg_metadata_alloc_size);
  this->msg_metadata_len = 0;

  this->msg_type = MSG_TYPE_NONE;
  this->topic_str = NULL;
  this->rcv_conn_key = NULL;

  this->sig = LBMCT_SIG_HANDSHAKE_PARSER;
  *rtn = this;

  return ERR_OK;
}  /* lbmct_handshake_parser */


ERR_F lbmct_handshake_parser_clear(lbmct_handshake_parser_t *this)
{
  if (this->sig != LBMCT_SIG_HANDSHAKE_PARSER) {
    ERR_THROW(EINVAL, "lbmct_handshake_parser_clear: bad sig");
  }

  ERR(sb_set_len(this->temp_str, 0));  /* Clear work area. */
  this->msg_metadata_len = 0;
  this->msg_type = MSG_TYPE_NONE;
  this->rcv_ct_id = 0;
  this->rcv_domain_id = 0;
  this->rcv_ip_addr = 0;
  this->rcv_request_port = 0;
  this->rcv_conn_id = 0;
  if (this->topic_str != NULL) {
    free(this->topic_str);
    this->topic_str = NULL;
  }
  if (this->rcv_conn_key != NULL) {
    free(this->rcv_conn_key);
    this->rcv_conn_key = NULL;
  }
  this->src_ct_id = 0;
  this->src_domain_id = 0;
  this->src_ip_addr = 0;
  this->src_request_port = 0;
  this->src_conn_id = 0;

  return ERR_OK;
}  /* lbmct_handshake_parser_clear */


ERR_F lbmct_handshake_parser_dispose(lbmct_handshake_parser_t *this)
{
  if (this->sig != LBMCT_SIG_HANDSHAKE_PARSER) {
    ERR_THROW(EINVAL, "lbmct_handshake_parser_clear: bad sig");
  }

  if (this->topic_str != NULL) {
    free(this->topic_str);
    this->topic_str = NULL;
  }
  if (this->rcv_conn_key != NULL) {
    free(this->rcv_conn_key);
    this->rcv_conn_key = NULL;
  }

  PRT_VOL32(this->sig) = LBMCT_SIG_DEAD;
  free(this);

  return ERR_OK;
}  /* lbmct_handshake_parser_dispose */


/* Getters (for getting fields after a message is parsed). */
int lbmct_handshake_parser_get_rcv_ct_id(lbmct_handshake_parser_t *this)
  { return this->rcv_ct_id; }
int lbmct_handshake_parser_get_rcv_domain_id(lbmct_handshake_parser_t *this)
  { return this->rcv_domain_id; }
int lbmct_handshake_parser_get_rcv_ip_addr(lbmct_handshake_parser_t *this)
  { return this->rcv_ip_addr; }
int lbmct_handshake_parser_get_rcv_request_port(lbmct_handshake_parser_t *this)
  { return this->rcv_request_port; }
int lbmct_handshake_parser_get_rcv_conn_id(lbmct_handshake_parser_t *this)
  { return this->rcv_conn_id; }
char *lbmct_handshake_parser_get_rcv_conn_key(lbmct_handshake_parser_t *this)
  { return this->rcv_conn_key; }
char *lbmct_handshake_parser_get_topic_str(lbmct_handshake_parser_t *this)
  { return this->topic_str; }
int lbmct_handshake_parser_get_src_ct_id(lbmct_handshake_parser_t *this)
  { return this->src_ct_id; }
int lbmct_handshake_parser_get_src_domain_id(lbmct_handshake_parser_t *this)
  { return this->src_domain_id; }
int lbmct_handshake_parser_get_src_ip_addr(lbmct_handshake_parser_t *this)
  { return this->src_ip_addr; }
int lbmct_handshake_parser_get_src_request_port(lbmct_handshake_parser_t *this)
  { return this->src_request_port; }
int lbmct_handshake_parser_get_src_conn_id(lbmct_handshake_parser_t *this)
  { return this->src_conn_id; }
long lbmct_handshake_parser_get_rcv_start_seq_num(lbmct_handshake_parser_t *this)
  { return this->rcv_start_seq_num; }
long lbmct_handshake_parser_get_rcv_end_seq_num(lbmct_handshake_parser_t *this)
  { return this->rcv_end_seq_num; }
char *lbmct_handshake_parser_get_msg_metadata(lbmct_handshake_parser_t *this)
  { return this->msg_metadata; }


ERR_F msg_get32_int(int *out_int,  char *in_msg, int *in_msg_ofs, int in_msg_len)
{
  lbm_uint32_t rtn_int;

  if (*in_msg_ofs + 4 > in_msg_len) {
    ERR_THROW(EINVAL, "msg_get32_int: in_msg too short");
  }

  memmove((char *)&rtn_int, &in_msg[*in_msg_ofs], 4);
  *in_msg_ofs += 4;
  *out_int = ntohl(rtn_int);  /* swap bytes as necessary. */

  return ERR_OK;
}   /* msg_get32_int */


ERR_F msg_get16_int(int *out_int,  char *in_msg, int *in_msg_ofs, int in_msg_len)
{
  lbm_uint16_t rtn_int;

  if (*in_msg_ofs + 2 > in_msg_len) {
    ERR_THROW(EINVAL, "msg_get16_int: in_msg too short");
  }

  memmove((char *)&rtn_int, &in_msg[*in_msg_ofs], 2);
  *in_msg_ofs += 2;
  *out_int = ntohs(rtn_int);  /* swap bytes as necessary. */

  return ERR_OK;
}   /* msg_get16_int */


ERR_F msg_get8_int(int *out_int,  char *in_msg, int *in_msg_ofs, int in_msg_len)
{
  if (*in_msg_ofs + 1 > in_msg_len) {
    ERR_THROW(EINVAL, "msg_get8_int: in_msg too short");
  }

  *out_int = in_msg[*in_msg_ofs];
  *in_msg_ofs += 1;

  return ERR_OK;
}   /* msg_get8_int */


ERR_F msg_get_bytes(char *out_bytes,  int num_bytes, char *in_msg, int *in_msg_ofs, int in_msg_len)
{
  if (*in_msg_ofs + num_bytes > in_msg_len) {
    ERR_THROW(EINVAL, "msg_get_bytes: in_msg too short");
  }

  memmove(out_bytes, &in_msg[*in_msg_ofs], num_bytes);
  *in_msg_ofs += num_bytes;

  return ERR_OK;
}  /* msg_get_bytes */


ERR_F msg_put32_int(int in_int, char *out_msg, int *out_msg_len, int out_msg_alloc_size)
{
  lbm_uint32_t out_int = htonl(in_int);  /* swap bytes as necessary. */

  if (*out_msg_len + 4 > out_msg_alloc_size) {
    ERR_THROW(EINVAL, "msg_put32_int: out_msg buffer too small");
  }

  memmove(&out_msg[*out_msg_len], (char *)&out_int, 4);
  *out_msg_len += 4;

  return ERR_OK;
}  /* msg_put32_int */


ERR_F msg_put16_int(int in_int, char *out_msg, int *out_msg_len, int out_msg_alloc_size)
{
  lbm_uint16_t out_int = htons(in_int);  /* swap bytes as necessary. */

  if (*out_msg_len + 2 > out_msg_alloc_size) {
    ERR_THROW(EINVAL, "msg_put16_int: out_msg buffer too small");
  }

  memmove(&out_msg[*out_msg_len], (char *)&out_int, 2);
  *out_msg_len += 2;

  return ERR_OK;
}  /* msg_put16_int */


ERR_F msg_put8_int(int in_int, char *out_msg, int *out_msg_len, int out_msg_alloc_size)
{
  lbm_uint8_t out_int = in_int;

  if (*out_msg_len + 1 > out_msg_alloc_size) {
    ERR_THROW(EINVAL, "msg_put8_int: out_msg buffer too small");
  }

  out_msg[*out_msg_len] = out_int;
  *out_msg_len += 1;

  return ERR_OK;
}  /* msg_put8_int */


ERR_F msg_put_bytes(char *in_bytes, int num_bytes, char *out_msg, int *out_msg_len, int out_msg_alloc_size)
{
  if (*out_msg_len + num_bytes > out_msg_alloc_size) {
    ERR_THROW(EINVAL, "msg_put8_int: out_msg buffer too small");
  }

  memmove(&out_msg[*out_msg_len], in_bytes, num_bytes);
  *out_msg_len += num_bytes;

  return ERR_OK;
}  /* msg_put_bytes */


/* Append (to a stringbuilder object) the string components of a receive connection key. */
ERR_F sb_append_rcv_conn_key(lbmct_handshake_parser_t *this, sb_t *out_str)
{
  ERR(sb_append_int(this->temp_str, this->rcv_ct_id));
  ERR(sb_append_char(this->temp_str, ','));
  if (this->rcv_domain_id > -1) {
    ERR(sb_append_int(this->temp_str, this->rcv_domain_id));
    ERR(sb_append_char(this->temp_str, ':'));
  }
  ERR(sb_append_int(this->temp_str, (this->rcv_ip_addr >> 24) & 0xff));
  ERR(sb_append_char(this->temp_str, '.'));
  ERR(sb_append_int(this->temp_str, (this->rcv_ip_addr >> 16) & 0xff));
  ERR(sb_append_char(this->temp_str, '.'));
  ERR(sb_append_int(this->temp_str, (this->rcv_ip_addr >> 8) & 0xff));
  ERR(sb_append_char(this->temp_str, '.'));
  ERR(sb_append_int(this->temp_str, (this->rcv_ip_addr) & 0xff));
  ERR(sb_append_char(this->temp_str, ':'));
  ERR(sb_append_int(this->temp_str, this->rcv_request_port));
  ERR(sb_append_char(this->temp_str, ','));
  ERR(sb_append_int(this->temp_str, this->rcv_conn_id));

  return ERR_OK;
}  /* sb_append_rcv_conn_key */


ERR_F parse_creq(lbmct_handshake_parser_t *this,
      char *in_msg, int *in_msg_ofs, int in_msg_len)
{
  int topic_str_len;
  char *conn_key;

  ERR(msg_get32_int(&this->rcv_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->rcv_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->rcv_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->rcv_conn_id, in_msg, in_msg_ofs, in_msg_len));

  /* Get topic string length. */
  ERR(msg_get8_int(&topic_str_len, in_msg, in_msg_ofs, in_msg_len));
  topic_str_len &= 0xff;  /* Ignore char's sign. */

  /* Get topic string. */
  if (this->topic_str != NULL) {
    free(this->topic_str);
  }
  PRT_MALLOC_N(this->topic_str, char, topic_str_len+1);  /* Allow room for final null. */
  ERR(msg_get_bytes(this->topic_str, topic_str_len, in_msg, in_msg_ofs, in_msg_len));
  this->topic_str[topic_str_len] = '\0';  /* Null-terminate string. */

  /* Get the receiver's connection key string. */
  ERR(sb_set_len(this->temp_str, 0));  /* Clear work area. */
  ERR(sb_append_rcv_conn_key(this, this->temp_str));
  /* Copy string (deeply)to rcv_conn_key. */
  ERR(sb_str_ref(this->temp_str, &conn_key));
  if (this->rcv_conn_key != NULL) {
    free(this->rcv_conn_key);
  }
  this->rcv_conn_key = strdup(conn_key);

  /* Make sure there's no garbage at the end of the message. */
  if (in_msg_len != *in_msg_ofs) {
    ERR_THROW(EINVAL, "parse_creq: message too long");
  }

  return ERR_OK;
}  /* parse_creq */


ERR_F parse_crsp(lbmct_handshake_parser_t *this,
      char *in_msg, int *in_msg_ofs, int in_msg_len)
{
  char *conn_key;

  ERR(msg_get32_int(&this->rcv_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->rcv_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->rcv_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->rcv_conn_id, in_msg, in_msg_ofs, in_msg_len));

  /* No topic string. */
  if (this->topic_str != NULL) {
    free(this->topic_str);
  }
  this->topic_str =  NULL;

  /* Get the receiver's connection key string. */
  ERR(sb_set_len(this->temp_str, 0));  /* Clear work area. */
  ERR(sb_append_rcv_conn_key(this, this->temp_str));
  /* Copy string (deeply)to rcv_conn_key. */
  ERR(sb_str_ref(this->temp_str, &conn_key));
  if (this->rcv_conn_key != NULL) {
    free(this->rcv_conn_key);
  }
  this->rcv_conn_key = strdup(conn_key);

  ERR(msg_get32_int(&this->src_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->src_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->src_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->src_conn_id, in_msg, in_msg_ofs, in_msg_len));

  ERR(msg_get32_int(&this->msg_metadata_len, in_msg, in_msg_ofs, in_msg_len));
  if (this->msg_metadata_len > this->msg_metadata_alloc_size) {
    this->msg_metadata_alloc_size = (11 * this->msg_metadata_len) / 10;
    free(this->msg_metadata);
    PRT_MALLOC_N(this->msg_metadata, char, this->msg_metadata_alloc_size);
  }
  if (this->msg_metadata_len > 0) {
    ERR(msg_get_bytes(this->msg_metadata, this->msg_metadata_len, in_msg, in_msg_ofs, in_msg_len));
  }

  /* Make sure there's no garbage at the end of the message. */
  if (in_msg_len != *in_msg_ofs) {
    ERR_THROW(EINVAL, "parse_crsp: message too long");
  }

  return ERR_OK;
}  /* parse_crsp */


ERR_F parse_cok(lbmct_handshake_parser_t *this,
      char *in_msg, int *in_msg_ofs, int in_msg_len)
{
  char *conn_key;

  ERR(msg_get32_int(&this->rcv_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->rcv_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->rcv_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->rcv_conn_id, in_msg, in_msg_ofs, in_msg_len));

  /* No topic string. */
  if (this->topic_str != NULL) {
    free(this->topic_str);
  }
  this->topic_str =  NULL;

  /* Get the receiver's connection key string. */
  ERR(sb_set_len(this->temp_str, 0));  /* Clear work area. */
  ERR(sb_append_rcv_conn_key(this, this->temp_str));
  /* Copy string (deeply)to rcv_conn_key. */
  ERR(sb_str_ref(this->temp_str, &conn_key));
  if (this->rcv_conn_key != NULL) {
    free(this->rcv_conn_key);
  }
  this->rcv_conn_key = strdup(conn_key);

  ERR(msg_get32_int(&this->src_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->src_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->src_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->src_conn_id, in_msg, in_msg_ofs, in_msg_len));

  ERR(msg_get32_int(&this->rcv_start_seq_num, in_msg, in_msg_ofs, in_msg_len));

  ERR(msg_get32_int(&this->msg_metadata_len, in_msg, in_msg_ofs, in_msg_len));
  if (this->msg_metadata_len > this->msg_metadata_alloc_size) {
    this->msg_metadata_alloc_size = (11 * this->msg_metadata_len) / 10;
    free(this->msg_metadata);
    PRT_MALLOC_N(this->msg_metadata, char, this->msg_metadata_alloc_size);
  }
  if (this->msg_metadata_len > 0) {
    ERR(msg_get_bytes(this->msg_metadata, this->msg_metadata_len, in_msg, in_msg_ofs, in_msg_len));
  }

  /* Make sure there's no garbage at the end of the message. */
  if (in_msg_len != *in_msg_ofs) {
    ERR_THROW(EINVAL, "parse_cok: message too long");
  }

  return ERR_OK;
}  /* parse_cok */


ERR_F parse_dreq(lbmct_handshake_parser_t *this,
      char *in_msg, int *in_msg_ofs, int in_msg_len)
{
  char *conn_key;

  ERR(msg_get32_int(&this->rcv_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->rcv_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->rcv_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->rcv_conn_id, in_msg, in_msg_ofs, in_msg_len));

  /* No topic string. */
  if (this->topic_str != NULL) {
    free(this->topic_str);
  }
  this->topic_str =  NULL;

  /* Get the receiver's connection key string. */
  ERR(sb_set_len(this->temp_str, 0));  /* Clear work area. */
  ERR(sb_append_rcv_conn_key(this, this->temp_str));
  /* Copy string (deeply)to rcv_conn_key. */
  ERR(sb_str_ref(this->temp_str, &conn_key));
  if (this->rcv_conn_key != NULL) {
    free(this->rcv_conn_key);
  }
  this->rcv_conn_key = strdup(conn_key);

  ERR(msg_get32_int(&this->src_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->src_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->src_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->src_conn_id, in_msg, in_msg_ofs, in_msg_len));

  /* Make sure there's no garbage at the end of the message. */
  if (in_msg_len != *in_msg_ofs) {
    ERR_THROW(EINVAL, "parse_dreq: message too long");
  }

  return ERR_OK;
}  /* parse_dreq */


ERR_F parse_drsp(lbmct_handshake_parser_t *this,
      char *in_msg, int *in_msg_ofs, int in_msg_len)
{
  char *conn_key;

  ERR(msg_get32_int(&this->rcv_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->rcv_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->rcv_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->rcv_conn_id, in_msg, in_msg_ofs, in_msg_len));

  /* No topic string. */
  if (this->topic_str != NULL) {
    free(this->topic_str);
  }
  this->topic_str =  NULL;

  /* Get the receiver's connection key string. */
  ERR(sb_set_len(this->temp_str, 0));  /* Clear work area. */
  ERR(sb_append_rcv_conn_key(this, this->temp_str));
  /* Copy string (deeply)to rcv_conn_key. */
  ERR(sb_str_ref(this->temp_str, &conn_key));
  if (this->rcv_conn_key != NULL) {
    free(this->rcv_conn_key);
  }
  this->rcv_conn_key = strdup(conn_key);

  ERR(msg_get32_int(&this->src_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->src_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->src_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->src_conn_id, in_msg, in_msg_ofs, in_msg_len));

  /* Make sure there's no garbage at the end of the message. */
  if (in_msg_len != *in_msg_ofs) {
    ERR_THROW(EINVAL, "parse_drsp: message too long");
  }

  return ERR_OK;
}  /* parse_drsp */


ERR_F parse_dok(lbmct_handshake_parser_t *this,
      char *in_msg, int *in_msg_ofs, int in_msg_len)
{
  char *conn_key;

  ERR(msg_get32_int(&this->rcv_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->rcv_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->rcv_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->rcv_conn_id, in_msg, in_msg_ofs, in_msg_len));

  /* No topic string. */
  if (this->topic_str != NULL) {
    free(this->topic_str);
  }
  this->topic_str =  NULL;

  /* Get the receiver's connection key string. */
  ERR(sb_set_len(this->temp_str, 0));  /* Clear work area. */
  ERR(sb_append_rcv_conn_key(this, this->temp_str));
  /* Copy string (deeply)to rcv_conn_key. */
  ERR(sb_str_ref(this->temp_str, &conn_key));
  if (this->rcv_conn_key != NULL) {
    free(this->rcv_conn_key);
  }
  this->rcv_conn_key = strdup(conn_key);

  ERR(msg_get32_int(&this->src_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->src_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->src_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->src_conn_id, in_msg, in_msg_ofs, in_msg_len));

  ERR(msg_get32_int(&this->rcv_end_seq_num, in_msg, in_msg_ofs, in_msg_len));

  /* Make sure there's no garbage at the end of the message. */
  if (in_msg_len != *in_msg_ofs) {
    ERR_THROW(EINVAL, "parse_dok: message too long");
  }

  return ERR_OK;
}  /* parse_dok */


ERR_F parse_dfin(lbmct_handshake_parser_t *this,
      char *in_msg, int *in_msg_ofs, int in_msg_len)
{
  char *conn_key;

  ERR(msg_get32_int(&this->rcv_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->rcv_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->rcv_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->rcv_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->rcv_conn_id, in_msg, in_msg_ofs, in_msg_len));

  /* No topic string. */
  if (this->topic_str != NULL) {
    free(this->topic_str);
  }
  this->topic_str =  NULL;

  /* Get the receiver's connection key string. */
  ERR(sb_set_len(this->temp_str, 0));  /* Clear work area. */
  ERR(sb_append_rcv_conn_key(this, this->temp_str));
  /* Copy string (deeply)to rcv_conn_key. */
  ERR(sb_str_ref(this->temp_str, &conn_key));
  if (this->rcv_conn_key != NULL) {
    free(this->rcv_conn_key);
  }
  this->rcv_conn_key = strdup(conn_key);

  ERR(msg_get32_int(&this->src_ct_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_domain_id, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get32_int(&this->src_ip_addr, in_msg, in_msg_ofs, in_msg_len));
  ERR(msg_get16_int(&this->src_request_port, in_msg, in_msg_ofs, in_msg_len));
  this->src_request_port &= 0xffff;  /* Ignore short's sign. */
  ERR(msg_get32_int(&this->src_conn_id, in_msg, in_msg_ofs, in_msg_len));

  /* Make sure there's no garbage at the end of the message. */
  if (in_msg_len != *in_msg_ofs) {
    ERR_THROW(EINVAL, "parse_dfin: message too long");
  }

  return ERR_OK;
}  /* parse_dfin */


/* Main message parser.  Returns MSG_TYPE_... through rtn_msg_type pointer. */
ERR_F lbmct_handshake_parser_parse(lbmct_handshake_parser_t *this,
      int *rtn_msg_type, char *in_msg, int in_msg_len)
{
  int magic = -1;
  int msg_type = -1;
  int in_msg_ofs = 0;

  if (this->sig != LBMCT_SIG_HANDSHAKE_PARSER) {
    ERR_THROW(EINVAL, "lbmct_handshake_parser_parse: bad sig");
  }

  ERR(msg_get32_int(&magic, in_msg, &in_msg_ofs, in_msg_len));

  msg_type = (magic & 0x000000ff);  /* Isolate the command type. */
  magic &= 0xffffff00;  /* Isolate the magic bytes. */
  if (magic != MAGIC) {
    ERR_THROW(EINVAL, "lbmct_handshake_parser_parse: Bad handshake magic bytes");
  }

  switch (msg_type) {
    case MSG_TYPE_CREQ:
      ERR(parse_creq(this, in_msg, &in_msg_ofs, in_msg_len));
      break;
    case MSG_TYPE_CRSP:
      ERR(parse_crsp(this, in_msg, &in_msg_ofs, in_msg_len));
      break;
    case MSG_TYPE_COK:
      ERR(parse_cok(this, in_msg, &in_msg_ofs, in_msg_len));
      break;
    case MSG_TYPE_DREQ:
      ERR(parse_dreq(this, in_msg, &in_msg_ofs, in_msg_len));
      break;
    case MSG_TYPE_DRSP:
      ERR(parse_drsp(this, in_msg, &in_msg_ofs, in_msg_len));
      break;
    case MSG_TYPE_DOK:
      ERR(parse_dok(this, in_msg, &in_msg_ofs, in_msg_len));
      break;
    case MSG_TYPE_DFIN:
      ERR(parse_dfin(this, in_msg, &in_msg_ofs, in_msg_len));
      break;
    default:
      ERR_THROW(EINVAL, "lbmct_handshake_parser_parse: Bad handshake command type");
  }  /* switch msg_type */

  *rtn_msg_type = msg_type;
  return ERR_OK;
}  /* lbmct_handshake_parser_parse */


/* CREQ: Connect request (receiver -> source). */
#define CREQ_MIN_LEN \
  (3+  1+4+   4+   4+   2+ 4+   1+1);
/* MMM 1 CCCC DDDD IIII PP iiii L T...
 * !   ! !    !    !    !  !    ! +- Topic string (L bytes long, one char per byte, not null terminated)
 * !   ! !    !    !    !  !    +--- Length of topic string (1-255) as unsigned 8-bit integer
 * !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
 * !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
 * !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
 * !   +---------------------------- Handshake message type (MSG_TYPE_CREQ)
 * +-------------------------------- Magic bytes indicating CT handshake protocol
 * The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
 * int, as MAGIC + MSG_TYPE_CREQ.
 */

/* The smallest handshake message happens to be a creq. */
#define HANDSHAKE_MIN_LEN CREQ_MIN_LEN

/* Build a creq message using state information from the ct receiver connection object. */
ERR_F make_creq(char *out_msg, int *out_msg_len, int out_msg_alloc_size, lbmct_rcv_conn_t *rcv_conn)
{
  lbmct_t *ct = rcv_conn->ct;
  lbmct_rcv_t *ct_rcv = rcv_conn->ct_rcv;
  int topic_str_len = strlen(ct_rcv->topic_str);

  if (topic_str_len > 256) {
    ERR_THROW(EINVAL, "make_creq: topic string too long");
  }

  *out_msg_len = 0;

  ERR(msg_put32_int(MAGIC + MSG_TYPE_CREQ, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(ct->local_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->rcv_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put8_int(topic_str_len, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put_bytes(ct_rcv->topic_str, topic_str_len, out_msg, out_msg_len, out_msg_alloc_size));

  return ERR_OK;
}  /* make_creq */


/* CRSP: Connect response (source -> receiver). */
/* 3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4+   4;
 * MMM 2 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii LLLL M...
 * !   ! !    !    !    !  !    !    !    !    !  !    !    +- 0 or more bytes of metadata (pure binary).
 * !   ! !    !    !    !  !    !    !    !    !  !    +- Src metadata length as 32-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    !    !  +- Src Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    !    +- Src Port as 16-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    +- Src IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
 * !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
 * !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
 * !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
 * !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
 * !   +---------------------------- Handshake message type (MSG_TYPE_CRSP)
 * +-------------------------------- Magic bytes indicating CT handshake protocol
 * The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
 * int, as MAGIC + MSG_TYPE_CRSP.
 */

/* Build a crsp message using state information from the ct receiver connection object. */
ERR_F make_crsp(char *out_msg, int *out_msg_len, int out_msg_alloc_size, lbmct_src_conn_t *src_conn)
{
  lbmct_t *ct = src_conn->ct;

  *out_msg_len = 0;

  ERR(msg_put32_int(MAGIC + MSG_TYPE_CRSP, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(src_conn->rcv_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(ct->ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(ct->local_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->src_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(ct->metadata_len, out_msg, out_msg_len, out_msg_alloc_size));
  if (ct->metadata_len > 0) {
    ERR(msg_put_bytes(ct->metadata, ct->metadata_len, out_msg, out_msg_len, out_msg_alloc_size));
  }

  return ERR_OK;
}  /* make_crsp */


/* COK: Connection OK (receiver -> source). */
/* 3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4+   4+   4;
 * MMM 2 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii SSSS LLLL M...
 * !   ! !    !    !    !  !    !    !    !    !  !    !    !    +- 0 or more bytes of metadata (pure binary).
 * !   ! !    !    !    !  !    !    !    !    !  !    !    +- Rcv metadata length as 32-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    !    !  !    +- Rcv start sequence number as 32-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    !    !  +- Src Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    !    +- Src Port as 16-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    +- Src IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
 * !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
 * !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
 * !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
 * !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
 * !   +---------------------------- Handshake message type (MSG_TYPE_COK)
 * +-------------------------------- Magic bytes indicating CT handshake protocol
 * The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
 * int, as MAGIC + MSG_TYPE_COK.
 */

/* Build a cok message using state information from the ct receiver connection object. */
ERR_F make_cok(char *out_msg, int *out_msg_len, int out_msg_alloc_size, lbmct_rcv_conn_t *rcv_conn)
{
  lbmct_t *ct = rcv_conn->ct;

  *out_msg_len = 0;

  ERR(msg_put32_int(MAGIC + MSG_TYPE_COK, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(ct->ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(ct->local_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->rcv_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(rcv_conn->src_ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->src_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->src_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(rcv_conn->src_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->src_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(rcv_conn->peer_info.rcv_start_seq_num, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(ct->metadata_len, out_msg, out_msg_len, out_msg_alloc_size));
  if (ct->metadata_len > 0) {
    ERR(msg_put_bytes(ct->metadata, ct->metadata_len, out_msg, out_msg_len, out_msg_alloc_size));
  }

  return ERR_OK;
}  /* make_cok */


/* DREQ: Disconnect request (receiver -> source). */
/* 3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4;
 * MMM 2 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii
 * !   ! !    !    !    !  !    !    !    !    !  +- Src Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    !    +- Src Port as 16-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    +- Src IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
 * !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
 * !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
 * !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
 * !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
 * !   +---------------------------- Handshake message type (MSG_TYPE_DREQ)
 * +-------------------------------- Magic bytes indicating CT handshake protocol
 * The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
 * int, as MAGIC + MSG_TYPE_DREQ.
 */

/* Build a dreq message using state information from the ct receiver connection object. */
ERR_F make_dreq(char *out_msg, int *out_msg_len, int out_msg_alloc_size, lbmct_rcv_conn_t *rcv_conn)
{
  lbmct_t *ct = rcv_conn->ct;

  *out_msg_len = 0;

  ERR(msg_put32_int(MAGIC + MSG_TYPE_DREQ, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(ct->ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(ct->local_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->rcv_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(rcv_conn->src_ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->src_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->src_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(rcv_conn->src_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->src_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  return ERR_OK;
}  /* make_dreq */


/* DRSP: Disconnect response (source -> receiver). */
/* 3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4;
 * MMM 2 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii
 * !   ! !    !    !    !  !    !    !    !    !  +- Src Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    !    +- Src Port as 16-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    +- Src IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
 * !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
 * !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
 * !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
 * !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
 * !   +---------------------------- Handshake message type (MSG_TYPE_DRSP)
 * +-------------------------------- Magic bytes indicating CT handshake protocol
 * The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
 * int, as MAGIC + MSG_TYPE_DRSP.
 */

/* Build a drsp message using state information from the ct receiver connection object. */
ERR_F make_drsp(char *out_msg, int *out_msg_len, int out_msg_alloc_size, lbmct_src_conn_t *src_conn)
{
  lbmct_t *ct = src_conn->ct;

  *out_msg_len = 0;

  ERR(msg_put32_int(MAGIC + MSG_TYPE_DRSP, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(src_conn->rcv_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(ct->ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(ct->local_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->src_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  return ERR_OK;
}  /* make_drsp */


/* DOK: Disconnect OK (receiver -> source). */
/* 3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4+   4;
 * MMM 2 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii SSSS
 * !   ! !    !    !    !  !    !    !    !    !  !    +- Rcv end sequence number as 32-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    !    !  +- Src Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    !    +- Src Port as 16-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    +- Src IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
 * !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
 * !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
 * !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
 * !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
 * !   +---------------------------- Handshake message type (MSG_TYPE_DOK)
 * +-------------------------------- Magic bytes indicating CT handshake protocol
 * The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
 * int, as MAGIC + MSG_TYPE_DOK.
 */

/* Build a dok message using state information from the ct receiver connection object. */
ERR_F make_dok(char *out_msg, int *out_msg_len, int out_msg_alloc_size, lbmct_rcv_conn_t *rcv_conn)
{
  lbmct_t *ct = rcv_conn->ct;

  *out_msg_len = 0;

  ERR(msg_put32_int(MAGIC + MSG_TYPE_DOK, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(ct->ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(ct->local_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->rcv_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(rcv_conn->src_ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->src_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->src_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(rcv_conn->src_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(rcv_conn->src_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(rcv_conn->peer_info.rcv_end_seq_num, out_msg, out_msg_len, out_msg_alloc_size));

  return ERR_OK;
}  /* make_dok */


/* DFIN: Disconnect response (source -> receiver). */
/* 3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4;
 * MMM 2 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii
 * !   ! !    !    !    !  !    !    !    !    !  +- Src Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    !    +- Src Port as 16-bit big-endian integer
 * !   ! !    !    !    !  !    !    !    +- Src IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
 * !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
 * !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
 * !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
 * !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
 * !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
 * !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
 * !   +---------------------------- Handshake message type (MSG_TYPE_DFIN)
 * +-------------------------------- Magic bytes indicating CT handshake protocol
 * The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
 * int, as MAGIC + MSG_TYPE_DFIN.
 */

/* Build a dfin message using state information from the ct receiver connection object. */
ERR_F make_dfin(char *out_msg, int *out_msg_len, int out_msg_alloc_size, lbmct_src_conn_t *src_conn)
{
  lbmct_t *ct = src_conn->ct;

  *out_msg_len = 0;

  ERR(msg_put32_int(MAGIC + MSG_TYPE_DFIN, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(src_conn->rcv_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->rcv_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  ERR(msg_put32_int(ct->ct_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.domain_id, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(ct->local_uim_addr.ip_addr, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put16_int(ct->local_uim_addr.port, out_msg, out_msg_len, out_msg_alloc_size));
  ERR(msg_put32_int(src_conn->src_conn_id, out_msg, out_msg_len, out_msg_alloc_size));

  return ERR_OK;
}  /* make_dfin */
