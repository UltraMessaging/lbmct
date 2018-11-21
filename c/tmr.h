/* tmr.h - External definitions for better timers built on UM timers.
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
#ifndef TMR_H
#define TMR_H

#include "prt.h"  /* Some portability definitions. */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

struct tmr_t_stct;  /* Forward definition. */
typedef struct tmr_t_stct tmr_t;

typedef int (*tmr_cb_proc)(tmr_t *tmr, lbm_context_t *ctx,
  const void *clientd);

enum tmr_state {
  TMR_STATE_IDLE = 1,
  TMR_STATE_TIMING,
  TMR_STATE_CANCEL_PENDING,
};

/* tmr object (app should treat as opaque). */
struct tmr_t_stct {
  prt_mutex_t lock;
  prt_sem_t sync_sem;
  lbm_context_t *ctx;
  enum tmr_state state;
  tmr_cb_proc app_proc;
  void *app_clientd;
  lbm_ulong_t delay;
  int id;
  int um_timer_id;
};


int tmr_create(tmr_t **tmrp, lbm_context_t *ctx);
int tmr_schedule(int *id, tmr_t *tmr, tmr_cb_proc app_proc, void *clientd,
  lbm_ulong_t delay);
int tmr_cancel_sync(tmr_t *tmr);
int tmr_cancel_ctx_thread(tmr_t *tmr);
int tmr_delete_sync(tmr_t *tmr);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif  /* TMR_H */
