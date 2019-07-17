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
import java.nio.*;
import com.latencybusters.lbm.*;

enum CmdCompletions {
  WAIT, NOWAIT
}

class LbmCtCtrlrCmd {
  enum CmdTypes {
    NONE, TEST, QUIT,
    CT_SRC_START, SRC_CONN_TMR_EXPIRE, SRC_HANDSHAKE, SRC_CONN_FINAL_STOP, CT_SRC_STOP, CT_SRC_UM_SOURCE_STOP,
    CT_SRC_FINAL_STOP,
    RCV_CONN_TMR_EXPIRE, CT_RCV_START, RCV_CONN_START, CT_RCV_STOP, RCV_CONN_STOP, RCV_SEND_COK, RCV_SEND_DOK,
    RCV_CONN_DISCONNECT, RCV_CONN_FINAL_STOP, CT_RCV_UM_RECEIVER_STOP, CT_RCV_FINAL_STOP
  }

  private CmdTypes cmdType;
  private CmdCompletions cmdCompletion;
  private Exception e;

  private Semaphore completeSem;
  private ByteBuffer msgData;

  // Constructor.
  LbmCtCtrlrCmd(LbmCt ct)
  {
    cmdType = CmdTypes.NONE;
    e = null;
    completeSem = new Semaphore(0);
    // Possible max message length, based on this ct's metadata length + 10%.
    msgData = ByteBuffer.allocate(LbmCtHandshakeParser.handshakeMaxLen(ct.getMetadata().remaining()));

    clear();
  }

  // General clear function to release references to command-specific objects.
  void clear() {
    cmdType = CmdTypes.NONE;
    e = null;
    ct = null;
    msgData.clear();  // empty the bytebuffer.
    testStr = null;
    ctSrc = null;
    srcTopicStr = null;
    srcAttr = null;
    srcCb = null;
    srcConnCreateCb = null;
    srcConnDeleteCb = null;
    srcCbArg = null;
    srcConn = null;
    ctRcv = null;
    rcvTopicStr = null;
    rcvAttr = null;
    rcvCb = null;
    rcvConnCreateCb = null;
    rcvConnDeleteCb = null;
    rcvCbArg = null;
    rcvConn = null;
    sourceStr = null;
  }

  // General getters.
  CmdTypes getCmdType() { return cmdType; }
  CmdCompletions getCmdCompletion() { return cmdCompletion; }
  Semaphore getCompleteSem() { return completeSem; }
  ByteBuffer getMsgData() { return msgData; }

  // Error handling.
  boolean isErr() {
    return (e != null);
  }
  String getErrmsg() {
    return LbmCt.exDetails(e);
  }
  Exception getE() {
    return e;
  }
  void setE(Exception e) {
    this.e = e;
  }
  void setCmdCompletion(CmdCompletions cmdCompletion) {
    this.cmdCompletion = cmdCompletion;
  }

  // Command-specific

  // cmdType = TEST

  private LbmCt ct;
  private String testStr;
  private int testErr;

  LbmCt getCt() { return ct; }
  String getTestStr() { return testStr; }
  int getTestErr() { return testErr; }

  void setTest(LbmCt inCt, String inLogText, int inTestErr) {
    cmdType = CmdTypes.TEST;
    ct = inCt;
    testStr = inLogText;
    testErr = inTestErr;
  }

  // cmdType = QUIT

  void setQuit() {
    cmdType = CmdTypes.QUIT;
  }

  // cmdType = SRC_START

  private LbmCtSrc ctSrc;
  private String srcTopicStr;
  private LBMSourceAttributes srcAttr;
  private LBMSourceEventCallback srcCb;
  private LbmCtSrcConnCreateCallback srcConnCreateCb;
  private LbmCtSrcConnDeleteCallback srcConnDeleteCb;
  private Object srcCbArg;

  LbmCtSrc getCtSrc() { return ctSrc; }
  LBMSourceEventCallback getSrcCb() { return srcCb; }
  String getSrcTopicStr() { return srcTopicStr; }
  LBMSourceAttributes getSrcAttr() { return srcAttr; }
  LbmCtSrcConnCreateCallback getSrcConnCreateCb() { return srcConnCreateCb; }
  LbmCtSrcConnDeleteCallback getSrcConnDeleteCb() { return srcConnDeleteCb; }
  Object getSrcCbArg() { return srcCbArg; }

  void setCtSrcStart(LbmCtSrc inCtSrc, String inTopicStr, LBMSourceAttributes inSrcAttr, LBMSourceEventCallback inSrcCb,
                     LbmCtSrcConnCreateCallback inSrcConnCreateCb, LbmCtSrcConnDeleteCallback inSrcConnDeleteCb, Object inSrcCbArg) {
    cmdType = CmdTypes.CT_SRC_START;
    ctSrc = inCtSrc;
    srcTopicStr = inTopicStr;
    srcAttr = inSrcAttr;
    srcCb = inSrcCb;
    srcConnCreateCb = inSrcConnCreateCb;
    srcConnDeleteCb = inSrcConnDeleteCb;
    srcCbArg = inSrcCbArg;
  }

  // cmdType = SRC_CONN_TMR_EXPIRE

  private LbmCtSrcConn srcConn;
  private int srcTmrId;

  int getSrcTmrId() { return srcTmrId; }
  LbmCtSrcConn getSrcConn() { return srcConn; }

  void setSrcConnTmrExpire(LbmCtSrcConn inSrcConn, int inTmrId) {
    cmdType = CmdTypes.SRC_CONN_TMR_EXPIRE;
    srcConn = inSrcConn;
    srcTmrId = inTmrId;
  }

  // cmdType = SRC_CONN_FINAL_STOP

  void setSrcConnFinalStop(LbmCtSrcConn inSrcConn) {
    cmdType = CmdTypes.SRC_CONN_FINAL_STOP;
    srcConn = inSrcConn;
  }

  // cmdType = CT_SRC_STOP

  void setCtSrcStop(LbmCtSrc inCtSrc) {
    cmdType = CmdTypes.CT_SRC_STOP;
    ctSrc = inCtSrc;
  }

  // cmdType = CT_SRC_UM_SOURCE_STOP

  void setCtSrcUmSourceStop(LbmCtSrc inCtSrc) {
    cmdType = CmdTypes.CT_SRC_UM_SOURCE_STOP;
    ctSrc = inCtSrc;
  }

  // cmdType = CT_SRC_FINAL_STOP

  void setCtSrcFinalStop(LbmCtSrc inCtSrc) {
    cmdType = CmdTypes.CT_SRC_FINAL_STOP;
    ctSrc = inCtSrc;
  }

  // cmdType = RCV_START

  private LbmCtRcv ctRcv;
  private String rcvTopicStr;
  private LBMReceiverAttributes rcvAttr;
  private LBMReceiverCallback rcvCb;
  private LbmCtRcvConnCreateCallback rcvConnCreateCb;
  private LbmCtRcvConnDeleteCallback rcvConnDeleteCb;
  private Object rcvCbArg;

  LbmCtRcv getCtRcv() { return ctRcv; }
  LBMReceiverCallback getRcvCb() { return rcvCb; }
  String getRcvTopicStr() { return rcvTopicStr; }
  LBMReceiverAttributes getRcvAttr() { return rcvAttr; }
  LbmCtRcvConnCreateCallback getRcvConnCreateCb() { return rcvConnCreateCb; }
  LbmCtRcvConnDeleteCallback getRcvConnDeleteCb() { return rcvConnDeleteCb; }
  Object getRcvCbArg() { return rcvCbArg; }

  void setCtRcvStart(LbmCtRcv inCtRcv, String inTopicStr, LBMReceiverAttributes inRcvAttr, LBMReceiverCallback inRcvCb,
                     LbmCtRcvConnCreateCallback inRcvConnCreateCb, LbmCtRcvConnDeleteCallback inRcvConnDeleteCb, Object inRcvCbArg) {
    cmdType = CmdTypes.CT_RCV_START;
    ctRcv = inCtRcv;
    rcvTopicStr = inTopicStr;
    rcvAttr = inRcvAttr;
    rcvCb = inRcvCb;
    rcvConnCreateCb = inRcvConnCreateCb;
    rcvConnDeleteCb = inRcvConnDeleteCb;
    rcvCbArg = inRcvCbArg;
  }

  // cmdType = CT_RCV_STOP

  void setCtRcvStop(LbmCtRcv inCtRcv) {
    cmdType = CmdTypes.CT_RCV_STOP;
    ctRcv = inCtRcv;
  }

  // cmdType = SRC_HANDSHAKE

  void setSrcHandshake(LbmCt inCt, LBMMessage inputUmMsg) {
    cmdType = CmdTypes.SRC_HANDSHAKE;
    ct = inCt;

    ByteBuffer inputMsgBB = inputUmMsg.dataBuffer();  // Get byte buffer for UM message.
    int msgLen = (int)inputUmMsg.dataLength();

    // Make sure the message can fit in the cmd object.
    if (msgLen > msgData.capacity()) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "Expanding msgData " + msgData.capacity() + " to " + ((11*msgLen)/10) + "\n");
      msgData = ByteBuffer.allocate((11*msgLen)/10);  // Add 10% more.
    }

    // Copy message contents to msgData.
    msgData.clear();
    if (msgLen > 0) {
      inputMsgBB.get(msgData.array(), inputMsgBB.position(), msgLen);
      msgData.position(msgLen);
      msgData.flip();
    }
  }

  // cmdType = RCV_CONN_START

  private LbmCtRcvConn rcvConn;
  private String sourceStr;

  String getSourceStr() { return sourceStr; }
  LbmCtRcvConn getRcvConn() { return rcvConn; }

  void setRcvConnStart(LbmCtRcvConn inRcvConn, String inSourceStr) {
    cmdType = CmdTypes.RCV_CONN_START;
    rcvConn = inRcvConn;
    sourceStr = inSourceStr;
  }

  // cmdType = RCV_CONN_TMR_EXPIRE

  private int rcvTmrId;

  int getRcvTmrId() { return rcvTmrId; }

  void setRcvConnTmrExpire(LbmCtRcvConn inRcvConn, int inTmrId) {
    cmdType = CmdTypes.RCV_CONN_TMR_EXPIRE;
    rcvConn = inRcvConn;
    rcvTmrId = inTmrId;
  }

  // cmdType = RCV_SEND_COK

  void setRcvSendCok(LbmCtRcvConn inRcvConn) {
    cmdType = CmdTypes.RCV_SEND_COK;
    rcvConn = inRcvConn;
  }

  // cmdType = RCV_SEND_DOK

  void setRcvSendDok(LbmCtRcvConn inRcvConn) {
    cmdType = CmdTypes.RCV_SEND_DOK;
    rcvConn = inRcvConn;
  }

  // cmdType = RCV_CONN_STOP

  void setRcvConnStop(LbmCtRcvConn inRcvConn) {
    cmdType = CmdTypes.RCV_CONN_STOP;
    rcvConn = inRcvConn;
  }

  // cmdType = RCV_CONN_FINAL_STOP

  void setRcvConnFinalStop(LbmCtRcvConn inRcvConn) {
    cmdType = CmdTypes.RCV_CONN_FINAL_STOP;
    rcvConn = inRcvConn;
  }

  // cmdType = CT_RCV_STOP_UM_RECEIVER

  void setCtRcvUmReceiverStop(LbmCtRcv inCtRcv) {
    cmdType = CmdTypes.CT_RCV_UM_RECEIVER_STOP;
    ctRcv = inCtRcv;
  }

  // cmdType = RCV_CONN_DISCONNECT

  void setRcvConnDisconnect(LbmCtRcvConn inRcvConn) {
    cmdType = CmdTypes.RCV_CONN_DISCONNECT;
    rcvConn = inRcvConn;
  }

  // cmdType = CT_RCV_FINAL_STOP

  void setCtRcvFinalStop(LbmCtRcv inCtRcv) {
    cmdType = CmdTypes.CT_RCV_FINAL_STOP;
    ctRcv = inCtRcv;
  }
}