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

/**
 * A connected source.
 * Maintains an underlying UM source, which is available with the {@link #getUmSrc()} method.
 * When an <tt>LbmCtSrc</tt> is created, its full initialization is deferred until its {@link #start} method is called.
 * Since it is an active object, it must be explicitly stopped when it is no longer needed, using the {@link #stop}
 * method.
 */
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

  private boolean stopping = false;
  private LbmCtCtrlrCmd pendingStopCmd = null;
  private boolean umSourceStopSubmitted = false;
  private boolean finalStopSubmitted = false;

  /**
   * Creates a Connected Topic Source object.
   * Creation of a CT Source object creates an underlying UM source.
   * Unlike a UM source, creation a CT Source object is not a two step process.
   * (A UM source requires first allocating a UM topic object, and then creating the source.
   * A CT source combines the two steps in its {@link #start} method.)
   * Note that to send a message, the application must access the underlying UM source
   * with the {@link #getUmSrc()} method.
   * <p>
   * This constructor only creates the object.
   * Its full initialization is deferred until its <tt>start</tt> method is called.
   */
  public LbmCtSrc() { }

  // Getters.
  LbmCt getCt() { return ct; }
  String getTopicStr() { return topicStr; }
  LbmCtSrcConnCreateCallback getConnCreateCb() { return connCreateCb; }
  LbmCtSrcConnDeleteCallback getConnDeleteCb() { return connDeleteCb; }
  Object getCbArg() { return cbArg; }
  boolean isStopping() { return stopping; }

  /**
   * Obtain a reference to a CT Source's underlying UM source.
   * This is needed so that the application can send messages.
   * @return  reference to underlying UM source.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public LBMSource getUmSrc() { return umSrc; }

  /**
   * Initializes a CT Source object.
   * This is typically called immediately after the object is created ({@link #LbmCtSrc} constructor).
   * The <tt>start</tt> method can return before any connections are made.
   * Any messages sent to the source prior to the first connection event may not be received by any receivers.
   * <p>
   * When the application is finished using this CT Source, it should be stopped ({@link #stop} API).
   * <p>
   * @param inCt  CT object to associate with this CT Source.
   * @param topicStr  Topic string.
   *     This is the same as the <tt>symbol</tt> parameter when allocating a UM source topic object with
   *     <a href="https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMTopic.html#ab728a34e695bfccc73c8fa1a33b0e70d">LBMTopic</a>.
   * @param srcAttr  Attributes object used to configure the underlying UM source.
   *     This is the same as the <tt>lbmsattr</tt> parameter when allocating a UM source topic object with
   *     <a href="https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMTopic.html#ab728a34e695bfccc73c8fa1a33b0e70d">LBMTopic</a>.
   * @param srcCb  An object implementing the
   *     <a href="https://ultramessaging.github.io/currdoc/doc/JavaAPI/interfacecom_1_1latencybusters_1_1lbm_1_1LBMSourceEventCallback.html">LBMSourceEventCallback</a>
   *     interface.
   *     This is the same as the <tt>cb</tt> parameter when creating a UM source with
   *     <a href="https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMSource.html#ab8e3998370398ea2929bfaf814d79457">LBMSource</a>.
   *     It is used to deliver UM source events to the application.
   *     This parameter is optional (null should be passed if not needed).
   *     Note that the <tt>SRC_EVENT_CONNECT</tt> and <tt>SRC_EVENT_DISCONNECT</tt> events should be ignored.
   *     The CT connection create and delete callbacks should be used instead.
   * @param connCreateCb  Callback object invoked when a CT Receiver connects to this CT source.
   *     Technically this parameter is optional (null should be passed if not needed), but its use is central to the
   *     Connected Topics paradigm.
   * @param connDeleteCb  Callback object invoked when a CT receiver disconnects from this CT source.
   *     Technically this parameter is optional (null should be passed if not needed), but its use is central to the
   *     Connected Topics paradigm.
   * @param cbArg  Application-specific object to be associated with the connected source, which is passed to the three
   *     callbacks (<tt>srcCb</tt>, <tt>connCreateCb</tt>, and <tt>connDeleteCb</tt>).
   *     Note that the CT Source maintains a reference to the cbArg object until the CT Source is stopped.
   *     This parameter is optional (null should be passed if not needed).
   * @throws Exception  LBMException thrown.
   */
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

  /**
   * Stops this CT source object.
   * Calling this method performs a graceful disconnection of any active connections, blocking the caller until
   * the disconnections are completed.
   * <p>
   * @throws Exception  LBMException thrown.
   */
  public void stop() throws Exception {
    LbmCtCtrlr ctrlr = this.ctrlr;  // get a local copy since the command will delete the ctSrc.

    LbmCtCtrlrCmd nextCmd = ctrlr.cmdGet();
    nextCmd.setCtSrcStop(this);
    ctrlr.submitWait(nextCmd);  // This "calls" cmdCtSrcStop below.

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
  boolean cmdCtSrcStop(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) throws Exception {
    stopping = true;
    pendingStopCmd = cmd;

    for (LbmCtSrcConn srcConn : srcConnSet) {
      // These disconnects are submitted to the command queue.
      srcConn.disconnect();
    }

    if ((numActiveConns == 0) && (!umSourceStopSubmitted)) {
      // Since stopping is true, no new connections should become active.
      // There might be some connection-related commands queued.  Queue the UM source stop behind them.
      LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
      nextCmd.setCtSrcUmSourceStop(this);
      ctrlr.submitNowait(nextCmd);
      umSourceStopSubmitted = true;
    }

    return false;
  }

  // This is called when there are no more "active" connections.
  // There can still be some connection-related commands queued.
  // THREAD: ctrlr
  boolean cmdCtSrcUmSourceStop(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) throws Exception {
    if (umSrc != null) {
      umSrc.close();
      umSrc = null;
    }

    // At this point, no new connections will be created.  But there might be some commands in the
    // queue waiting to be processed.  Submit the final stop to be behind those.
    if ((numActiveConns != 0) || (! srcConnSet.isEmpty())) {
      // This should never happen.  Log warning and continue.
      LBMPubLog.pubLog(LBM.LOG_WARNING, "Internal error: cmdCtSrcUmSourceStop: numActiveConns=" + numActiveConns + " (should be 0), srcConnSet.isEmpty=" + srcConnSet.isEmpty() + " (should be true)\n");
    } else {
      if (!finalStopSubmitted) {  // Don't submit another one if one is already pending.
        LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
        nextCmd.setCtSrcFinalStop(this);
        ctrlr.submitNowait(nextCmd);  // This "calls" cmdSrcConnStop below.

        finalStopSubmitted = true;
      }
    }
    return true;
  }

  // There should be no more connections of any kind.
  // THREAD: ctrlr
  boolean cmdCtSrcFinalStop(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) {
    synchronized (this) {
      // There should be no connections left at all.
      try {
        if (! srcConnSet.isEmpty()) {
          throw new LBMException("Internal error, cmdCtSrcFinalStop: srcConnSet not empty");
        }
      } catch (Exception e) {
        // Pass the exception back to the application.
        pendingStopCmd.setE(e);
      }

      ct.removeFromCtSrcSet(this);
      // This ct source is now fully removed from the LbmCt.

      // Wake up ctSrc stop().
      ctrlr.cmdComplete(pendingStopCmd);
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
    pendingStopCmd = null;
  }

  void addToSrcConnSet(LbmCtSrcConn srcConn) {
    srcConnSet.add(srcConn);
  }

  void removeFromSrcConnSet(LbmCtSrcConn srcConn) {
    srcConnSet.remove(srcConn);
    if (isStopping() && srcConnSet.isEmpty()) {
      if (!umSourceStopSubmitted) {
        LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
        nextCmd.setCtSrcUmSourceStop(this);
        ctrlr.submitNowait(nextCmd);

        umSourceStopSubmitted = true;
      }
      else if (!finalStopSubmitted) {
        // At this point, no new connections will be created.  But there might be some commands in the
        // queue waiting to be processed.  Submit the final stop to be behind those.
        LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
        nextCmd.setCtSrcFinalStop(this);
        ctrlr.submitNowait(nextCmd);

        finalStopSubmitted = true;
      }
    }
  }

  void incrementActiveConns() {
    numActiveConns++;
  }

  void decrementActiveConns() {
    numActiveConns--;
    if (isStopping()) {
      if ((numActiveConns == 0) && (!umSourceStopSubmitted)) {
        // There might be some connection-related commands queued.  Queue the UM receiver stop behind.
        LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
        nextCmd.setCtSrcUmSourceStop(this);
        ctrlr.submitNowait(nextCmd);
        umSourceStopSubmitted = true;
      }
    }
  }
}