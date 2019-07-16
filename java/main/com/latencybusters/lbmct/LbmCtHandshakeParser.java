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

import java.lang.*;
import java.nio.*;
import com.latencybusters.lbm.*;

class LbmCtHandshakeParser {
  // Constants.
  final static int MAGIC=2064878592;  // 7b 13 8c 00  (random bytes from random.org)
  private final static int MSG_TYPE_NONE = 0;
  final static int MSG_TYPE_CREQ = 1;
  final static int MSG_TYPE_CRSP = 2;
  final static int MSG_TYPE_COK  = 3;
  final static int MSG_TYPE_DREQ = 4;
  final static int MSG_TYPE_DRSP = 5;
  final static int MSG_TYPE_DOK  = 6;
  final static int MSG_TYPE_DFIN = 7;

  // State data used by message parsing.
  private StringBuilder msgStrBuilder;  // Initialized in constructor
  private ByteBuffer msgMetadata;  // Initialized in constructor
  private int msgType = MSG_TYPE_NONE;
  private int rcvCtId = 0;
  private int rcvDomainId = 0;
  private int rcvIpAddr = 0;
  private int rcvRequestPort = 0;
  private int rcvConnId = 0;
  private String topicStr = null;
  private String rcvConnKey = null;
  private int srcCtId = 0;
  private int srcDomainId = 0;
  private int srcIpAddr = 0;
  private int srcRequestPort = 0;
  private int srcConnId = 0;
  private long rcvStartSequenceNum = 0;
  private long rcvEndSequenceNum = 0;

  // Constructor.
  LbmCtHandshakeParser(int metadataSize) {
    msgStrBuilder = new StringBuilder(256);
    // Allocate 10% extra for metadata.
    msgMetadata = ByteBuffer.allocate((11*metadataSize)/10);
  }

  void clear() {
    msgStrBuilder.setLength(0);
    msgMetadata.clear();
    msgType = MSG_TYPE_NONE;
    rcvCtId = 0;
    rcvDomainId = 0;
    rcvIpAddr = 0;
    rcvRequestPort = 0;
    rcvConnId = 0;
    topicStr = null;
    rcvConnKey = null;
    srcCtId = 0;
    srcDomainId = 0;
    srcIpAddr = 0;
    srcRequestPort = 0;
    srcConnId = 0;
  }

  // Getters, for use after a message has been parsed.
  int getRcvCtId() { return rcvCtId; }
  int getRcvDomainId() { return rcvDomainId; }
  int getRcvIpAddr() { return rcvIpAddr; }
  int getRcvRequestPort() { return rcvRequestPort; }
  int getRcvConnId() { return rcvConnId; }
  String getRcvConnKey() { return rcvConnKey; }
  String getTopicStr() { return topicStr; }
  int getSrcCtId() { return srcCtId; }
  int getSrcDomainId() { return srcDomainId; }
  int getSrcIpAddr() { return srcIpAddr; }
  int getSrcRequestPort() { return srcRequestPort; }
  int getSrcConnId() { return srcConnId; }
  long getRcvStartSequenceNum() { return rcvStartSequenceNum; }
  long getRcvEndSequenceNum() { return rcvEndSequenceNum; }
  ByteBuffer getMsgMetadata() { return msgMetadata; }

  // Main message parser.  Returns MSG_TYPE_...
  int parse(ByteBuffer inMsgBB) throws Exception {
    int msgLen = inMsgBB.remaining();
    if ((msgLen < HANDSHAKE_MIN_LEN)) {
      throw (new LBMException("Bad handshake len " + msgLen));
    }

    int magic = inMsgBB.getInt();
    msgType = (magic & 0x000000ff);  // Isolate the command type.
    magic &= 0xffffff00;  // Isolate the magic bytes.
    if (magic != MAGIC) {
      throw (new LBMException("Bad handshake magic bytes " + magic));
    }

    switch (msgType) {
      case MSG_TYPE_CREQ:
        parseCreq(inMsgBB);
        break;
      case MSG_TYPE_CRSP:
        parseCrsp(inMsgBB);
        break;
      case MSG_TYPE_COK:
        parseCok(inMsgBB);
        break;
      case MSG_TYPE_DREQ:
        parseDreq(inMsgBB);
        break;
      case MSG_TYPE_DRSP:
        parseDrsp(inMsgBB);
        break;
      case MSG_TYPE_DOK:
        parseDok(inMsgBB);
        break;
      case MSG_TYPE_DFIN:
        parseDfin(inMsgBB);
        break;
      default:
        throw (new LBMException("Bad handshake command type " + msgType));
    }

    return msgType;
  }

  private void parseCreq(ByteBuffer inMsg) throws Exception {
    rcvCtId = inMsg.getInt();
    rcvDomainId = inMsg.getInt();
    rcvIpAddr = inMsg.getInt();
    rcvRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    rcvConnId = inMsg.getInt();

    // Get topic string.
    int topicStrLen = (int) inMsg.get();
    topicStrLen &= 0x000000ff;  // Bytes are signed, and therefore you get sign extension.  Isolate unsigned value.
    if (topicStrLen != inMsg.remaining()) {
      throw (new LBMException("Bad topic string len " + topicStrLen + " or remaining message len " + inMsg.remaining()));
    }
    msgStrBuilder.setLength(0);
    while (topicStrLen > 0) {
      msgStrBuilder.append((char) inMsg.get());
      topicStrLen--;
    }
    topicStr = msgStrBuilder.toString();

    // Get the receiver's connection key.
    msgStrBuilder.setLength(0);
    msgStrBuilder.append(rcvCtId);  msgStrBuilder.append(',');
    if (rcvDomainId > -1) {
      msgStrBuilder.append(rcvDomainId);  msgStrBuilder.append(':');
    }
    msgStrBuilder.append((rcvIpAddr >> 24) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 16) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 8) & 0xff);   msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr) & 0xff);        msgStrBuilder.append(':');
    msgStrBuilder.append(rcvRequestPort);            msgStrBuilder.append(',');
    msgStrBuilder.append(rcvConnId);
    rcvConnKey = msgStrBuilder.toString();
  }

  private void parseCrsp(ByteBuffer inMsg) throws Exception {
    rcvCtId = inMsg.getInt();
    rcvDomainId = inMsg.getInt();
    rcvIpAddr = inMsg.getInt();
    rcvRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    rcvConnId = inMsg.getInt();

    // No topic string.
    topicStr = null;

    // Get the receiver's connection key.
    msgStrBuilder.setLength(0);
    msgStrBuilder.append(rcvCtId);  msgStrBuilder.append(',');
    if (rcvDomainId > -1) {
      msgStrBuilder.append(rcvDomainId);  msgStrBuilder.append(':');
    }
    msgStrBuilder.append((rcvIpAddr >> 24) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 16) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 8) & 0xff);   msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr) & 0xff);        msgStrBuilder.append(':');
    msgStrBuilder.append(rcvRequestPort);            msgStrBuilder.append(',');
    msgStrBuilder.append(rcvConnId);
    rcvConnKey = msgStrBuilder.toString();

    srcCtId = inMsg.getInt();
    srcDomainId = inMsg.getInt();
    srcIpAddr = inMsg.getInt();
    srcRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    srcConnId = inMsg.getInt();

    int metadataLen = inMsg.getInt();
    if (metadataLen != inMsg.remaining()) {
      throw (new LBMException("Bad metadata len " + metadataLen + " or remaining message len " + inMsg.remaining()));
    }
    if (metadataLen > msgMetadata.capacity()) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "Expanding srcMetadata " + msgMetadata.capacity() + " to " + ((11*metadataLen)/10) + "\n");
      msgMetadata = ByteBuffer.allocate((11*metadataLen)/10);
    }
    if (metadataLen > 0) {
      inMsg.get(msgMetadata.array(), 0, metadataLen);
      msgMetadata.position(metadataLen);
    } else {
      msgMetadata.clear();
    }
    msgMetadata.flip();
  }

  private void parseCok(ByteBuffer inMsg) throws Exception {
    rcvCtId = inMsg.getInt();
    rcvDomainId = inMsg.getInt();
    rcvIpAddr = inMsg.getInt();
    rcvRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    rcvConnId = inMsg.getInt();

    // No topic string.
    topicStr = null;

    // Get the receiver's connection key.
    msgStrBuilder.setLength(0);
    msgStrBuilder.append(rcvCtId);  msgStrBuilder.append(',');
    if (rcvDomainId > -1) {
      msgStrBuilder.append(rcvDomainId);  msgStrBuilder.append(':');
    }
    msgStrBuilder.append((rcvIpAddr >> 24) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 16) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 8) & 0xff);   msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr) & 0xff);        msgStrBuilder.append(':');
    msgStrBuilder.append(rcvRequestPort);            msgStrBuilder.append(',');
    msgStrBuilder.append(rcvConnId);
    rcvConnKey = msgStrBuilder.toString();

    srcCtId = inMsg.getInt();
    srcDomainId = inMsg.getInt();
    srcIpAddr = inMsg.getInt();
    srcRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    srcConnId = inMsg.getInt();

    rcvStartSequenceNum = inMsg.getInt();

    int metadataLen = inMsg.getInt();
    if (metadataLen != inMsg.remaining()) {
      throw (new LBMException("Bad metadata len " + metadataLen + " or remaining message len " + inMsg.remaining()));
    }
    if (metadataLen > msgMetadata.capacity()) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "Expanding srcMetadata " + msgMetadata.capacity() + " to " + ((11*metadataLen)/10) + "\n");
      msgMetadata = ByteBuffer.allocate((11*metadataLen)/10);
    }
    if (metadataLen > 0) {
      inMsg.get(msgMetadata.array(), 0, metadataLen);
      msgMetadata.position(metadataLen);
    } else {
      msgMetadata.clear();
    }
    msgMetadata.flip();
  }

  private void parseDreq(ByteBuffer inMsg) {
    rcvCtId = inMsg.getInt();
    rcvDomainId = inMsg.getInt();
    rcvIpAddr = inMsg.getInt();
    rcvRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    rcvConnId = inMsg.getInt();

    // No topic string.
    topicStr = null;

    // Get the receiver's connection key.
    msgStrBuilder.setLength(0);
    msgStrBuilder.append(rcvCtId);  msgStrBuilder.append(',');
    if (rcvDomainId > -1) {
      msgStrBuilder.append(rcvDomainId);  msgStrBuilder.append(':');
    }
    msgStrBuilder.append((rcvIpAddr >> 24) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 16) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 8) & 0xff);   msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr) & 0xff);        msgStrBuilder.append(':');
    msgStrBuilder.append(rcvRequestPort);            msgStrBuilder.append(',');
    msgStrBuilder.append(rcvConnId);
    rcvConnKey = msgStrBuilder.toString();

    srcCtId = inMsg.getInt();
    srcDomainId = inMsg.getInt();
    srcIpAddr = inMsg.getInt();
    srcRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    srcConnId = inMsg.getInt();
  }

  private void parseDrsp(ByteBuffer inMsg) {
    rcvCtId = inMsg.getInt();
    rcvDomainId = inMsg.getInt();
    rcvIpAddr = inMsg.getInt();
    rcvRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    rcvConnId = inMsg.getInt();

    // No topic string.
    topicStr = null;

    // Get the receiver's connection key.
    msgStrBuilder.setLength(0);
    msgStrBuilder.append(rcvCtId);  msgStrBuilder.append(',');
    if (rcvDomainId > -1) {
      msgStrBuilder.append(rcvDomainId);  msgStrBuilder.append(':');
    }
    msgStrBuilder.append((rcvIpAddr >> 24) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 16) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 8) & 0xff);   msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr) & 0xff);        msgStrBuilder.append(':');
    msgStrBuilder.append(rcvRequestPort);            msgStrBuilder.append(',');
    msgStrBuilder.append(rcvConnId);
    rcvConnKey = msgStrBuilder.toString();

    srcCtId = inMsg.getInt();
    srcDomainId = inMsg.getInt();
    srcIpAddr = inMsg.getInt();
    srcRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    srcConnId = inMsg.getInt();
  }

  private void parseDok(ByteBuffer inMsg) {
    rcvCtId = inMsg.getInt();
    rcvDomainId = inMsg.getInt();
    rcvIpAddr = inMsg.getInt();
    rcvRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    rcvConnId = inMsg.getInt();

    // No topic string.
    topicStr = null;

    // Get the receiver's connection key.
    msgStrBuilder.setLength(0);
    msgStrBuilder.append(rcvCtId);  msgStrBuilder.append(',');
    if (rcvDomainId > -1) {
      msgStrBuilder.append(rcvDomainId);  msgStrBuilder.append(':');
    }
    msgStrBuilder.append((rcvIpAddr >> 24) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 16) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 8) & 0xff);   msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr) & 0xff);        msgStrBuilder.append(':');
    msgStrBuilder.append(rcvRequestPort);            msgStrBuilder.append(',');
    msgStrBuilder.append(rcvConnId);
    rcvConnKey = msgStrBuilder.toString();

    srcCtId = inMsg.getInt();
    srcDomainId = inMsg.getInt();
    srcIpAddr = inMsg.getInt();
    srcRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    srcConnId = inMsg.getInt();

    rcvEndSequenceNum = inMsg.getInt();
  }

  private void parseDfin(ByteBuffer inMsg) {
    rcvCtId = inMsg.getInt();
    rcvDomainId = inMsg.getInt();
    rcvIpAddr = inMsg.getInt();
    rcvRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    rcvConnId = inMsg.getInt();

    // No topic string.
    topicStr = null;

    // Get the receiver's connection key.
    msgStrBuilder.setLength(0);
    msgStrBuilder.append(rcvCtId);  msgStrBuilder.append(',');
    if (rcvDomainId > -1) {
      msgStrBuilder.append(rcvDomainId);  msgStrBuilder.append(':');
    }
    msgStrBuilder.append((rcvIpAddr >> 24) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 16) & 0xff);  msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr >> 8) & 0xff);   msgStrBuilder.append('.');
    msgStrBuilder.append((rcvIpAddr) & 0xff);        msgStrBuilder.append(':');
    msgStrBuilder.append(rcvRequestPort);            msgStrBuilder.append(',');
    msgStrBuilder.append(rcvConnId);
    rcvConnKey = msgStrBuilder.toString();

    srcCtId = inMsg.getInt();
    srcDomainId = inMsg.getInt();
    srcIpAddr = inMsg.getInt();
    srcRequestPort = (int) inMsg.getShort();  rcvRequestPort &= 0xffff;  // Make unsigned.
    srcConnId = inMsg.getInt();
  }

  // Static methods for constructing message.

  static int handshakeMaxLen(int metadataLen) {
    // Set aside 10% more space for the peer's metadata.
    int extraSpace = Math.max((11*metadataLen)/10, 256);
    // COK has the largest fixed space.
    return COK_MIN_LEN + extraSpace;
  }

  // CREQ: Connect request (receiver -> source).
  private final static int CREQ_MIN_LEN =
     3+  1+4+   4+   4+   2+ 4+   1+1;
  // MMM 1 CCCC DDDD IIII PP iiii L T...
  // !   ! !    !    !    !  !    ! +- Topic string (L bytes long, one char per byte, not null terminated)
  // !   ! !    !    !    !  !    +--- Length of topic string (1-255) as unsigned 8-bit integer
  // !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
  // !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
  // !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
  // !   +---------------------------- Handshake message type (MSG_TYPE_CREQ)
  // +-------------------------------- Magic bytes indicating CT handshake protocol
  // The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
  // int, as MAGIC + MSG_TYPE_CREQ.

  // The smallest handshake message happens to be a CREQ.
  private final static int HANDSHAKE_MIN_LEN = CREQ_MIN_LEN;

  // Build a CREQ message using state information from the ct receiver connection object.
  static void makeCreq(ByteBuffer outMsg, LbmCtRcvConn rcvConn) {
    LbmCtRcv ctRcv = rcvConn.getCtRcv();
    LbmCt ct = ctRcv.getCt();

    outMsg.clear();
    outMsg.putInt(MAGIC + MSG_TYPE_CREQ);
    outMsg.putInt(ct.getCtId());
    outMsg.putInt(ct.getLocalDomainId());
    outMsg.putInt(ct.getLocalIpAddr());
    outMsg.putShort((short)ct.getLocalRequestPort());
    outMsg.putInt(rcvConn.getRcvConnId());
    outMsg.put((byte)ctRcv.getTopicStr().length());
    outMsg.put(ctRcv.getTopicStr().getBytes());

    outMsg.flip();  // Msg done, ready to be read out.
  }

  // CRSP: connect response (source -> receiver).
  //private final static int CRSP_MIN_LEN =
  // 3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4+   4;
  // MMM 2 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii LLLL M...
  // !   ! !    !    !    !  !    !    !    !    !  !    !    +- 0 or more bytes of metadata (pure binary).
  // !   ! !    !    !    !  !    !    !    !    !  !    +- SRC metadata length as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    !  +- SRC Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    +- SRC Port as 16-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    +- SRC IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
  // !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
  // !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
  // !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
  // !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
  // !   +---------------------------- Handshake message type (MSG_TYPE_CRSP)
  // +-------------------------------- Magic bytes indicating CT handshake protocol
  // The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
  // int, as MAGIC + MSG_TYPE_CRSP.

  // Build a CRSP message using state information from the ct receiver connection object.
  static void makeCrsp(ByteBuffer outMsg, LbmCtSrcConn srcConn) {
    LbmCtSrc ctSrc = srcConn.getCtSrc();
    LbmCt ct = ctSrc.getCt();

    outMsg.clear();
    outMsg.putInt(MAGIC + MSG_TYPE_CRSP);

    outMsg.putInt(srcConn.getRcvCtId());
    outMsg.putInt(srcConn.getRcvDomainId());
    outMsg.putInt(srcConn.getRcvIpAddr());
    outMsg.putShort((short)srcConn.getRcvRequestPort());
    outMsg.putInt(srcConn.getRcvConnId());

    outMsg.putInt(ct.getCtId());
    outMsg.putInt(ct.getLocalDomainId());
    outMsg.putInt(ct.getLocalIpAddr());
    outMsg.putShort((short)ct.getLocalRequestPort());
    outMsg.putInt(srcConn.getSrcConnId());

    ByteBuffer metadata = ct.getMetadata();
    int metaLen = metadata.remaining();
    outMsg.putInt(metaLen);
    if (metaLen > 0) {
      outMsg.put(metadata.array(), 0, metaLen);
    }

    outMsg.flip();  // message complete, ready to be read out.
  }

  // COK: Connect response (source->receiver).
  private final static int COK_MIN_LEN =
     3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4+   4+   4;
  // MMM 3 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii SSSS LLLL M...
  // !   ! !    !    !    !  !    !    !    !    !  !    !    !    +- 0 or more bytes of metadata (pure binary).
  // !   ! !    !    !    !  !    !    !    !    !  !    !    +- SRC metadata length as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    !  !    +- Receiver start sequence number as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    !  +- SRC Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    +- SRC Port as 16-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    +- SRC IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
  // !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
  // !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
  // !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
  // !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
  // !   +---------------------------- Handshake message type (MSG_TYPE_COK)
  // +-------------------------------- Magic bytes indicating CT handshake protocol
  // The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
  // int, as MAGIC + MSG_TYPE_COK.

  // Build a COK message using state information from the ct receiver connection object.
  static void makeCok(ByteBuffer outMsg, LbmCtRcvConn rcvConn) {
    LbmCtRcv ctRcv = rcvConn.getCtRcv();
    LbmCt ct = ctRcv.getCt();

    outMsg.clear();
    outMsg.putInt(MAGIC + MSG_TYPE_COK);

    outMsg.putInt(ct.getCtId());
    outMsg.putInt(ct.getLocalDomainId());
    outMsg.putInt(ct.getLocalIpAddr());
    outMsg.putShort((short)ct.getLocalRequestPort());
    outMsg.putInt(rcvConn.getRcvConnId());

    outMsg.putInt(rcvConn.getSrcCtId());
    outMsg.putInt(rcvConn.getSrcDomainId());
    outMsg.putInt(rcvConn.getSrcIpAddr());
    outMsg.putShort((short)rcvConn.getSrcRequestPort());
    outMsg.putInt(rcvConn.getSrcConnId());

    outMsg.putInt((int)rcvConn.getStartSequenceNum());

    ByteBuffer metadata = ct.getMetadata();
    outMsg.putInt(metadata.remaining());
    outMsg.put(metadata.array(), 0, metadata.remaining());

    outMsg.flip();  // message complete, ready to be read out.
  }

  // DREQ: Disconnect request (receiver->source).
  //private final static int DREQ_MIN_LEN =
  // 3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4;
  // MMM 4 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii
  // !   ! !    !    !    !  !    !    !    !    !  +- SRC Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    +- SRC Port as 16-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    +- SRC IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
  // !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
  // !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
  // !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
  // !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
  // !   +---------------------------- Handshake message type (MSG_TYPE_DREQ)
  // +-------------------------------- Magic bytes indicating CT handshake protocol
  // The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
  // int, as MAGIC + MSG_TYPE_DREQ.

  // Build a DREQ message using state information from the ct receiver connection object.
  static void makeDreq(ByteBuffer outMsg, LbmCtRcvConn rcvConn) {
    LbmCtRcv ctRcv = rcvConn.getCtRcv();
    LbmCt ct = ctRcv.getCt();

    outMsg.clear();
    outMsg.putInt(MAGIC + MSG_TYPE_DREQ);

    outMsg.putInt(ct.getCtId());
    outMsg.putInt(ct.getLocalDomainId());
    outMsg.putInt(ct.getLocalIpAddr());
    outMsg.putShort((short)ct.getLocalRequestPort());
    outMsg.putInt(rcvConn.getRcvConnId());

    outMsg.putInt(rcvConn.getSrcCtId());
    outMsg.putInt(rcvConn.getSrcDomainId());
    outMsg.putInt(rcvConn.getSrcIpAddr());
    outMsg.putShort((short)rcvConn.getSrcRequestPort());
    outMsg.putInt(rcvConn.getSrcConnId());

    outMsg.flip();  // message complete, ready to be read out.
  }

  // DRSP: connect response (source -> receiver).
  //private final static int DRSP_MIN_LEN =
  // 3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4+   4;
  // MMM 5 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii LLLL
  // !   ! !    !    !    !  !    !    !    !    !  !    +- SRC metadata length as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    !  +- SRC Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    +- SRC Port as 16-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    +- SRC IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
  // !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
  // !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
  // !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
  // !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
  // !   +---------------------------- Handshake message type (MSG_TYPE_DRSP)
  // +-------------------------------- Magic bytes indicating CT handshake protocol
  // The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
  // int, as MAGIC + MSG_TYPE_DRSP.

  // Build a DRSP message using state information from the ct receiver connection object.
  static void makeDrsp(ByteBuffer outMsg, LbmCtSrcConn srcConn) {
    LbmCtSrc ctSrc = srcConn.getCtSrc();
    LbmCt ct = ctSrc.getCt();

    outMsg.clear();
    outMsg.putInt(MAGIC + MSG_TYPE_DRSP);

    outMsg.putInt(srcConn.getRcvCtId());
    outMsg.putInt(srcConn.getRcvDomainId());
    outMsg.putInt(srcConn.getRcvIpAddr());
    outMsg.putShort((short)srcConn.getRcvRequestPort());
    outMsg.putInt(srcConn.getRcvConnId());

    outMsg.putInt(ct.getCtId());
    outMsg.putInt(ct.getLocalDomainId());
    outMsg.putInt(ct.getLocalIpAddr());
    outMsg.putShort((short)ct.getLocalRequestPort());
    outMsg.putInt(srcConn.getSrcConnId());

    outMsg.flip();  // message complete, ready to be read out.
  }

  // DOK: Disconnect OK (receiver->source)
  //private final static int DOK_MIN_LEN =
  // 3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4+   4;
  // MMM 6 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii SSSS
  // !   ! !    !    !    !  !    !    !    !    !  !    +- Receiver end sequence number as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    !  +- SRC Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    +- SRC Port as 16-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    +- SRC IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
  // !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
  // !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
  // !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
  // !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
  // !   +---------------------------- Handshake message type (MSG_TYPE_DOK)
  // +-------------------------------- Magic bytes indicating CT handshake protocol
  // The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
  // int, as MAGIC + MSG_TYPE_DOK.

  // Build a DOK message using state information from the ct receiver connection object.
  static void makeDok(ByteBuffer outMsg, LbmCtRcvConn rcvConn) {
    LbmCtRcv ctRcv = rcvConn.getCtRcv();
    LbmCt ct = ctRcv.getCt();

    outMsg.clear();
    outMsg.putInt(MAGIC + MSG_TYPE_DOK);

    outMsg.putInt(ct.getCtId());
    outMsg.putInt(ct.getLocalDomainId());
    outMsg.putInt(ct.getLocalIpAddr());
    outMsg.putShort((short)ct.getLocalRequestPort());
    outMsg.putInt(rcvConn.getRcvConnId());

    outMsg.putInt(rcvConn.getSrcCtId());
    outMsg.putInt(rcvConn.getSrcDomainId());
    outMsg.putInt(rcvConn.getSrcIpAddr());
    outMsg.putShort((short)rcvConn.getSrcRequestPort());
    outMsg.putInt(rcvConn.getSrcConnId());

    outMsg.putInt((int)rcvConn.getEndSequenceNum());

    ByteBuffer metadata = ct.getMetadata();
    outMsg.putInt(metadata.remaining());
    outMsg.put(metadata.array(), 0, metadata.remaining());

    outMsg.flip();  // message complete, ready to be read out.
  }

  // DFIN: connect response (source -> receiver).
  //private final static int DFIN_MIN_LEN =
  // 3+  1+4+   4+   4+   2+ 4+   4+   4+   4+   2+ 4+   4;
  // MMM 7 CCCC DDDD IIII PP iiii CCCC DDDD IIII PP iiii LLLL
  // !   ! !    !    !    !  !    !    !    !    !  !    +- SRC metadata length as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    !  +- SRC Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    !    +- SRC Port as 16-bit big-endian integer
  // !   ! !    !    !    !  !    !    !    +- SRC IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    !    !    !  !    !    +- Src Domain ID (binary, big-endian)
  // !   ! !    !    !    !  !    +--- Src CT identifier (binary random number, big-endian)
  // !   ! !    !    !    !  +-------- Rcv Conn ID as 32-bit big-endian integer
  // !   ! !    !    !    +----------- Rcv Port as 16-bit big-endian integer
  // !   ! !    !    +---------------- Rcv IP address as 32-bit big-endian integer (1st octet is MSB)
  // !   ! !    +--------------------- Rcv Domain ID (binary, big-endian)
  // !   ! +-------------------------- Rcv CT identifier (binary random number, big-endian)
  // !   +---------------------------- Handshake message type (MSG_TYPE_DFIN)
  // +-------------------------------- Magic bytes indicating CT handshake protocol
  // The 4-bytes consisting of 3 magic bytes and 1 "handshake message type" byte can be read as a single big-endian
  // int, as MAGIC + MSG_TYPE_DFIN.

  // Build a DRSP message using state information from the ct receiver connection object.
  static void makeDfin(ByteBuffer outMsg, LbmCtSrcConn srcConn) {
    LbmCtSrc ctSrc = srcConn.getCtSrc();
    LbmCt ct = ctSrc.getCt();

    outMsg.clear();
    outMsg.putInt(MAGIC + MSG_TYPE_DFIN);

    outMsg.putInt(srcConn.getRcvCtId());
    outMsg.putInt(srcConn.getRcvDomainId());
    outMsg.putInt(srcConn.getRcvIpAddr());
    outMsg.putShort((short)srcConn.getRcvRequestPort());
    outMsg.putInt(srcConn.getRcvConnId());

    outMsg.putInt(ct.getCtId());
    outMsg.putInt(ct.getLocalDomainId());
    outMsg.putInt(ct.getLocalIpAddr());
    outMsg.putShort((short)ct.getLocalRequestPort());
    outMsg.putInt(srcConn.getSrcConnId());

    outMsg.flip();  // message complete, ready to be read out.
  }
}
