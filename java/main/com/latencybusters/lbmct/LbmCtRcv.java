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
 * A connected receiver.
 * Maintains an underlying UM receiver.
 * When an <tt>LbmCtRcv</tt> is created, its full initialization is deferred until its {@link #start} method is called.
 */
@SuppressWarnings("WeakerAccess")  // public API.
public class LbmCtRcv {
  private Object lock = new Object();
  private LbmCt ct = null;
  private LbmCtCtrlr ctrlr = null;
  private LBMContext ctx = null;
  private LBMReceiver umRcv = null;
  private String topicStr = null;
  private LBMReceiverCallback rcvCb = null;
  private LbmCtRcvConnCreateCallback connCreateCb = null;
  private LbmCtRcvConnDeleteCallback connDeleteCb = null;
  private Object cbArg = null;
  private Set<LbmCtRcvConn> rcvConnSet = null;
  private int numActiveConns = 0;

  private boolean stopping = false;
  private LbmCtCtrlrCmd pendingStopCmd = null;
  private boolean umReceiverStopSubmitted = false;
  private boolean finalStopSubmitted = false;

  /**
   * Creates a Connected Topic Receiver object.
   * Creation of a CT Receiver object creates an underlying UM Receiver.
   * Unlike a UM receiver, creation of a CT Receiver object is not a two step process.
   * (A UM receiver requires first looking up a UM topic object, and then creating the receiver.
   * A CT receiver combines the two steps in its {@link #start} method.)
   * <p>
   * This constructor only creates the object.
   * Its full initialization is deferred until its <tt>start</tt> method is called.
   */
  public LbmCtRcv() { }

  // Getters.
  Object getLock() { return lock; }
  LbmCt getCt() { return ct; }
  String getTopicStr() { return topicStr; }
  LBMReceiverCallback getRcvCb() { return rcvCb; }
  LbmCtRcvConnCreateCallback getConnCreateCb() { return connCreateCb; }
  LbmCtRcvConnDeleteCallback getConnDeleteCb() { return connDeleteCb; }
  Object getCbArg() { return cbArg; }
  boolean isStopping() { return stopping; }

  /**
   * Initilaizes a CT Receiver object.
   * This is typically called immediately after the object is created ({@link #LbmCtRcv} constructor).
   * The <tt>start</tt> method can return before any connections are made.
   * <p>
   * When the application is finished using this CT Receiver, it should be stopped ({@link #stop} API).
   * <p>
   * @param inCt  CT object to associate with this CT Source.
   * @param topicStr  Topic string.
   *     This is the same as the <tt>symbol</tt> parameter when looking up a UM receiver topic object with
   *     <a href="https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMTopic.html#aad9c1f2065be4fd8e357f55063268b22">LBMTopic</a>.
   * @param rcvAttr  Attributes object used to configure the underlying UM receiver.
   *     This is the same as the <tt>lbmrattr</tt> parameter when looking up a UM receiver topic object with
   *     <a href="https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMTopic.html#aad9c1f2065be4fd8e357f55063268b22">LBMTopic</a>.
   * @param rcvCb  An object implementing the <a href="https://ultramessaging.github.io/currdoc/doc/JavaAPI/interfacecom_1_1latencybusters_1_1lbm_1_1LBMReceiverCallback.html">LBMReceiverCallback</a>
   *     interface.
   *     This is the same as the <tt>cb</tt> parameter when creating a UM receiver with
   *     <a href="https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMReceiver.html#ac913d4f6f0b710711c56e6d1c43596ba">LBMReceiver</a>.
   *     It is used to deliver messages and other receiver-related events to the application.
   *     Note that the <tt>LBM_BOS</tt> and <tt>LBM_EOS</tt> events are filtered out and are not delivered to the
   *     application.
   *     The CT connection create and delete callbacks should be used instead.
   * @param connCreateCb  Callback object invoked when a CT source connects to this CT receiver.
   *     Technically this parameter is optional (null should be passed if not needed), but its use is central to the
   *     Connected Topics paradigm.
   * @param connDeleteCb  Callback object invoked when a CT source disconnects from this CT receiver.
   *     Technically this parameter is optional (null should be passed if not needed), but its use is central to the
   *     Connected Topics paradigm.
   * @param cbArg  Application-specific object to be associated with the connected receiver, which is passed to the
   *     three callbacks (<tt>rcvCb</tt>, <tt>connCreateCb</tt>, and <tt>connDeleteCb</tt>).
   *     Note that the CT Receiver maintains a reference to the cbArg object until the CT Receiver is stopped.
   *     This parameter is optional (null should be passed if not needed).
   * @throws Exception  LBMException thrown.
   */
  public void start(LbmCt inCt, String topicStr, LBMReceiverAttributes rcvAttr, LBMReceiverCallback rcvCb,
                    LbmCtRcvConnCreateCallback connCreateCb, LbmCtRcvConnDeleteCallback connDeleteCb, Object cbArg)
      throws Exception
  {
    ct = inCt;
    ctrlr = ct.getCtrlr();
    ctx = ct.getCtx();
    LbmCtCtrlrCmd cmd;
    try {
      cmd = ctrlr.cmdGet();
      cmd.setCtRcvStart(this, topicStr, rcvAttr, rcvCb, connCreateCb, connDeleteCb, cbArg);
      ctrlr.submitWait(cmd);  // This "calls" cmdCtRcvStart below.
    } catch (Exception e) {
      throw (new Exception(e));
    }

    Exception e = cmd.getE();
    ctrlr.cmdFree(cmd);  // Return command object to free pool.
    if (e != null) {
      throw (new Exception(e));
    }
  }

  // THREAD: ctrlr
  boolean cmdCtRcvStart(LbmCtCtrlrCmd cmd) throws Exception {
    topicStr = cmd.getRcvTopicStr();
    connCreateCb = cmd.getRcvConnCreateCb();
    connDeleteCb = cmd.getRcvConnDeleteCb();
    rcvCb = cmd.getRcvCb();
    cbArg = cmd.getRcvCbArg();
    SrcNotification srcNotification = new SrcNotification(this, ct, ctrlr);

    rcvConnSet = new HashSet<>();

    ct.addToCtRcvSet(this);

    LBMReceiverAttributes rcvAttr = cmd.getRcvAttr();
    if (rcvAttr == null) {
      // User didn't supply an attribute object, create our own.
      rcvAttr = new LBMReceiverAttributes();
    }

    rcvAttr.setSourceNotificationCallbacks(srcNotification, srcNotification, null);

    LBMTopic topicObj = new LBMTopic(ctx, topicStr, rcvAttr);
    RcvSideMsgRcvCb rcvSideMsgRcvCb = new RcvSideMsgRcvCb(ct, ctrlr);
    umRcv = new LBMReceiver(ctx, topicObj, rcvSideMsgRcvCb, cbArg);

    if (cmd.getRcvAttr() == null) {
      // We created our own attribute object; delete it.
      rcvAttr.dispose();
    }
    return true;
  }

  /**
   * Stops this CT receiver object.
   * Calling this method performs a graceful disconnection of any active connections, blocking the caller until
   * the disconnections are completed.
   * <p>
   * @throws Exception  LBMException thrown.
   */
  public void stop() throws Exception {
    LbmCtCtrlr ctrlr = this.ctrlr;  // get a local copy since the command will delete the ctSrc.

    LbmCtCtrlrCmd nextCmd = ctrlr.cmdGet();
    nextCmd.setCtRcvStop(this);
    ctrlr.submitWait(nextCmd);  // This "calls" cmdCtRcvStop below.

    Exception e = nextCmd.getE();  // Save exception for re-throwing after freeing the command.
    ctrlr.cmdFree(nextCmd);  // Return command object to free pool.
    if (e != null) {
      throw (new Exception(e));
    }

    // Release any references to other objects.
    clear();
  }

  // THREAD: ctrlr
  boolean cmdCtRcvStop(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) {
    synchronized (this) {
      stopping = true;
      pendingStopCmd = cmd;

      for (LbmCtRcvConn rcvConn : rcvConnSet) {
        // These disconnects are submitted to the command queue.
        rcvConn.disconnect();
      }

      if ((numActiveConns == 0) && (!umReceiverStopSubmitted)) {
        // Since stopping is true, no new connections should become active.
        // There might be some connection-related commands queued.  Queue the UM receiver stop behind them.
        LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
        nextCmd.setCtRcvUmReceiverStop(this);
        ctrlr.submitNowait(nextCmd);
        umReceiverStopSubmitted = true;
      }
    }

    return false;
  }

  // This is called when there are no more "active" connections.  But there can still be STOP_WAIT connections.
  // Also, there can be some connection-related commands queued.
  // THREAD: ctrlr
  boolean cmdCtRcvUmReceiverStop(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) throws Exception {
    // Stop the UM receiver outside of the lock because the call to stop can directly call onSourceDelete(), which
    // might need to take the lock.
    if (umRcv != null) {
      umRcv.close();
      umRcv = null;
    }

    // At this point, no new connections will be created.  But there might be some commands in the
    // queue waiting to be processed.  Submit the final stop to be behind those.
    synchronized(this) {
      if (numActiveConns != 0) {
        // This should never happen.  Log warning and continue.
        LBMPubLog.pubLog(LBM.LOG_WARNING, "Internal error: cmdCtRcvUmReceiverStop: numActiveConns=" + numActiveConns + "\n");
      }
      if (rcvConnSet.isEmpty()) {
        // No more
        if (!finalStopSubmitted) {  // Don't submit another one if one is already pending.
          LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
          nextCmd.setCtRcvFinalStop(this);
          ctrlr.submitNowait(nextCmd);  // This "calls" cmdRcvConnStop below.

          finalStopSubmitted = true;
        }
      }
    }
    return true;
  }

  // There should be no more connections of any kind, active or STOP_WAIT.
  // THREAD: ctrlr
  boolean cmdCtRcvFinalStop(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) {
    synchronized (this) {
      // There should be no connections left at all.
      try {
        if (! rcvConnSet.isEmpty()) {
          throw new LBMException("Internal error, cmdCtRcvFinalStop: rcvConnSet not empty");
        }
      } catch (Exception e) {
        // Pass the exception back to the application.
        pendingStopCmd.setE(e);
      }

      ct.removeFromCtRcvSet(this);
      // This ct receiver is now fully removed from the LbmCt.

      // Wake up ctRcv stop().
      ctrlr.cmdComplete(pendingStopCmd);
    }
    return true;
  }

  // Release references to other objects.  This is in case the
  // application is holding a reference to the CT receiver.
  private void clear() {
    ct = null;
    ctrlr = null;
    ctx = null;
    umRcv = null;
    topicStr = null;
    rcvCb = null;
    connCreateCb = null;
    connDeleteCb = null;
    cbArg = null;
    rcvConnSet = null;
    pendingStopCmd = null;
  }

  void addToRcvConnSet(LbmCtRcvConn rcvConn) {
    rcvConnSet.add(rcvConn);
  }

  void removeFromRcvConnSet(LbmCtRcvConn rcvConn) {
    synchronized (this) {
      rcvConnSet.remove(rcvConn);
      if (isStopping() && rcvConnSet.isEmpty()) {
        if (!umReceiverStopSubmitted) {
          LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
          nextCmd.setCtRcvUmReceiverStop(this);
          ctrlr.submitNowait(nextCmd);

          umReceiverStopSubmitted = true;
        }
        else if (!finalStopSubmitted) {  // Don't submit another one if one is already pending.
          // At this point, no new connections will be created.  But there might be some commands in the
          // queue waiting to be processed.  Submit the final stop to be behind those.
          LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
          nextCmd.setCtRcvFinalStop(this);
          ctrlr.submitNowait(nextCmd);

          finalStopSubmitted = true;
        }
      }
    }
  }

  void incrementActiveConns() {
    synchronized (this) {
      numActiveConns++;
    }
  }

  void decrementActiveConns() {
    synchronized (this) {
      numActiveConns--;
      if (isStopping()) {
        if ((numActiveConns == 0) && (!umReceiverStopSubmitted)) {
          // There might be some connection-related commands queued.  Queue the UM receiver stop behind.
          LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
          nextCmd.setCtRcvUmReceiverStop(this);
          ctrlr.submitNowait(nextCmd);
          umReceiverStopSubmitted = true;
        }
      }
    }
  }

  private static class SrcNotification implements LBMSourceCreationCallback, LBMSourceDeletionCallback {
    LbmCtRcv ctRcv;
    LbmCt ct;
    LbmCtCtrlr ctrlr;

    private SrcNotification(LbmCtRcv ctRcv, LbmCt ct, LbmCtCtrlr ctrlr) {
      this.ctRcv = ctRcv;
      this.ct = ct;
      this.ctrlr = ctrlr;
    }

    // Not part of public API.  Declared public to conform to interface.
    // THREAD: ctx
    public Object onNewSource(String sourceStr, Object cbObj) {
      LbmCtRcvConn rcvConn = new LbmCtRcvConn(ctRcv);

      try {
        ct.dbg("onNewSource: " + rcvConn);
        rcvConn.start(sourceStr);
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtRcv:OnNewSource(" + sourceStr + "): " + LbmCt.exDetails(e) + "\n");
      }
      // Return the rcvConn object as the per-source callback argument.
      return rcvConn;
    }

    // Not part of public API.  Declared public to conform to interface.
    // THREAD: ctx
    public int onSourceDelete(String source, Object cbObj, Object sourceCbObj) {
      LbmCtRcvConn rcvConn = (LbmCtRcvConn)sourceCbObj;

      ct.dbg("onSourceDelete: " + rcvConn);
      try {
        rcvConn.stop();
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtRcv:onSourceDelete: " + LbmCt.exDetails(e) + "\n");
      }
      return 0;
    }
  }

  private static class RcvSideMsgRcvCb implements LBMReceiverCallback {
    LbmCt ct;
    LbmCtCtrlr ctrlr;

    private RcvSideMsgRcvCb(LbmCt ct, LbmCtCtrlr ctrlr) {
      this.ct = ct;
      this.ctrlr = ctrlr;
    }

    // Not part of public API.  Declared public to conform to interface.
    // THREAD: ctx
    public int onReceive(Object cbArgs, LBMMessage umMsg) {
      // Connected Topics receivers don't use BOS/EOS for anything.
      if ((umMsg.type() == LBM.MSG_BOS) || (umMsg.type() == LBM.MSG_EOS)) {
        umMsg.dispose();  // Ignore.
      } else if (umMsg.sourceClientObject() == null) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtRcv.onReceive: topic=" + umMsg.topicName() + ", sourceClientObject is null\n");
        umMsg.dispose();
      } else {
        try {
          LbmCtRcvConn rcvConn = (LbmCtRcvConn)umMsg.sourceClientObject();
          ct.dbg("onReceive: " + rcvConn);
          // Have rcvConn.  Continue handling inside the conn object.
          rcvConn.handleMsg(umMsg);
          // Don't dispose message here because most received messages are for user application, and the app gets to
          // decide the message disposition.
        } catch (Exception e) {
          LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtRcv.onReceive: " + LbmCt.exDetails(e) + "\n");
        }
      }
      return 0;
    }
  }
}
