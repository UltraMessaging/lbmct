/* lbm_internal.h - non-official APIs, un-documented, and subject to change
 * without notice.  For Informatica internal use only.
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

#ifndef _LBM_INTERNAL_H_
#define _LBM_INTERNAL_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* A few misc functions. */
lbm_uint32_t mul_random_range(lbm_uint32_t from, lbm_uint32_t to);
size_t mul_strnlen(const char *s, size_t maxlen);
char *mul_inet_ntop(lbm_uint_t addr, char *dst, size_t buf_len);

/* Two-lock, non-contending queue. */
struct lbm_tl_queue_t_stct;
typedef struct lbm_tl_queue_t_stct lbm_tl_queue_t;
lbm_tl_queue_t *lbm_tl_queue_create(void);
int lbm_tl_queue_enqueue(lbm_tl_queue_t *q, void *value);
int lbm_tl_queue_dequeue(lbm_tl_queue_t *q, void **valuep, int block);
int lbm_tl_queue_delete(lbm_tl_queue_t *q);


/* The rest of this file describes the UM implementation of ASL (Alternating
 * Skip List), an efficient way of storing key-word/value pairs.  Based off
 * the C++ version in August 2000 issue of Dr. Dobb's Journal article written
 * by Laurence Marrie. http://www.ddj.com/184404217?pgno=1
 */

typedef int (*mul_asl_compare_func)(void *lhs, void *rhs);

typedef int (*mul_asl_delete_func)(void *key, void *clientd);

typedef struct mul_asl_node_t_stct mul_asl_node_t;
typedef struct mul_asl_t_stct mul_asl_t;
typedef struct mul_asl_iter_t_stct mul_asl_iter_t;

LBMExpDLL mul_asl_t *mul_asl_create_ex(mul_asl_compare_func cfunc,
  mul_asl_delete_func dfunc, int nodecache_sz, void *clientd);
LBMExpDLL mul_asl_t *mul_asl_create(mul_asl_compare_func cfunc,
  mul_asl_delete_func dfunc, void *clientd);
LBMExpDLL void mul_asl_delete(mul_asl_t *asl);

LBMExpDLL int mul_asl_clear(mul_asl_t *asl);

mul_asl_node_t *mul_asl_find_helper(mul_asl_t *asl, int level,
  mul_asl_node_t *node, void *key);
mul_asl_node_t *mul_asl_find_with_hint_helper(mul_asl_t *asl,
  mul_asl_node_t *hint, void *key);
int mul_asl_insert_helper(mul_asl_t *asl, mul_asl_node_t *node, void *key,
  mul_asl_node_t **new_asl_node_p);

LBMExpDLL mul_asl_node_t *mul_asl_find(mul_asl_t *asl, void *key);
LBMExpDLL mul_asl_node_t *mul_asl_find_with_hint(mul_asl_t *asl,
  mul_asl_node_t *hint, void *key);
LBMExpDLL mul_asl_node_t *mul_asl_find_nearest(mul_asl_t *asl, void *key);

LBMExpDLL int mul_asl_insert(mul_asl_t *asl, void *key);
LBMExpDLL int mul_asl_insert_with_hint(mul_asl_t *asl, mul_asl_node_t *hint,
  void *key);
LBMExpDLL int mul_asl_insert_and_retrieve_node(mul_asl_t *asl, void *key,
  mul_asl_node_t **new_node);

LBMExpDLL void mul_asl_remove(mul_asl_t *asl, void *key);
LBMExpDLL void mul_asl_remove_node(mul_asl_t *asl, mul_asl_node_t *onode);

LBMExpDLL void *mul_asl_node_key(mul_asl_node_t *node);

LBMExpDLL mul_asl_iter_t *mul_asl_iter_create(mul_asl_t *asl);
LBMExpDLL mul_asl_iter_t *mul_asl_iter_create_from_node(mul_asl_t *asl,
  mul_asl_node_t *n);
LBMExpDLL int mul_asl_iter_init(mul_asl_iter_t *iter, mul_asl_t *asl,
  mul_asl_node_t *n);
LBMExpDLL void mul_asl_iter_delete(mul_asl_iter_t *asl);
LBMExpDLL mul_asl_node_t *mul_asl_iter_current(mul_asl_iter_t *iter);

/* These next 4 functions return 0 in all cases */
LBMExpDLL int mul_asl_iter_first(mul_asl_iter_t *iter);
LBMExpDLL int mul_asl_iter_last(mul_asl_iter_t *iter);
LBMExpDLL int mul_asl_iter_next(mul_asl_iter_t *iter);
LBMExpDLL int mul_asl_iter_prev(mul_asl_iter_t *iter);
/* Returns 1 if done, 0 otherwise */
LBMExpDLL int mul_asl_iter_done(mul_asl_iter_t *iter);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _LBM_INTERNAL_H_ */
