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

/**
 * Connection object, mostly for internal use by CT.
 * Application must NOT create, start, or stop this object directly; it is fully managed by CT internally.
 * There are only two public APIs associated with this object: {@link #getRcvConnCbArg} and {@link #isHandshakeMessage}.
 * Those APIs should only be called from inside the receiver callback.
 */
@SuppressWarnings("WeakerAccess")  // public API.
public class LbmCtRcvConn {
  enum States {
    PRE_CREATED, STARTING, RUNNING, ENDING, FIN_WAIT, STOP_WAIT
  }

  private LbmCtRcv ctRcv;  // Initialized in constructor.
  private LbmCt ct;  // Initialized in constructor.
  private LbmCtConfig config;  // Initialized in constructor.
  private LbmCtCtrlr ctrlr;  // Initialized in constructor.
  private LBMContext ctx;  // Initialized in constructor.
  private int rcvConnId;  // Initialized in constructor.
  private States connState = States.PRE_CREATED;
  private Tmr connTmr = null;
  private StringBuilder rcvConnStrBuilder = null;
  private String sourceStr = null;
  private String rcvConnKey = null;

  // Fields taken from source's CRSP message.
  private int srcCtId = 0;
  private int srcDomainId = -1;
  private int srcIpAddr = 0;
  private int srcRequestPort = 0;
  private int srcConnId = 0;
  private long startSequenceNum = 0;
  private long endSequenceNum = 0;
  private ByteBuffer srcMetadata = null;

  private Object rcvConnCbArg = null;
  private LbmCtPeerInfo peerInfo = null;
  private int tryCnt = 0;
  private int currCreqTimeout = 0;
  private int pendingTmrId = 0;
  private RcvConnTmrCallback rcvConnTmrCb = null;
  private boolean wasAppConnCreateCalled = false;
  private boolean wasAppConnDeleteCalled = false;
  private String srcDestAddr = null;
  private ByteBuffer outgoingHandshake = null;
  private LbmCtHandshakeParser handshakeParser = null;
  private boolean handshakeMessage = false;

  // THREAD: ctx
  LbmCtRcvConn(LbmCtRcv ctRcv) {
    this.ctRcv = ctRcv;
    ct = ctRcv.getCt();
    config = ct.getConfig();
    ctrlr = ct.getCtrlr();
    ctx = ct.getCtx();

    rcvConnId = ct.nextConnId();
    ctRcv.addToRcvConnSet(this);
  }

  // Getters.
  LbmCtRcv getCtRcv() { return ctRcv; }
  int getRcvConnId() { return rcvConnId; }
  int getSrcCtId() { return srcCtId; }
  int getSrcDomainId() { return srcDomainId; }
  int getSrcIpAddr() { return srcIpAddr; }
  int getSrcRequestPort() { return srcRequestPort; }
  int getSrcConnId() { return srcConnId; }
  long getStartSequenceNum() { return startSequenceNum; }
  long getEndSequenceNum() { return endSequenceNum; }

  /**
   * Retrieve the application-specific object associated with the connection, supplied by the application as
   *     the return value from {@link LbmCtRcvConnCreateCallback#onRcvConnCreate}.
   *     This API can only be called synchronously from inside the receiver callback.
   * <p>
   * @return  application-specific object.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public Object getRcvConnCbArg() { return rcvConnCbArg; }

  /**
   * Determine if a delivered message is a CT handshake message.
   *     Typically an application ignores CT handshake messages, except that they consume sequence numbers.
   *     This API can only be called synchronously from inside the receiver callback.
   * @return  True=delivered message is a CT handshake, false=delivered message is an application message.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public boolean isHandshakeMessage() { return handshakeMessage; }


  private void setConnState(States newState) {
    States oldState = connState;

    // Sanity check.
    if (newState.ordinal() < oldState.ordinal()) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LbmCtRcvConn::setConnState: attempt to change state from " + oldState + " to " + newState + " prevented.\n");
    } else {
      connState = newState;

      if (newState == States.STOP_WAIT) {
        // In the normal case, the user's delete callback is invoked from inside handleDrsp.  But there are a lot of
        // cases where the conn can be stopped abnormally.  This catches them all.
        if (wasAppConnCreateCalled) {
          if ((ctRcv.getConnDeleteCb() != null) && (! wasAppConnDeleteCalled)) {
            ctRcv.getConnDeleteCb().onRcvConnDelete(ctRcv, peerInfo, ctRcv.getCbArg(), rcvConnCbArg);
          }
          wasAppConnDeleteCalled = true;
        }

        // See if transitioning from an active to an inactive state.
        if ((oldState.ordinal() > States.PRE_CREATED.ordinal()) &&
            (oldState.ordinal() < States.STOP_WAIT.ordinal()))
        {
          ctRcv.decrementActiveConns();
        }
      }
      else {
        // See if transitionining from an inactive state to active.
        if (oldState == States.PRE_CREATED) {
          ctRcv.incrementActiveConns();
        }
      }
    }
  }


  // The conn object is started by the per-source create callback.
  // THREAD: ctx
  void start(String sourceStr) {
    rcvConnTmrCb = new RcvConnTmrCallback(this);
    connTmr = new Tmr(ctx);

    LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
    nextCmd.setRcvConnStart(this, sourceStr);
    ct.getCtrlr().submitNowait(nextCmd);  // This "calls" cmdRcvConnStart below.
  }

  // Triggered from per-source create callback.
  // THREAD: ctrlr
  boolean cmdRcvConnStart(LbmCtCtrlrCmd cmd) throws Exception {
    boolean timerNeeded = false;

    synchronized (ctRcv.getLock()) {
      if (ctRcv.isStopping()) {
        setConnState(States.STOP_WAIT);
      } else {
        setConnState(States.STARTING);
        sourceStr = cmd.getSourceStr();
        outgoingHandshake = ByteBuffer.allocate(LbmCtHandshakeParser.handshakeMaxLen(ct.getMetadata().remaining()));
        handshakeParser = new LbmCtHandshakeParser(ct.getMetadata().remaining());

        // Get the destination address of the source.
        rcvConnStrBuilder = new StringBuilder(64);  // Some extra space.
        rcvConnStrBuilder.append("SOURCE:").append(sourceStr);
        srcDestAddr = rcvConnStrBuilder.toString();

        // Build this connection's key: cccccccccc,dddddddddd:ii.ii.ii.ii:ppppp,iiiiiiiiii
        rcvConnStrBuilder.setLength(0);
        rcvConnStrBuilder.append(ct.getCtId()).append(',');
        if (ct.getLocalDomainId() > -1) {
          rcvConnStrBuilder.append(ct.getLocalDomainId()).append(':');
        }
        rcvConnStrBuilder.append((ct.getLocalIpAddr() >> 24) & 0xff).append('.');
        rcvConnStrBuilder.append((ct.getLocalIpAddr() >> 16) & 0xff).append('.');
        rcvConnStrBuilder.append((ct.getLocalIpAddr() >> 8) & 0xff).append('.');
        rcvConnStrBuilder.append((ct.getLocalIpAddr()) & 0xff).append(':');
        rcvConnStrBuilder.append(ct.getLocalRequestPort()).append(',');
        rcvConnStrBuilder.append(rcvConnId);
        rcvConnKey = rcvConnStrBuilder.toString();

        peerInfo = new LbmCtPeerInfo();
        peerInfo.setStatus(LbmCtPeerInfo.STATUS_OK);
        peerInfo.setRcvMetadata(ct.getMetadata());
        peerInfo.setRcvSourceStr(sourceStr);

        // Short time delay for first CREQ.
        currCreqTimeout = config.getDelayCreq();
        timerNeeded = true;
      }
    }

    if (timerNeeded) {
      pendingTmrId = connTmr.schedule(rcvConnTmrCb, null, currCreqTimeout);
    }
    return true;
  }

  private static class RcvConnTmrCallback implements TmrCallback {
    LbmCtRcvConn rcvConn;

    private RcvConnTmrCallback(LbmCtRcvConn rcvConn) {
      this.rcvConn = rcvConn;
    }

    // Not part of public API.  Declared public to conform to interface.
    // THREAD: ctx
    public void onExpire(Tmr tmr, LBMContext ctx, Object cbArg) {
      // Transition to rcvConn object.
      try {
        rcvConn.rcvConnTmrExpire();
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtRcvConn::OnExpire: " + LbmCt.exDetails(e) + "\n");
      }
    }
  }

  // THREAD: ctx
  private void rcvConnTmrExpire() {
    ct.dbg("rcvConnTmrExpire: " + this);
    synchronized (ctRcv.getLock()) {
      // Don't bother delivering timer ticks if conn is waiting to be deleted.  Ditto stale ticks.
      if ((connState != States.STOP_WAIT) && (connTmr.getId() == pendingTmrId)) {
        LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
        nextCmd.setRcvConnTmrExpire(this, pendingTmrId);
        ctrlr.submitNowait(nextCmd);  // This "calls" cmdRcvConnTmrExpire below.
      }
    }
  }

  // Need to decide upon an action while lock is held, and perform the action after the lock is released.
  enum RcvConnTmrExpireActions { NONE, SEND_CREQ, SEND_DREQ, SEND_DOK }

  // THREAD: ctrlr
  boolean cmdRcvConnTmrExpire(LbmCtCtrlrCmd cmd) throws Exception {
    // Much of the processing must be done with the lock held (synchronized), but some of it must be done after the
    // lock is released.  Use this enum to remember what needs to be done after the lock is released.
    RcvConnTmrExpireActions nextAction = RcvConnTmrExpireActions.NONE;

    synchronized (ctRcv.getLock()) {
      // Make sure we are interested in this timer (it may have been rendered uninteresting).
      if (cmd.getRcvTmrId() == pendingTmrId) {
        pendingTmrId = -1;  // Timer has fired; not pending any more.

        if (connState == States.STARTING) {
          if (ctRcv.isStopping()) {  // User requested stop of the CT receiver; this timer event was probably queued.
            // Even if a CREQ was sent, go straight to time_wait.  No need to send DREQ (the src will timeout CRSPs).
            setConnState(States.STOP_WAIT);
          }
          else {
            // Backoff time initial fast tries.
            if (currCreqTimeout < config.getRetryIvl()) {
              if (currCreqTimeout > 0) {
                currCreqTimeout *= 10;
              } else {
                // current creq timeout is zero (immediate try); re-set it to 1/10 the retry interval
                currCreqTimeout = config.getRetryIvl() / 10;
              }
              // Cap the backoff to the configured retry interval.
              if (currCreqTimeout > config.getRetryIvl()) {
                currCreqTimeout = config.getRetryIvl();
              }
            }

            if (tryCnt < config.getMaxTries()) {
              tryCnt++;  // (Re-)try the CREQ.
              nextAction = RcvConnTmrExpireActions.SEND_CREQ;
            } else {
              LBMPubLog.pubLog(LBM.LOG_WARNING, "giving up connecting to source '" + sourceStr + "' for topic '" + ctRcv.getTopicStr() + "'\n");
              setConnState(States.STOP_WAIT);
            }
          }
        }

        else if (connState == States.ENDING) {
          if (tryCnt < config.getMaxTries()) {
            tryCnt++;  // Retry the DREQ.
            nextAction = RcvConnTmrExpireActions.SEND_DREQ;
          } else {
            LBMPubLog.pubLog(LBM.LOG_WARNING, "giving up stopping connection to source '" + sourceStr + "' for topic '" + ctRcv.getTopicStr() + "'\n");
            peerInfo.setStatus(LbmCtPeerInfo.STATUS_BAD_STOP);

            setConnState(States.STOP_WAIT);
          }
        }

        else if (connState == States.FIN_WAIT) {
          if (tryCnt < config.getMaxTries()) {
            tryCnt++;  // Retry the DREQ.
            nextAction = RcvConnTmrExpireActions.SEND_DOK;
          } else {
            LBMPubLog.pubLog(LBM.LOG_WARNING, "giving up stopping connection to source '" + sourceStr + "' for topic '" + ctRcv.getTopicStr() + "'\n");
            setConnState(States.STOP_WAIT);
          }
        }

        else {  // connState not starting or ending (should not be possible).
          LBMPubLog.pubLog(LBM.LOG_INFO, "Received timeout when rcv_conn in state '" + connState + "'\n");
        }
      }  // if interesting timer
    }  // synchronized

    // Lock released, send appropriate message and set retry timer.
    switch (nextAction) {
      case SEND_CREQ:
        handshakeSendCreq();
        pendingTmrId = connTmr.schedule(rcvConnTmrCb, null, currCreqTimeout);
        break;
      case SEND_DREQ:
        handshakeSendDreq();
        pendingTmrId = connTmr.schedule(rcvConnTmrCb, null, config.getRetryIvl());
        break;
      case SEND_DOK:
        handshakeSendDok();
        pendingTmrId = connTmr.schedule(rcvConnTmrCb, null, config.getRetryIvl());
        break;
    }
    return true;
  }

  // THREAD: ctrlr
  private void handshakeSendCreq() {
    LbmCtHandshakeParser.makeCreq(outgoingHandshake, this);
    /* The UIM send can fail due to "CoreApi-9901-02: target SOURCE type:
     * transport not found".  This is a race condition where a delivery
     * controller is created and the per-source create is called,
     * which enqueues a conn create command, but before that command can
     * execute, the transport session dies and the TR cache is cleared,
     * so that when this send creq function is called, that source is no
     * longer in the topic cache, and the send to "SOURCE:..." fails.
     */
    if ((ct.getConfig().getTestBits() & LbmCtConfig.TEST_BITS_NO_CREQ) == LbmCtConfig.TEST_BITS_NO_CREQ) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LBMCT_TEST_BITS_NO_CREQ, skipping send of CREQ to " + ctRcv.getTopicStr() + "\n");
    } else {
      try {
        ctx.send(srcDestAddr, LbmCt.HANDSHAKE_TOPIC_STR, outgoingHandshake.array(), outgoingHandshake.remaining(), LBM.MSG_FLUSH);
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtRcvConn::handshakeSendCreq: error sending CREQ to " + srcDestAddr + ": " + LbmCt.exDetails(e) + "\n");
      }
    }
  }

  private void handshakeSendDreq() {
    LbmCtHandshakeParser.makeDreq(outgoingHandshake, this);
    if ((ct.getConfig().getTestBits() & LbmCtConfig.TEST_BITS_NO_DREQ) == LbmCtConfig.TEST_BITS_NO_DREQ) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LBMCT_TEST_BITS_NO_DREQ, skipping send of DREQ to " + ctRcv.getTopicStr() + "\n");
    } else {
      try {
        ctx.send(srcDestAddr, LbmCt.HANDSHAKE_TOPIC_STR, outgoingHandshake.array(), outgoingHandshake.remaining(), LBM.MSG_FLUSH);
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtRcvConn::handshakeSendDreq: error sending DREQ to " + srcDestAddr + ": " + LbmCt.exDetails(e) + "\n");
      }
    }
  }

  // Called from LbmCtRcv::OnReceive.  Might be handshake message, or might be user data message.
  // THREAD: ctx
  void handleMsg(LBMMessage umMsg) throws Exception {
    if (connState == States.PRE_CREATED) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LbmCtRcvConn::handleMsg: Received UM event type " + umMsg.type() + " on pre-created connection; ignoring\n");
      umMsg.dispose();
    } else {
      // First check if it is a handshake message.
      if ((umMsg.type() == LBM.MSG_DATA) &&
          (umMsg.properties() != null) &&
          (umMsg.properties().containsKey(LbmCt.HANDSHAKE_TOPIC_STR)))
      {
        ByteBuffer umMsgBB = umMsg.dataBuffer();
        int oldPosition = umMsgBB.position();
        int handshakeType = handshakeParser.parse(umMsgBB);
        umMsgBB.position(oldPosition);  // Restore.

        // Make sure the message is for this connection.
        if ((handshakeParser.getRcvCtId() == ct.getCtId()) && (handshakeParser.getRcvConnId() == rcvConnId)) {
          synchronized (ctRcv.getLock()) {
            // It is a handshake message.  But we can't send it to the ctrlr thread since it needs to be delivered to
            // the user *in order* with other messages.

            long umSequenceNum = umMsg.sequenceNumber();  // This will be saved if it represents a start or end sqn.

            // A crsp is handled by the connection before it is delivered to the app.
            switch (handshakeType) {
              case LbmCtHandshakeParser.MSG_TYPE_CRSP:
                if ((connState == States.STARTING) && ctRcv.isStopping()) {
                  // This conn isn't started yet,
                  // even if a CREQ was sent, go straight to time_wait.  No need to send DREQ (the src will timeout CRSPs).
                  setConnState(States.STOP_WAIT);
                } else {
                  // Want to deliver crsp to user *after* it is handled.
                  handleCrsp(handshakeParser, umSequenceNum);
                  if (connState == States.RUNNING) {
                    handshakeMessage = true;
                    ctRcv.getRcvCb().onReceive(ctRcv.getCbArg(), umMsg);
                  }
                }
                break;
              case LbmCtHandshakeParser.MSG_TYPE_DRSP:
                // Want to deliver drsp to user *before* it is handled.
                if (connState.ordinal() >= States.RUNNING.ordinal() && connState.ordinal() <= States.ENDING.ordinal()) {
                  handshakeMessage = true;
                  ctRcv.getRcvCb().onReceive(ctRcv.getCbArg(), umMsg);
                }
                handleDrsp(handshakeParser, umSequenceNum);
                break;
              case LbmCtHandshakeParser.MSG_TYPE_DFIN:
                // This message never gets delivered since the state is already past running.
                handleDfin(handshakeParser);
                break;
              default:
                // This should never happen, but just in case, still want to deliver it since it consumes a sequence num.
                LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtRcvConn::handleMsg: unexpected handshake type " + handshakeType + "\n");
                if (connState == States.RUNNING) {
                  handshakeMessage = true;
                  ctRcv.getRcvCb().onReceive(ctRcv.getCbArg(), umMsg);
                }
            }

            handshakeParser.clear();
            umMsg.dispose();  // User is not allowed to dispose or promote handshake messages.
          }  // synchronized
        }
        else {  // It is a handshake, but it's not our handshake.  Deliver if running, and dispose.
          if ((connState == States.RUNNING) || config.getPreDelivery() != 0) {
            // Deliver user message to the user.
            handshakeMessage = true;
            ctRcv.getRcvCb().onReceive(ctRcv.getCbArg(), umMsg);
            umMsg.dispose();
          }
        }
      }
      else {  // Not a handshake message.
        handshakeMessage = false;
        if ((connState == States.RUNNING) || config.getPreDelivery() != 0) {
          // Deliver user message to the user.
          handshakeMessage = false;
          ctRcv.getRcvCb().onReceive(ctRcv.getCbArg(), umMsg);
          // User is responsible for promoting or disposing the message.
        } else {
          // Dropping the message.
          umMsg.dispose();
        }
      }
    }
  }

  // THREAD: ctx
  private void handleCrsp(LbmCtHandshakeParser handshakeParser, long umSequenceNum) throws Exception {
    /* This message came across the connection associated with this per-source callback argument.
     * However, there can be multiple receivers with connections to
     * this transport session, so only pay attention to this handshake if it
     * is for *this* connection.
     */
    if (handshakeParser.getRcvConnKey().contentEquals(rcvConnKey)) {  // Is message for this connection?
      if (connState == States.STARTING) {
        // Save conn info from source.
        startSequenceNum = umSequenceNum;
        // Get info from received handshake.
        srcCtId = handshakeParser.getSrcCtId();
        srcDomainId = handshakeParser.getSrcDomainId();
        srcIpAddr = handshakeParser.getSrcIpAddr();
        srcRequestPort = handshakeParser.getSrcRequestPort();

        // If the source reports a domain ID (!= -1), let's use that for our next outgoing msg.
        if (srcDomainId != -1) {
          rcvConnStrBuilder.setLength(0);
          rcvConnStrBuilder.append("TCP:");
          rcvConnStrBuilder.append(srcDomainId).append(':');
          rcvConnStrBuilder.append((srcIpAddr >> 24) & 0xff).append('.');
          rcvConnStrBuilder.append((srcIpAddr >> 16) & 0xff).append('.');
          rcvConnStrBuilder.append((srcIpAddr >> 8) & 0xff).append('.');
          rcvConnStrBuilder.append((srcIpAddr) & 0xff).append(':');
          rcvConnStrBuilder.append(srcRequestPort);

          srcDestAddr = rcvConnStrBuilder.toString();
        }

        srcConnId = handshakeParser.getSrcConnId();
        // Get deep copy of metadata.
        int len = handshakeParser.getMsgMetadata().remaining();
        if (len > 0) {
          srcMetadata = ByteBuffer.allocate(len);
          handshakeParser.getMsgMetadata().get(srcMetadata.array(), 0, len);
          srcMetadata.position(len);
          srcMetadata.flip();
        } else {  // Empty metadata.
          srcMetadata = ByteBuffer.allocate(1);
          srcMetadata.clear();
          srcMetadata.flip();
        }

        peerInfo.setSrcMetadata(srcMetadata);
        peerInfo.setRcvStartSequenceNumber(startSequenceNum);

        setConnState(States.RUNNING);

        // Call the user's per-connection callback.
        if ((ctRcv.getConnCreateCb() != null) &&(! wasAppConnCreateCalled)) {
          rcvConnCbArg = ctRcv.getConnCreateCb().onRcvConnCreate(ctRcv, peerInfo, ctRcv.getCbArg());
        }
        wasAppConnCreateCalled = true;
      }  // if starting

      // State is starting or running; send connection OK message.
      LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
      nextCmd.setRcvSendCok(this);
      ct.getCtrlr().submitNowait(nextCmd);  // This "calls" cmdRcvSendCok below.

      pendingTmrId = -1;

      connTmr.cancelCtxThread();
    }  // This connection.
  }

  // Triggered from per-source create callback.  (cmd supplied for consistency.)
  // THREAD: ctrlr
  boolean cmdRcvSendCok(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) {
    LbmCtHandshakeParser.makeCok(outgoingHandshake, this);
    if ((ct.getConfig().getTestBits() & LbmCtConfig.TEST_BITS_NO_COK) == LbmCtConfig.TEST_BITS_NO_COK) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LBMCT_TEST_BITS_NO_COK, skipping send of CRSP to " + ctRcv.getTopicStr() + "\n");
    } else {
      try {
        ctx.send(srcDestAddr, LbmCt.HANDSHAKE_TOPIC_STR, outgoingHandshake.array(), outgoingHandshake.remaining(), LBM.MSG_FLUSH);
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtRcvConn::handshakeSendCok: error sending COK to " + srcDestAddr + ": " + LbmCt.exDetails(e) + "\n");
      }
    }
    return true;
  }

  // THREAD: ctx
  private void handleDrsp(LbmCtHandshakeParser handshakeParser, long umSequenceNum) throws Exception {
    if (handshakeParser.getRcvConnKey().contentEquals(rcvConnKey)) {  // Is message for this connection?
      if ((connState.ordinal() >= States.STARTING.ordinal()) && (connState.ordinal() <= States.FIN_WAIT.ordinal())) {
        // Capture sequence number of first reception of DRSP.
        if (connState == States.RUNNING || connState == States.ENDING) {
          endSequenceNum = umSequenceNum;
          peerInfo.setRcvEndSequenceNumber(endSequenceNum);
        }

        // Only call the app conn delete callback if the conn create callback was called.  If the conn never got
        // fully started, it didn't get called.
        if (wasAppConnCreateCalled) {
          if ((ctRcv.getConnDeleteCb() != null) && (! wasAppConnDeleteCalled)) {
            ctRcv.getConnDeleteCb().onRcvConnDelete(ctRcv, peerInfo, ctRcv.getCbArg(), rcvConnCbArg);
          }
          wasAppConnDeleteCalled = true;
        }
      }

      setConnState(States.FIN_WAIT);

      pendingTmrId = -1;
      connTmr.cancelCtxThread();

      // Send DOK.

      LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
      nextCmd.setRcvSendDok(this);
      ct.getCtrlr().submitNowait(nextCmd);  // This "calls" cmdRcvSendDok below.
    }  // This connection.
  }

  // THREAD: ctrlr
  boolean cmdRcvSendDok(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) throws Exception {
    handshakeSendDok();
    pendingTmrId = connTmr.schedule(rcvConnTmrCb, null, config.getRetryIvl());
    return true;
  }

  // THREAD: ctrlr
  void handshakeSendDok() {
    LbmCtHandshakeParser.makeDok(outgoingHandshake, this);
    if ((ct.getConfig().getTestBits() & LbmCtConfig.TEST_BITS_NO_DOK) == LbmCtConfig.TEST_BITS_NO_DOK) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LBMCT_TEST_BITS_NO_DOK, skipping send of CRSP to " + ctRcv.getTopicStr() + "\n");
    } else {
      try {
        ctx.send(srcDestAddr, LbmCt.HANDSHAKE_TOPIC_STR, outgoingHandshake.array(), outgoingHandshake.remaining(), LBM.MSG_FLUSH);
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtRcvConn::handshakeSendDok: error sending DOK to " + srcDestAddr + ": " + LbmCt.exDetails(e) + "\n");
      }
    }
  }

  // THREAD: ctx
  private void handleDfin(LbmCtHandshakeParser handshakeParser) throws Exception {
    if (handshakeParser.getRcvConnKey().contentEquals(rcvConnKey)) {  // Is message for this connection?
      synchronized (ctRcv.getLock()) {
        if ((connState == States.STARTING) || (connState == States.RUNNING) || (connState == States.FIN_WAIT) || (connState == States.ENDING)) {
          // In time wait state, we are waiting for the UM receiver delivery controller to be deleted, triggering a
          // call to per-source delete callback.  That does the final connection cleanup.
          setConnState(States.STOP_WAIT);
        }

        pendingTmrId = -1;
      }  // synchronized

      connTmr.cancelCtxThread();
    }  // This connection.
  }


  // Called from onSourceDelete
  // THREAD: ctx
  void stop() throws Exception {
    // The connection is about to be deleted, no more timer ticks.
    pendingTmrId = -1;
    connTmr.cancelCtxThread();
    // As of now, there can be no new conn-level events generated for this conn.  But there might be some queued
    // events, like timers that fired just before the cancel.  Submit this stop to the ctrlr thread to be the last
    // event processed.

    ct.dbg("stop: " + this);
    LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
    nextCmd.setRcvConnStop(this);
    ctrlr.submitNowait(nextCmd);  // This "calls" cmdRcvConnStop below.
  }

  // This performs the stop activities, after the delivery controller has been deleted.
  // THREAD: ctrlr
  boolean cmdRcvConnStop(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) throws Exception {
    synchronized (ctRcv.getLock()) {
      if (connState != States.STOP_WAIT) {
        // Tell app that the connection stopped abnormally.
        peerInfo.setStatus(LbmCtPeerInfo.STATUS_BAD_STOP);
        setConnState(States.STOP_WAIT);
      }

      // No new events for this connection, but there may be queued events.  Process them before final stop.
      LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
      nextCmd.setRcvConnFinalStop(this);
      ctrlr.submitNowait(nextCmd);

      // Remove from the ctRcv connection set.  Won't be found by the disconnect loop any more.
      // This can also wake up the stop on the CtRcv, if a stop is pending.
      ctRcv.removeFromRcvConnSet(this);

      pendingTmrId = -1;
    }

    connTmr.cancelSync();  // Cancel timer, if running.
    return true;
  }

  // THREAD: ctrlr
  boolean cmdRcvConnFinalStop(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) {
    clear();
    return true;
  }

  // Remove any dangling references to other objects.
  private void clear() {
    ctRcv = null;
    ct = null;
    config = null;
    ctrlr = null;
    ctx = null;
    connTmr = null;
    sourceStr = null;
    rcvConnKey = null;
    rcvConnStrBuilder = null;
    srcMetadata = null;
    rcvConnCbArg = null;
    peerInfo = null;
    rcvConnTmrCb = null;
    srcDestAddr = null;
    outgoingHandshake = null;
    handshakeParser = null;
  }

  // Invoked by user calling stop for CtRcv.  That stop calls disconnect for each extant connection.
  // THREAD: ctrlr
  void disconnect() {
    LbmCtCtrlrCmd nextCmd = ct.getCtrlr().cmdGet();
    nextCmd.setRcvConnDisconnect(this);
    ctrlr.submitNowait(nextCmd);
  }

  // Executed when the user calls the receive stop API.  ctRcv.stopping is true.
  // THREAD: ctrlr
  boolean cmdRcvConnDisconnect(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) throws Exception {
    boolean sendDreqNeeded = false;
    boolean tmrCancelNeeded = false;

    synchronized (ctRcv.getLock()) {
      if ((connState == States.PRE_CREATED) || (connState == States.STARTING)) {
        // Even if a CREQ was sent, go straight to time_wait.  No need to send DREQ (the src will timeout CRSPs).
        setConnState(States.STOP_WAIT);
        tmrCancelNeeded = true;  // pre_created and starting have timers that we don't need any more.
      } else if (connState == States.RUNNING) {
        setConnState(States.ENDING);
        sendDreqNeeded = true;
        tmrCancelNeeded = true;
        pendingTmrId = -1;
        tryCnt = 1;  // About to send a DREQ.
      }
    }

    if (tmrCancelNeeded) {
      connTmr.cancelSync();
    }
    if (sendDreqNeeded) {
      handshakeSendDreq();
      pendingTmrId = connTmr.schedule(rcvConnTmrCb, null, config.getRetryIvl());
    }

    return true;
  }
}
