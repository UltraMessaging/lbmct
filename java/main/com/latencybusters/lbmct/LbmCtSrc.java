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

import java.util.*;
import com.latencybusters.lbm.*;

@SuppressWarnings("WeakerAccess")  // public API.
public class LbmCtSrc {
  private LbmCt ct = null;
  private LbmCtCtrlr ctrlr = null;
  private LBMContext ctx = null;
  private LBMSource umSrc = null;
  private String topicStr = null;
  private LbmCtSrcConnCreateCallback connCreateCb = null;
  private LbmCtSrcConnDeleteCallback connDeleteCb = null;
  private Object cbArg = null;
  private Set<LbmCtSrcConn> srcConnSet = null;
  private int numActiveConns = 0;

  private boolean closing = false;
  private LbmCtCtrlrCmd pendingCloseCmd = null;
  private boolean umSourceCloseSubmitted = false;
  private boolean finalCloseSubmitted = false;

  // Getters.
  LbmCt getCt() { return ct; }
  String getTopicStr() { return topicStr; }
  LbmCtSrcConnCreateCallback getConnCreateCb() { return connCreateCb; }
  LbmCtSrcConnDeleteCallback getConnDeleteCb() { return connDeleteCb; }
  Object getCbArg() { return cbArg; }
  boolean isClosing() { return closing; }

  // Public API to get underlying UM source object from CT source.
  @SuppressWarnings("WeakerAccess")  // public API.
  public LBMSource getUmSrc() { return umSrc; }

  // Public API for actual creation of the UM source.  Just hand off to ctrlr thread.
  @SuppressWarnings("WeakerAccess")  // public API.
  public void start(LbmCt inCt, String topicStr, LBMSourceAttributes srcAttr, LBMSourceEventCallback srcCb,
                    LbmCtSrcConnCreateCallback connCreateCb, LbmCtSrcConnDeleteCallback connDeleteCb, Object cbArg)
      throws Exception
  {
    ct = inCt;
    ctrlr = ct.getCtrlr();
    ctx = ct.getCtx();

    LbmCtCtrlrCmd nextCmd = ctrlr.cmdGet();
    nextCmd.setCtSrcStart(this, topicStr, srcAttr, srcCb, connCreateCb, connDeleteCb, cbArg);
    ctrlr.submitWait(nextCmd);  // This "calls" cmdCtSrcStart below.

    Exception e = nextCmd.getE();  // Save exception to be re-thrown after command free.
    ctrlr.cmdFree(nextCmd);  // Return command object to free pool.
    if (e != null) {
      throw(new Exception(e));
    }
  }

  // THREAD: ctrlr
  boolean cmdCtSrcStart(LbmCtCtrlrCmd cmd) throws Exception {
    topicStr = cmd.getSrcTopicStr();
    LBMSourceEventCallback srcCb = cmd.getSrcCb();
    connCreateCb = cmd.getSrcConnCreateCb();
    connDeleteCb = cmd.getSrcConnDeleteCb();
    cbArg = cmd.getSrcCbArg();
    srcConnSet = new HashSet<>();

    ct.addToCtSrcSet(this);

    LBMTopic topicObj = ctx.allocTopic(topicStr, cmd.getSrcAttr());
    if (srcCb == null) {
      umSrc = new LBMSource(ctx, topicObj);
    } else {
      umSrc = new LBMSource(ctx, topicObj, srcCb, cbArg);
    }

    ct.addToSrcMap(this);
    return true;
  }

  // Public API for deleting CT source.
  @SuppressWarnings("WeakerAccess")  // public API.
  public void close() throws Exception {
    LbmCtCtrlr ctrlr = this.ctrlr;  // get a local copy since the command will delete the ctSrc.

    LbmCtCtrlrCmd nextCmd = ctrlr.cmdGet();
    nextCmd.setCtSrcClose(this);
    ctrlr.submitWait(nextCmd);  // This "calls" cmdCtSrcClose below.

    Exception e = nextCmd.getE();  // Save exception to be re-thrown after cmd free.
    ctrlr.cmdFree(nextCmd);  // Return command object to free pool.
    if (e != null) {
      throw(new Exception(e));
    }

    // No longer need to "find" this conn as UIM handshakes are received.
    ct.removeFromSrcMap(this);

    // Release any references to other objects.
    clear();
  }

  // THREAD: ctrlr
  boolean cmdCtSrcClose(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) throws Exception {
    closing = true;
    pendingCloseCmd = cmd;

    for (LbmCtSrcConn srcConn : srcConnSet) {
      // These disconnects are submitted to the command queue.
      srcConn.disconnect();
    }

    if ((numActiveConns == 0) && (!umSourceCloseSubmitted)) {
      // Since closing is true, no new connections should become active.
      // There might be some connection-related commands queued.  Queue the UM source close behind them.
      LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
      nextCmd.setCtSrcUmSourceClose(this);
      ctrlr.submitNowait(nextCmd);
      umSourceCloseSubmitted = true;
    }

    return false;  // Command completion is triggered by finalCloseMaybe().
  }

  // This is called when there are no more "active" connections.
  // There can still be some connection-related commands queued.
  // THREAD: ctrlr
  boolean cmdCtSrcUmSourceClose(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) throws Exception {
    if (umSrc != null) {
      umSrc.close();
      umSrc = null;
    }

    // At this point, no new connections will be created.  But there might be some commands in the
    // queue waiting to be processed.  Submit the final close to be behind those.
    if ((numActiveConns != 0) || (! srcConnSet.isEmpty())) {
      // This should never happen.  Log warning and continue.
      LBMPubLog.pubLog(LBM.LOG_WARNING, "Internal error: cmdCtSrcUmSourceClose: numActiveConns=" + numActiveConns + " (should be 0), srcConnSet.isEmpty=" + srcConnSet.isEmpty() + " (should be true)\n");
    } else {
      if (!finalCloseSubmitted) {  // Don't submit another one if one is already pending.
        LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
        nextCmd.setCtSrcFinalClose(this);
        ctrlr.submitNowait(nextCmd);  // This "calls" cmdSrcConnClose below.

        finalCloseSubmitted = true;
      }
    }
    return true;
  }

  // There should be no more connections of any kind.
  // THREAD: ctrlr
  boolean cmdCtSrcFinalClose(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) {
    synchronized (this) {
      // There should be no connections left at all.
      try {
        if (! srcConnSet.isEmpty()) {
          throw new LBMException("Internal error, cmdCtSrcFinalClose: srcConnSet not empty");
        }
      } catch (Exception e) {
        // Pass the exception back to the application.
        pendingCloseCmd.setE(e);
      }

      ct.removeFromCtSrcSet(this);
      // This ct source is now fully removed from the LbmCt.

      // Wake up ctSrc close().
      ctrlr.cmdComplete(pendingCloseCmd);
    }
    return true;
  }

  // Release references to other objects.  This is in case the
  // application is holding a reference to the CT source.
  private void clear() {
    ct = null;
    ctrlr = null;
    ctx = null;
    umSrc = null;
    topicStr = null;
    connCreateCb = null;
    connDeleteCb = null;
    cbArg = null;
    srcConnSet = null;
    pendingCloseCmd = null;
  }

  void addToSrcConnSet(LbmCtSrcConn srcConn) {
    srcConnSet.add(srcConn);
  }

  void removeFromSrcConnSet(LbmCtSrcConn srcConn) {
    srcConnSet.remove(srcConn);
    if (isClosing() && srcConnSet.isEmpty()) {
      if (!umSourceCloseSubmitted) {
        LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
        nextCmd.setCtSrcUmSourceClose(this);
        ctrlr.submitNowait(nextCmd);

        umSourceCloseSubmitted = true;
      }
      else if (!finalCloseSubmitted) {
        // At this point, no new connections will be created.  But there might be some commands in the
        // queue waiting to be processed.  Submit the final close to be behind those.
        LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
        nextCmd.setCtSrcFinalClose(this);
        ctrlr.submitNowait(nextCmd);

        finalCloseSubmitted = true;
      }
    }
  }

  void incrementActiveConns() {
    numActiveConns++;
  }

  void decrementActiveConns() {
    numActiveConns--;
    if (isClosing()) {
      if ((numActiveConns == 0) && (!umSourceCloseSubmitted)) {
        // There might be some connection-related commands queued.  Queue the UM receiver close behind.
        LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
        nextCmd.setCtSrcUmSourceClose(this);
        ctrlr.submitNowait(nextCmd);
        umSourceCloseSubmitted = true;
      }
    }
  }
}