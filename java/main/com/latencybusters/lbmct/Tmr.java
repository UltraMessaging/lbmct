package com.latencybusters.lbmct;
/*
 * See https://github.com/UltraMessaging/lbmct for code and documentation.
 *
 * Copyright (c) 2018-2019 Informatica Corporation. All Rights Reserved.
 * Permission is granted to licensees to use or alter this software for
 * any purpose, including commercial applications, according to the terms
 * laid out in the Software License Agreement.
 -
 - This receiver code example is provided by Informatica for educational
 - and evaluation purposes only.
 -
 - THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
 - EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
 - NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 - INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE UNINTERRUPTED
 - OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE
 - LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR
 - INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE
 - TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED
 - OF THE LIKELIHOOD OF SUCH DAMAGES.
 */

import java.util.concurrent.*;
import com.latencybusters.lbm.*;

/**
 * Wrapper for UM timers with semantics that are more deterministic.
 * When a pending UM timer is canceled, it is possible that the cancel
 * comes a little too late and the timer callback is already being
 * invoked.  In that case, the timer cancel method can return but the
 * callback is still being executed.
 * <p>
 * This wrapper performs a handshake with the underlying UM context
 * to guarantee that, when the cancel method returns, the callback
 * either won't be invoked, or has completed.
 */
class Tmr {
  enum States {
    IDLE, TIMING, CANCEL_PENDING
  }

  private LBMContext ctx;
  private States tmrState;
  private int id;
  private Semaphore wakeupSem;
  private TmrCallback cb;
  private Object cbArg;
  private LBMTimer umTimer;
  private LBMTimer umTimerRecycler;  // re-use old UM timer object.
  private LBMTimer umCanceler;
  private LBMTimer umCancelerRecycler;  // re-use old UM timer object.
  private TmrExpireCallback expireCb;
  private TmrCancelerCallback cancelerCb;

  /**
   * Creates a UM timer wrapper with the given UM context.
   * @param ctx UM context to be used for the underlying UM timers.
   */
  Tmr(LBMContext ctx) {
    this.ctx = ctx;
    tmrState = States.IDLE;
    id = 0;
    wakeupSem = new Semaphore(0);
    cb = null;
    cbArg = null;
    umTimer = null;
    umTimerRecycler = null;
    umCanceler = null;
    umCancelerRecycler = null;
    expireCb = new TmrExpireCallback(this, ctx);
    cancelerCb = new TmrCancelerCallback(this, ctx);
  }

  // Public API to see the ID of the current (or most recent) timer.
  // THREAD: user
  int getId() {
    return id;
  }

  // THREAD: user
  // Public API to schedule a timer.  May be called from any thread.
  int schedule(TmrCallback cb, Object cbArg, int delay) throws Exception {
    int rtnId;
    synchronized (this) {
      if (tmrState != States.IDLE) {
        throw new LBMException("Not idle");
      }
      this.cb = cb;
      this.cbArg = cbArg;
      id++;
      rtnId = id;
      tmrState = States.TIMING;
      if (umTimerRecycler != null) {
        // Re-use previous timer object.
        umTimer = umTimerRecycler;
        umTimerRecycler = null;
        umTimer.reschedule(delay);
      } else {
        umTimer = ctx.scheduleTimer(delay, expireCb, null);
      }
    }  // synchronized

    return rtnId;
  }

  // Normal timer expiration.
  public class TmrExpireCallback implements LBMTimerCallback {
    Tmr tmr;
    LBMContext ctx;

    // THREAD: user
    TmrExpireCallback(Tmr tmr, LBMContext ctx) {
      this.tmr = tmr;
      this.ctx = ctx;
    }

    // THREAD: ctx
    public void onExpiration(Object cbArg) {
      try {
        tmr.expire();
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "Tmr::TmrExpireCallback::onExpire: tmr.expire threw '" + e.toString() + "'\n");
      }
    }
  }

  // Internal method which invokes user's expiration callback (if appropriate).
  // THREAD: ctx
  private void expire() {
    synchronized (this) {
      umTimerRecycler = umTimer;
      umTimer = null;
      if (tmrState == States.TIMING) {
        tmrState = States.IDLE;
        TmrCallback localCb = cb;
        Object localCbArg = cbArg;
        // Release any resources used by the timer.
        cb = null;
        cbArg = null;
        localCb.onExpire(this, ctx, localCbArg);
      }
    }
  }

  // Public API to cancel a timer, NOT from context thread.
  // On return, guarantees that user callback either has been called or won't be called.
  // THREAD: user
  void cancelSync() throws Exception {
    boolean mustWait = false;
    synchronized (this) {
      if (tmrState == States.CANCEL_PENDING) {
        throw new LBMException("Cancel already pending");
      }
      else if (tmrState == States.TIMING) {
        if (umTimer != null) {
          tmrState = States.CANCEL_PENDING;
          if (umCancelerRecycler != null) {
            // Re-use previous timer object.
            umCanceler = umCancelerRecycler;
            umCancelerRecycler = null;
            umCanceler.reschedule(0);
          } else {
            umCanceler = ctx.scheduleTimer(0, cancelerCb, null);
          }
          mustWait = true;
        }
      }
    }  // synchronized

    // Wait for completion.
    if (mustWait) {
      wakeupSem.acquire();
    }
  }

  // Internal method to process a cancel.
  class TmrCancelerCallback implements LBMTimerCallback {
    Tmr tmr;
    LBMContext ctx;

    // THREAD: user
    TmrCancelerCallback(Tmr tmr, LBMContext ctx) {
      this.tmr = tmr;
      this.ctx = ctx;
    }

    // THREAD: ctx
    public void onExpiration(Object cbArg) {
      try {
        tmr.canceler();
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "Tmr::TmrCancelCallback::onExpiration: tmr.canceler threw '" + e.toString() + "'\n");
      }
    }
  }

  // THREAD: ctx
  private void canceler() throws Exception {
    synchronized (this) {
      if (tmrState == States.CANCEL_PENDING) {
        if (umTimer != null) {
          umTimer.cancel();
          umTimerRecycler = umTimer;
          umTimer = null;
        }
        tmrState = States.IDLE;

        // Release any resoureces used by the timer.
        cb = null;
        cbArg = null;

        wakeupSem.release();
      }
    }
  }

  // Public API to cancel timer from context thread.
  // Since already in context thread, don't need to set zero-duration timer.
  // THREAD: ctx
  void cancelCtxThread() throws Exception {
    synchronized (this) {
      if (tmrState == States.CANCEL_PENDING) {
        throw new LBMException("Cancel already pending");
      }
      else if (tmrState == States.TIMING) {
        if (umTimer != null) {
          umTimer.cancel();
          umTimerRecycler = umTimer;
          umTimer = null;
          // Release any resoureces used by the timer.
          cb = null;
          cbArg = null;
        }
      }
      tmrState = States.IDLE;
    }  // synchronized
  }
}

