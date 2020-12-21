/* sb.c - string builder.
 *
 * This code and its documentation is Copyright 2019, 2019 Steven Ford,
 * http://geeky-boy.com and licensed "public domain" style under Creative
 * Commons "CC0": http://creativecommons.org/publicdomain/zero/1.0/
 * To the extent possible under law, the contributors to this project have
 * waived all copyright and related or neighboring rights to this work.
 * In other words, you can use this code for any purpose without any
 * restrictions. This work is published from: United States. The project home
 * is https://github.com/fordsfords/sb
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "err.h"
#include "sb.h"


/* Internal function, not for public use. */
ERR_F sb_chk_expand(sb_t *sb, size_t add_len)
{
  size_t new_len = sb->str_len + add_len + 1;

  if (new_len > sb->alloc_size) {
    sb->alloc_size = new_len + (new_len / 2);
    sb->str = realloc(sb->str, sb->alloc_size);
    if (sb->str == NULL) {
      fprintf(stderr, "sb_chk_expand: realloc error, aborting.\n"); fflush(stderr);
      abort();
    }
  }

  return ERR_OK;
}  /* sb_chk_expand */


ERR_F sb_constructor(sb_t **rtn_sb, size_t initial_size)
{
  sb_t *sb;

  if (initial_size <= 0) {
    ERR_THROW(EINVAL, "sb_append_int: snprintf returned <= 0");
  }

  sb = (sb_t *)malloc(sizeof(sb_t));
  if (sb == NULL) {
    fprintf(stderr, "sb_constructor: malloc error, aborting.\n"); fflush(stderr);
    abort();
  }

  sb->str_len = 0;
  sb->alloc_size = initial_size;
  sb->str = (char *)malloc(sb->alloc_size);
  if (sb->str == NULL) {
    fprintf(stderr, "sb_constructor: malloc error, aborting.\n"); fflush(stderr);
    free(sb);
    abort();
  }

  *rtn_sb = sb;
  return ERR_OK;
}  /* sb_constructor */


ERR_F sb_destructor(sb_t *sb)
{
  free(sb->str);
  free(sb);

  return ERR_OK;
}  /* sb_destructor */


ERR_F sb_str_len(sb_t *sb, size_t *str_len)
{
  *str_len = sb->str_len;

  return ERR_OK;
}


ERR_F sb_str_ref(sb_t *sb, char **str)
{
  *str = sb->str;

  return ERR_OK;
}


ERR_F sb_set_len(sb_t *sb, size_t new_size)
{
  if (new_size > sb->str_len) {
    ERR_THROW(EINVAL, "sb_set_len: new_size too big");
  }

  sb->str_len = new_size;
  sb->str[new_size] = '\0';

  return ERR_OK;
}  /* sb_set_len */


ERR_F sb_append_char(sb_t *sb, char in_char)
{
  ERR(sb_chk_expand(sb, 1));

  sb->str[sb->str_len] = in_char;
  sb->str_len++;
  sb->str[sb->str_len] = '\0';

  return ERR_OK;
}  /* sb_append_char */


ERR_F sb_append_bytes(sb_t *sb, char *in_bytes, size_t in_bytes_len)
{
  ERR(sb_chk_expand(sb, in_bytes_len));

  memmove(&sb->str[sb->str_len], in_bytes, in_bytes_len);
  sb->str_len += in_bytes_len;
  sb->str[sb->str_len] = '\0';

  return ERR_OK;
}  /* sb_append_bytes */


ERR_F sb_append_str(sb_t *sb, char *in_str)
{
  int in_str_len = strlen(in_str);

  ERR(sb_append_bytes(sb, in_str, in_str_len));

  return ERR_OK;
}  /* sb_append_str */


ERR_F sb_append_int(sb_t *sb, long long in_int)
{
  char tmp_str[128];  /* Longest 64-bit int should be 20 digits plus sign. */
  int snprintf_len;

  snprintf_len = snprintf(tmp_str, sizeof(tmp_str), "%lld", in_int);
  if (snprintf_len <= 0) {
    ERR_THROW(errno, "sb_append_int: snprintf returned <= 0");
  }
  if (snprintf_len > (sizeof(tmp_str) - 1)) {
    ERR_THROW(ERANGE, "sb_append_int: snprintf returned too much");
  }

  ERR(sb_append_bytes(sb, tmp_str, snprintf_len));

  return ERR_OK;
}  /* sb_append_int */


ERR_F sb_append_uint(sb_t *sb, unsigned long long in_uint)
{
  char tmp_str[128];  /* Longest 64-bit int should be 20 digits plus sign. */
  int snprintf_len;

  snprintf_len = snprintf(tmp_str, sizeof(tmp_str), "%llu", in_uint);
  if (snprintf_len <= 0) {
    ERR_THROW(errno, "sb_append_uint: snprintf returned <= 0");
  }
  if (snprintf_len > (sizeof(tmp_str) - 1)) {
    ERR_THROW(ERANGE, "sb_append_uint: snprintf returned too much");
  }

  ERR(sb_append_bytes(sb, tmp_str, snprintf_len));

  return ERR_OK;
}  /* sb_append_uint */
