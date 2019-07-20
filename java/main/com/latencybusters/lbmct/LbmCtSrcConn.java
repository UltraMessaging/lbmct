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

import java.nio.*;
import com.latencybusters.lbm.*;

class LbmCtSrcConn {
  enum States {
    PRE_CREATED, STARTING, RUNNING, ENDING, STOP_WAIT
  }

  private LbmCtSrc ctSrc;  // Initialized in constructor.
  private LbmCt ct;  // Initialized in constructor.
  private LbmCtConfig config;  // Initialized in constructor.
  private LbmCtCtrlr ctrlr;  // Initialized in constructor.
  private int srcConnId;  // Initialized in constructor.
  private States connState = States.PRE_CREATED;
  private Tmr connTmr = null;
  private int tryCnt = 0;
  private LbmCtPeerInfo peerInfo = null;
  private int pendingTmrId = 0;
  private SrcConnTmrCallback srcConnTmrCb = null;

  // Fields taken from receiver's CREQ message.
  private int rcvCtId = 0;
  private int rcvDomainId = -1;
  private int rcvIpAddr = 0;
  private int rcvRequestPort = 0;
  private int rcvConnId = 0;
  private ByteBuffer rcvMetadata = null;

  private String rcvConnKey = null;
  private Object srcConnCbArg = null;
  private ByteBuffer outgoingHandshake = null;
  private boolean wasAppConnCreateCalled = false;
  private boolean wasAppConnDeleteCalled = false;

  /**
   * Creates a source-side connection object.
   * When an <tt>LbmCtSrcConn</tt> is created, its full initialization is deferred until its {@link #start} method is called.
   */
  LbmCtSrcConn(LbmCtSrc ctSrc) {
    this.ctSrc = ctSrc;
    ct = ctSrc.getCt();
    config = ct.getConfig();
    ctrlr = ct.getCtrlr();

    srcConnId = ct.nextConnId();
    ctSrc.addToSrcConnSet(this);
  }

  // Getters.
  LbmCtSrc getCtSrc() { return ctSrc; }
  int getSrcConnId() { return srcConnId; }
  String getRcvConnKey() { return rcvConnKey; }
  int getRcvCtId() { return rcvCtId; }
  int getRcvDomainId() { return rcvDomainId; }
  int getRcvIpAddr() { return rcvIpAddr; }
  int getRcvRequestPort() { return rcvRequestPort; }
  int getRcvConnId() { return rcvConnId; }


  private void setConnState(States newState) {
    States oldState = connState;

    // Sanity check.
    if (newState.ordinal() < oldState.ordinal()) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LbmCtSrcConn::setConnState: attempt to change state from " + oldState + " to " + newState + " prevented.\n");
    } else {
      connState = newState;

      if (newState == States.STOP_WAIT) {
        // In the normal case, the user's delete callback is invoked from inside handleDok.  But there are a lot of
        // cases where the conn can be stopped abnormally.  This catches them all.
        if (wasAppConnCreateCalled) {
          if ((ctSrc.getConnDeleteCb() != null) && (! wasAppConnDeleteCalled)) {
            ctSrc.getConnDeleteCb().onSrcConnDelete(ctSrc, peerInfo, ctSrc.getCbArg(), srcConnCbArg);
          }
          wasAppConnDeleteCalled = true;
        }

        if (oldState != States.STOP_WAIT) {
          // Remove from the ctSrc connection set.  Won't be found by the disconnect loop any more.
          ctSrc.removeFromSrcConnSet(this);
          // Remove from the map that lets the UIM receive find the connection.
          ct.removeFromSrcConnMap(this);

          // There shouldn't be any new events generated, but there might be some events queued, like timers.
          // Queue the connection stop to end up behind any final queued events.
          LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
          nextCmd.setSrcConnFinalStop(this);
          ctrlr.submitNowait(nextCmd);  // This "calls" cmdSrcConnFinalStop below.
        }

        // See if transitioning from an active to an inactive state.
        if ((oldState.ordinal() > States.PRE_CREATED.ordinal()) &&
            (oldState.ordinal() < States.STOP_WAIT.ordinal()))
        {
          ctSrc.decrementActiveConns();
        }
      }
      else {
        // See if transitionining from an inactive state to active.
        if (oldState == States.PRE_CREATED) {
          ctSrc.incrementActiveConns();
        }
      }
    }
  }

  // This performs the stop activities, after the deletion handshakes are done.
  // THREAD: ctrlr
  boolean cmdSrcConnFinalStop(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) {
    clear();
    return true;
  }

  // called when have CREQ message.  The srcConn object is already created, this initializes it.
  // THREAD: ctrlr
  void start(LbmCtHandshakeParser creqHandshakeParser) {
    srcConnTmrCb = new SrcConnTmrCallback(this);
    connTmr = new Tmr(ct.getCtx());

    peerInfo = new LbmCtPeerInfo();
    peerInfo.setStatus(LbmCtPeerInfo.STATUS_OK);
    peerInfo.setSrcMetadata(ct.getMetadata());

    outgoingHandshake = ByteBuffer.allocate(LbmCtHandshakeParser.handshakeMaxLen(ct.getMetadata().remaining()));

    rcvCtId = creqHandshakeParser.getRcvCtId();
    rcvDomainId = creqHandshakeParser.getRcvDomainId();
    rcvIpAddr = creqHandshakeParser.getRcvIpAddr();
    rcvRequestPort = creqHandshakeParser.getRcvRequestPort();
    rcvConnId = creqHandshakeParser.getRcvConnId();
    rcvConnKey = creqHandshakeParser.getRcvConnKey();

    if (ctSrc.isStopping()) {
      setConnState(States.STOP_WAIT);
    } else {
      setConnState(States.STARTING);
    }
  }

  // THREAD: ctrlr
  void handleCreq(@SuppressWarnings("unused") LbmCtHandshakeParser handshakeParser) throws Exception {
    if ((connState == States.STARTING) || (connState == States.RUNNING)) {
      // Each creq restarts the sequence of crsp retries.
      tryCnt = 0;
      pendingTmrId = -1;  // Not expecting a tick.
      connTmr.cancelSync();

      // Send the crsp handshake.
      tryCnt++;
      handshakeSendCrsp();
      pendingTmrId = connTmr.schedule(srcConnTmrCb, null, config.getRetryIvl());
    } else {
      LBMPubLog.pubLog(LBM.LOG_INFO, "Received CREQ handshake while srcConn in state " + connState + "\n");
    }
  }

  // THREAD: ctrlr
  private void handshakeSendCrsp() throws Exception {
    LbmCtHandshakeParser.makeCrsp(outgoingHandshake, this);
    if ((ct.getConfig().getTestBits() & LbmCtConfig.TEST_BITS_NO_CRSP) == LbmCtConfig.TEST_BITS_NO_CRSP) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LBMCT_TEST_BITS_NO_CRSP, skipping send of CRSP to " + ctSrc.getTopicStr() + "\n");
    } else {
      ctSrc.getUmSrc().send(outgoingHandshake.array(), outgoingHandshake.remaining(), LBM.MSG_FLUSH, ct.getSrcExInfo());
    }
  }

  // THREAD: ctrlr
  void handleCok(LbmCtHandshakeParser handshakeParser) throws Exception {
    if (connState == States.STARTING) {
      tryCnt = 0;

      rcvCtId = handshakeParser.getRcvCtId();
      rcvDomainId = handshakeParser.getRcvDomainId();
      rcvIpAddr = handshakeParser.getRcvIpAddr();
      rcvRequestPort = handshakeParser.getRcvRequestPort();
      rcvConnId = handshakeParser.getRcvConnId();
      peerInfo.setRcvStartSequenceNumber(handshakeParser.getRcvStartSequenceNum());

      // Get deep copy of metadata.
      int len = handshakeParser.getMsgMetadata().remaining();
      if (len > 0) {
        rcvMetadata = ByteBuffer.allocate(len);
        handshakeParser.getMsgMetadata().get(rcvMetadata.array(), 0, len);
        rcvMetadata.position(len);
        rcvMetadata.flip();
      } else {  // Empty metadata.
        rcvMetadata = ByteBuffer.allocate(1);
        rcvMetadata.clear();
      }
      peerInfo.setRcvMetadata(rcvMetadata);

      setConnState(States.RUNNING);

      pendingTmrId = -1;  // Not expecting a tick.
      connTmr.cancelSync();

      if ((ctSrc.getConnCreateCb() != null) && (! wasAppConnCreateCalled)) {
        srcConnCbArg = ctSrc.getConnCreateCb().onSrcConnCreate(ctSrc, peerInfo, ctSrc.getCbArg());
      }
      wasAppConnCreateCalled = true;
    } else {
      LBMPubLog.pubLog(LBM.LOG_INFO, "Received COK handshake while srcConn in state " + connState + "\n");
    }
  }

  // THREAD: ctrlr
  void handleDreq(@SuppressWarnings("unused") LbmCtHandshakeParser handshakeParser) throws Exception {
    if ((connState == States.STARTING) || (connState == States.RUNNING || (connState == States.ENDING))) {
      // Each dreq restarts the sequence of drsp retries.
      tryCnt = 0;
      pendingTmrId = -1;  // Not expecting a tick.
      connTmr.cancelSync();

      setConnState(States.ENDING);

      // Send DRSP handshake.
      tryCnt++;
      handshakeSendDrsp();
      pendingTmrId = connTmr.schedule(srcConnTmrCb, null, config.getRetryIvl());
    } else {
      LBMPubLog.pubLog(LBM.LOG_INFO, "Received DREQ handshake while srcConn in state " + connState + "\n");
    }
  }

  /**
   * Implementation of <tt>Tmr</tt> callback interface for source-side connections.
   */
  private static class SrcConnTmrCallback implements TmrCallback {
    LbmCtSrcConn srcConn;

    /**
     * Create timer callback object, associated with the given source connection.
     *
     * @param srcConn  Source connection to associate with the timer callback.
     */
    private SrcConnTmrCallback(LbmCtSrcConn srcConn) {
      this.srcConn = srcConn;
    }

    // Not part of the public API.  Declared public to conform to interface.
    // THREAD: ctx
    public void onExpire(Tmr tmr, LBMContext ctx, Object cbArg) {
      try {
        srcConn.srcConnTmrExpire();
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtSrcConn:OnExpire: " + LbmCt.exDetails(e) + "\n");
      }
    }
  }

  // THREAD: ctx
  private void srcConnTmrExpire() {
    synchronized (this) {
      if (connTmr.getId() == pendingTmrId) {
        LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
        nextCmd.setSrcConnTmrExpire(this, pendingTmrId);
        ctrlr.submitNowait(nextCmd);  // This "calls" cmdSrcConnTmrExpire below.
      }
    }
  }

  // THREAD: ctrlr
  boolean cmdSrcConnTmrExpire(LbmCtCtrlrCmd cmd) throws Exception {
    if (cmd.getSrcTmrId() == pendingTmrId) {
      pendingTmrId = -1;

      if (connState == States.STARTING) {
        if (ctSrc.isStopping()) {
          setConnState(States.STOP_WAIT);
        }
        else {
          // Timed out waiting for COK, retry?
          if (tryCnt < config.getMaxTries()) {
            tryCnt++;  // Retry the CRSP.
            handshakeSendCrsp();
            pendingTmrId = connTmr.schedule(srcConnTmrCb, null, config.getRetryIvl());
          } else {
            // Too many retries, force-delete the connection.
            LBMPubLog.pubLog(LBM.LOG_WARNING, "giving up accepting connection from receiver '" + ctSrc.getTopicStr() + "'\n");
            setConnState(States.STOP_WAIT);
          }
        }
      }
      else if (connState == States.ENDING) {
        // Timed out waiting for DOK, retry?
        if (tryCnt < config.getMaxTries()) {
          tryCnt++;
          handshakeSendDrsp();
          pendingTmrId = connTmr.schedule(srcConnTmrCb, null, config.getRetryIvl());
        } else {
          // Too many retries, force-delete the connection.
          LBMPubLog.pubLog(LBM.LOG_WARNING, "giving up stopping connection from receiver for topic '" + ctSrc.getTopicStr() + "'\n");
          peerInfo.setStatus(LbmCtPeerInfo.STATUS_BAD_DISCONNECT);

          setConnState(States.STOP_WAIT);
        }
      }
      else {
        LBMPubLog.pubLog(LBM.LOG_INFO, "Timer expire on connection in state " + connState + "\n");
      }
    }
    return true;
  }

  // Remove any dangling references to other objects.
  private void clear() {
    ctSrc = null;
    ct = null;
    config = null;
    ctrlr = null;
    connTmr = null;
    srcConnTmrCb = null;
    rcvMetadata = null;
    rcvConnKey = null;
    srcConnCbArg = null;
    peerInfo = null;
    outgoingHandshake = null;
  }

  // THREAD: ctrlr
  private void handshakeSendDrsp() throws Exception {
    LbmCtHandshakeParser.makeDrsp(outgoingHandshake, this);
    if ((ct.getConfig().getTestBits() & LbmCtConfig.TEST_BITS_NO_DRSP) == LbmCtConfig.TEST_BITS_NO_DRSP) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LBMCT_TEST_BITS_NO_DRSP, skipping send of DRSP to " + ctSrc.getTopicStr() + "\n");
    } else {
      ctSrc.getUmSrc().send(outgoingHandshake.array(), outgoingHandshake.remaining(), LBM.MSG_FLUSH, ct.getSrcExInfo());
    }
  }

  // THREAD: ctrlr
  void handleDok(LbmCtHandshakeParser handshakeParser) throws Exception {
    // Capture sequence number of first reception of DOK.
    if (connState == States.RUNNING || connState == States.ENDING) {
      peerInfo.setRcvEndSequenceNumber(handshakeParser.getRcvEndSequenceNum());
    }
    if (wasAppConnCreateCalled) {
      if ((ctSrc.getConnDeleteCb() != null) && (! wasAppConnDeleteCalled)) {
        ctSrc.getConnDeleteCb().onSrcConnDelete(ctSrc, peerInfo, ctSrc.getCbArg(), srcConnCbArg);
      }
      wasAppConnDeleteCalled = true;
    }

    pendingTmrId = -1;
    connTmr.cancelSync();

    handshakeSendDfin();

    setConnState(States.STOP_WAIT);
  }


  // THREAD: ctrlr
  private void handshakeSendDfin() throws Exception {
    LbmCtHandshakeParser.makeDfin(outgoingHandshake, this);
    ctSrc.getUmSrc().send(outgoingHandshake.array(), outgoingHandshake.remaining(), LBM.MSG_FLUSH, ct.getSrcExInfo());
  }

  // THREAD: ctrlr
  void disconnect() throws Exception {
    if (connState == States.RUNNING) {
      setConnState(States.ENDING);

      pendingTmrId = -1;
      connTmr.cancelSync();

      handshakeSendDrsp();
      pendingTmrId = connTmr.schedule(srcConnTmrCb, null, config.getRetryIvl());
    }
  }
}
