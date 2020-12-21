/* tmr.c - better timers built on UM timers.
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
#include <limits.h>
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <unistd.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif
#include <lbm/lbm.h>
#include "tmr.h"
#include "prt.h"  /* Some portability definitions. */


/*
 * Internal functions (not public).
 */


int tmr_expire_cb(lbm_context_t *ctx, const void *clientd)
{
  tmr_t *tmr = (tmr_t *)clientd;

  PRT_MUTEX_LOCK(tmr->lock);

  if (tmr->state == TMR_STATE_TIMING) {
    tmr->um_timer_id = -1;
    /* Set to idle before the callback so that callback can reschedule. */
    tmr->state = TMR_STATE_IDLE;

    /* Call app callback. */
    (*tmr->app_proc)(tmr, tmr->ctx, tmr->app_clientd);
  }

  PRT_MUTEX_UNLOCK(tmr->lock);

  return LBM_OK;
}  /* tmr_expire_cb */


int tmr_cancel_sync_cb(lbm_context_t *ctx, const void *clientd)
{
  tmr_t *tmr = (tmr_t *)clientd;

  PRT_MUTEX_LOCK(tmr->lock);

  if (tmr->state == TMR_STATE_CANCEL_PENDING) {
    if (tmr->um_timer_id != -1) {
      lbm_cancel_timer(tmr->ctx, tmr->um_timer_id, NULL);
      tmr->um_timer_id = -1;
    }
    tmr->state = TMR_STATE_IDLE;
    PRT_SEM_POST(tmr->sync_sem);  /* Wake up the cancel API. */
  }
  else {
    lbm_logf(LBM_LOG_ERR, "tmr_cancel_sync_cb error state=%d [%s:%d]\n",
      (int)tmr->state, BASENAME(__FILE__), __LINE__);
  }

  PRT_MUTEX_UNLOCK(tmr->lock);

  return LBM_OK;
}  /* tmr_cancel_sync_cb */


/*
 * Public APIs
 */


int tmr_create(tmr_t **tmrp, lbm_context_t *ctx)
{
  tmr_t *tmr;

  PRT_MALLOC_N(tmr, tmr_t, sizeof(tmr_t));

  PRT_MUTEX_INIT_RECURSIVE(tmr->lock);
  PRT_SEM_INIT(tmr->sync_sem, 0);  /* Init count to 0. */
  tmr->ctx = ctx;
  tmr->state = TMR_STATE_IDLE;
  tmr->app_proc = NULL;
  tmr->app_clientd = NULL;
  tmr->delay = 0;
  tmr->id = 0;
  tmr->um_timer_id = -1;

  *tmrp = tmr;
  return LBM_OK;
}  /* tmr_create */


int tmr_schedule(int *id, tmr_t *tmr, tmr_cb_proc app_proc, void *clientd,
  lbm_ulong_t delay)
{
  PRT_MUTEX_LOCK(tmr->lock);

  if (tmr->state != TMR_STATE_IDLE) {
    lbm_seterrf(LBM_EINVAL, "Error at %s:%d, 'tmr_schedule error, state=%d`'",
      BASENAME(__FILE__), __LINE__, tmr->state);
    PRT_MUTEX_UNLOCK(tmr->lock);
    return -1;
  }

  tmr->app_proc = app_proc;
  tmr->app_clientd = clientd;
  tmr->delay = delay;
  tmr->state = TMR_STATE_TIMING;
  if (tmr->id == INT_MAX) {
    tmr->id = 1;
  } else {
    tmr->id ++;
  }
  if (id != NULL) {
    *id = tmr->id;
  }

  tmr->um_timer_id = lbm_schedule_timer(tmr->ctx, tmr_expire_cb,
    tmr, NULL, delay);  /* Pass the tmr object as the clientd. */

  PRT_MUTEX_UNLOCK(tmr->lock);
  return LBM_OK;
}  /* tmr_schedule */


int tmr_cancel_sync(tmr_t *tmr)
{
  PRT_MUTEX_LOCK(tmr->lock);

  if (tmr->state == TMR_STATE_CANCEL_PENDING) {
    /* State is cancel pending only briefly; only way this can happen is
     * two threads calling cancel.
     */
    lbm_seterrf(LBM_EINVAL, "Error at %s:%d, 'tmr_cancel_sync error, state=%d`'",
      BASENAME(__FILE__), __LINE__, tmr->state);
    PRT_MUTEX_UNLOCK(tmr->lock);
    return -1;
  }

  /* If UM timer is active, set zero-length timer to cancel it. */
  if (tmr->um_timer_id != -1) {
    tmr->state = TMR_STATE_CANCEL_PENDING;

    /* Schedule zero-duration timer and wait for it to complete. */
    (void)lbm_schedule_timer(tmr->ctx, tmr_cancel_sync_cb,
      tmr, NULL, 0);  /* Pass the tmr object as the clientd. */

    PRT_MUTEX_UNLOCK(tmr->lock);

    PRT_SEM_WAIT(tmr->sync_sem);
  }
  else {
    /* UM timer not active, do nothing. */
    PRT_MUTEX_UNLOCK(tmr->lock);
  }

  return LBM_OK;
}  /* tmr_cancel_sync */


/* Only call from context thread. */
int tmr_cancel_ctx_thread(tmr_t *tmr)
{
  PRT_MUTEX_LOCK(tmr->lock);

  if (tmr->state == TMR_STATE_CANCEL_PENDING) {
    lbm_seterrf(LBM_EINVAL, "Error at %s:%d, 'tmr_cancel_ctx_thread error, state=%d`'",
      BASENAME(__FILE__), __LINE__, tmr->state);
    PRT_MUTEX_UNLOCK(tmr->lock);
    return -1;
  }

  if (tmr->um_timer_id != -1) {
    lbm_cancel_timer(tmr->ctx, tmr->um_timer_id, NULL);
    tmr->um_timer_id = -1;
  }
  tmr->state = TMR_STATE_IDLE;

  PRT_MUTEX_UNLOCK(tmr->lock);

  return LBM_OK;
}  /* tmr_cancel_ctx_thread */


int tmr_delete_sync(tmr_t *tmr)
{
  PRT_MUTEX_LOCK(tmr->lock);

  if (tmr->state == TMR_STATE_TIMING) {
    tmr->state = TMR_STATE_CANCEL_PENDING;

    /* Schedule zero-duration timer and wait for it to complete. */
    (void)lbm_schedule_timer(tmr->ctx, tmr_cancel_sync_cb,
      tmr, NULL, 0);  /* Pass the tmr object as the clientd. */

    PRT_MUTEX_UNLOCK(tmr->lock);

    PRT_SEM_WAIT(tmr->sync_sem);
  }
  else {
    PRT_MUTEX_UNLOCK(tmr->lock);
  }

  PRT_MUTEX_DELETE(tmr->lock);
  PRT_SEM_DELETE(tmr->sync_sem);
  free(tmr);

  return LBM_OK;
}  /* tmr_delete_sync */
