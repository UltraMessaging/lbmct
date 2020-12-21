/* sb.h - string builder.
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

#ifndef SB_H
#define SB_H

/* This include file uses definitions from these include files. */

#include "err.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* For a Windows build of sb.c, set the preprocessor symbol SB_EXPORTS
 * if you want this subsystem to be available from a DLL.
 */
#if defined(_WIN32)
#  ifdef SB_EXPORTS
#    define SB_API __declspec(dllexport)
#  else
#    define SB_API __declspec(dllimport)
#  endif
#else
#  define SB_API
#endif


typedef struct sb_s {
  size_t alloc_size;
  size_t str_len;
  char *str;
} sb_t;


ERR_F sb_constructor(sb_t **rtn_sb, size_t initial_size);
ERR_F sb_destructor(sb_t *sb);
ERR_F sb_str_len(sb_t *sb, size_t *str_len);
ERR_F sb_str_ref(sb_t *sb, char **str);
ERR_F sb_set_len(sb_t *sb, size_t new_len);
ERR_F sb_append_char(sb_t *sb, char in_char);
ERR_F sb_append_bytes(sb_t *sb, char *in_bytes, size_t in_bytes_len);
ERR_F sb_append_str(sb_t *sb, char *in_str);
ERR_F sb_append_int(sb_t *sb, long long in_int);
ERR_F sb_append_uint(sb_t *sb, unsigned long long in_uint);


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif  /* SB_H */
