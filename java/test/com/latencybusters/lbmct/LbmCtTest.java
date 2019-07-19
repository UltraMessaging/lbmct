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
import java.nio.*;
import java.util.concurrent.*;

import com.latencybusters.lbm.*;

import static org.hamcrest.CoreMatchers.*;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.*;
import org.junit.*;
import org.junit.runners.*;

@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class LbmCtTest
{
  // Globals usable by the tests.
  @SuppressWarnings("FieldCanBeLocal")  // Need to keep a reference to lbm so it isn't GCed.
  private static LBM lbm;
  private static LBMContext ctx1;
  private static LBMContext ctx2;
  private static Semaphore syncSem;
  private static String globalE;  // error indicator.
  private static String globalS;  // string used to indicate a callback has been called.
  private static final List<String> logStrings = new ArrayList<>(1024);
  private static final List<String> msgStrings = new ArrayList<>(1024);
  private static final String srcCbArg = "SrcClientd";
  private static final String src2CbArg = "Src2Clientd";
  private static final String rcvCbArg = "RcvClientd";
  private static final String rcv2CbArg = "Rcv2Clientd";
  private static final String srcConnCbArg = "SrcConnClientd";
  private static final String rcvConnCbArg = "RcvConnClientd";

  private String peer2str(LbmCtPeerInfo peerInfo) {
    StringBuilder sb = new StringBuilder(10000);
    int flags = peerInfo.getFlags();
    sb.append(" status=").append(peerInfo.getStatus()).append(", flags=0x");
    Formatter fmt = new Formatter(sb);
    fmt.format("%x", flags);

    try {
      if ((flags & LbmCtPeerInfo.FLAGS_SRC_METADATA) == LbmCtPeerInfo.FLAGS_SRC_METADATA) {
        ByteBuffer metadata = peerInfo.getSrcMetadata();
        if (metadata.remaining() == 0) {
          sb.append(", src_metadata=none");
        } else {
          sb.append(", src_metadata='");
          while (metadata.hasRemaining()) { sb.append((char)metadata.get()); }
          metadata.position(0);  // Restore data.
          sb.append('\'');
        }
      } else {
        sb.append(", no src_metadata");
      }

      if ((flags & LbmCtPeerInfo.FLAGS_RCV_METADATA) == LbmCtPeerInfo.FLAGS_RCV_METADATA) {
        ByteBuffer metadata = peerInfo.getRcvMetadata();
        if (metadata.remaining() == 0) {
          sb.append(", rcv_metadata=none");
        } else {
          sb.append(", rcv_metadata='");
          while (metadata.hasRemaining()) { sb.append((char)metadata.get()); }
          metadata.position(0);  // Restore data.
          sb.append('\'');
        }
      } else {
        sb.append(", no rcv_metadata");
      }

      if ((flags & LbmCtPeerInfo.FLAGS_RCV_SOURCE_STR) == LbmCtPeerInfo.FLAGS_RCV_SOURCE_STR) {
        sb.append(", rcv_source_name='").append(peerInfo.getRcvSourceStr()).append('\'');
      } else {
        sb.append(", no rcv_source_name");
      }

      if ((flags & LbmCtPeerInfo.FLAGS_RCV_START_SEQ_NUM) == LbmCtPeerInfo.FLAGS_RCV_START_SEQ_NUM) {
        sb.append(", rcv_start_seq_num=").append(peerInfo.getRcvStartSequenceNumber());
      } else {
        sb.append(", no rcv_start_seq_num");
      }

      if ((flags & LbmCtPeerInfo.FLAGS_RCV_END_SEQ_NUM) == LbmCtPeerInfo.FLAGS_RCV_END_SEQ_NUM) {
        sb.append(", rcv_end_seq_num=").append(peerInfo.getRcvEndSequenceNumber());
      } else {
        sb.append(", no rcv_end_seq_num");
      }
    } catch (Exception e) {
      LBMPubLog.pubLog(LBM.LOG_WARNING, "peer2str exception: " + LbmCt.exDetails(e) + "\n");
    }

    return sb.toString();
  }

  class TestLogger implements LBMLogging {
    public void LBMLog(int level, String message) {
      synchronized (logStrings) {
        switch (level) {
          case LBM.LOG_EMERG: logStrings.add("EMERG: (" + System.nanoTime() + ") " + message);  break;
          case LBM.LOG_ALERT: logStrings.add("ALERT: (" + System.nanoTime() + ") " + message);  break;
          case LBM.LOG_CRIT: logStrings.add("CRIT: (" + System.nanoTime() + ") " + message);  break;
          case LBM.LOG_WARNING: logStrings.add("WARNING: (" + System.nanoTime() + ") " + message);  break;
          case LBM.LOG_NOTICE: logStrings.add("NOTICE: (" + System.nanoTime() + ") " + message);  break;
          case LBM.LOG_INFO: logStrings.add("INFO: (" + System.nanoTime() + ") " + message);  break;
          default: logStrings.add("???: (" + System.nanoTime() + ") " + message + " (level=" + level);
        }
        int i = logStrings.size()-1;
        System.out.print("LOG[" + i + "]=" + logStrings.get(i));
      }
    }
  }

  private void recordMsg(LBMMessage umMsg, String cbArg, boolean handshakeMessage) {
    StringBuilder msgStr = new StringBuilder(4096);
    LbmCtRcvConn rcvConn = (LbmCtRcvConn) umMsg.sourceClientObject();
    String rcvConnCbArg = (String) rcvConn.getRcvConnCbArg();

    synchronized (msgStrings) {
      int i = msgStrings.size();

      msgStr.append('{').append(i).append("} (").append(System.nanoTime()).append(") test_rcv_cb: type=").append(umMsg.type());
      msgStr.append(", sqn=").append(umMsg.sequenceNumber());
      msgStr.append(", source='").append(umMsg.source());
      msgStr.append("', properties=").append((umMsg.properties() == null) ? "(nil)" : "(non-nil)");
      msgStr.append(", clientd='").append(cbArg);
      msgStr.append("', source_clientd='").append((rcvConnCbArg == null) ? "(null)" : rcvConnCbArg);
      msgStr.append("', handshakeMessage=").append(handshakeMessage);
      msgStr.append(", data:\n").append(hexDump(umMsg.data()));
      if (umMsg.properties() != null) {
        msgStr.append(", props=");
        int numProps = 0;
        for (LBMMessageProperty prop : umMsg.properties()) {
          numProps++;
          if (numProps > 1) {
            msgStr.append(',');
          }
          msgStr.append(prop.key()).append(':');
          switch(prop.type()) {
            case LBM.MSG_PROPERTY_INT:
              try {
                msgStr.append(prop.getInteger());
              } catch (Exception e) { LBMPubLog.pubLog(LBM.LOG_WARNING, "recordMsg: int prop exception\n"); }
              break;
            case LBM.MSG_PROPERTY_STRING:
              try {
                msgStr.append(prop.getString());
              } catch (Exception e) { LBMPubLog.pubLog(LBM.LOG_WARNING, "recordMsg: str prop exception\n"); }
              break;
            default:
              try {
                msgStr.append("type").append(prop.type());
              } catch (Exception e) { LBMPubLog.pubLog(LBM.LOG_WARNING, "recordMsg: str prop exception\n"); }
          }
        }
        msgStr.append('.');
      }
      msgStr.append('\n');

      msgStrings.add(msgStr.toString());
      System.out.print(msgStrings.get(i));
    }
  }

  // Code executed before the tests run.
  @Test
  public void t0000() throws Exception {
    System.out.println("Test t0000");
    System.out.println("UM version" + LBM.version());
    syncSem = new Semaphore(0);
    TestLogger testLogger = new TestLogger();
    lbm = new LBM();
    lbm.setLogger(testLogger);
    LBM.setConfiguration("test.cfg");

    LBMContextAttributes ctxAttr = new LBMContextAttributes();
    ctxAttr.setValue("response_tcp_nodelay", "1");
    ctx1 = new LBMContext(ctxAttr);
    ctx2 = new LBMContext(ctxAttr);
    ctxAttr.dispose();

    LbmCtConfig config = new LbmCtConfig();
    ByteBuffer metadata = ByteBuffer.allocate(100);
    metadata.put("MyMeta".getBytes());
    LbmCt ct1 = new LbmCt();
    // Oops, forgetting to flip!  Verify exception.
    try {
      ct1.start(ctx1, config, metadata);
      fail("Expected exception");
    } catch (Exception e) {
      assertThat(e.toString(), containsString("missing flip?"));
    }
    metadata.flip();
    ct1.start(ctx1, config, metadata);

    ct1.stop();
  }  // t0000()

  // Test handshake message object.
  @Test
  public void t0003() throws Exception {
    System.out.println("Test t0003");
    logStrings.clear();
    LbmCtHandshakeParser msgParser = new LbmCtHandshakeParser(100);
    ByteBuffer msgBB = ByteBuffer.allocate(300);

    msgBB.clear();
    msgBB.putInt(1);
    msgBB.flip();
    try {
      msgParser.parse(msgBB);
      fail("Expected exception");
    } catch (Exception e) {
      assertThat(e.toString(), containsString("Bad handshake len"));
    }

    msgBB.clear();
    int i;
    for (i = msgBB.position(); i < msgBB.capacity(); i++) {
      msgBB.put((byte)i);
    }
    msgBB.flip();
    try {
      msgParser.parse(msgBB);
      fail("Expected exception");
    } catch (Exception e) {
      assertThat(e.toString(), containsString("Bad handshake magic bytes"));
    }

    msgBB.clear();
    msgBB.putInt(LbmCtHandshakeParser.MAGIC + 99);
    for (i = msgBB.position(); i < msgBB.capacity(); i++) {
      msgBB.put((byte)i);
    }
    msgBB.flip();
    try {
      msgParser.parse(msgBB);
      fail("Expected exception");
    } catch (Exception e) {
      assertThat(e.toString(), containsString("Bad handshake command type"));
    }

    msgBB.clear();
    msgBB.putInt(LbmCtHandshakeParser.MAGIC + LbmCtHandshakeParser.MSG_TYPE_CREQ);
    msgBB.putInt(101);  // ctId
    msgBB.putInt(102);  // domainId
    msgBB.putInt(103);  // ipAddr
    msgBB.putShort((short)104);  // requestPort
    msgBB.putInt(105);  // ConnId
    msgBB.put((byte)0);  // topic string len
    // Omit topic string (not valid since need at least 1 char).
    msgBB.flip();
    try {
      msgParser.parse(msgBB);
      fail("Expected exception");
    } catch (Exception e) {
      assertThat(e.toString(), containsString("Bad handshake len"));
    }

    msgBB.clear();
    msgBB.putInt(LbmCtHandshakeParser.MAGIC + LbmCtHandshakeParser.MSG_TYPE_CREQ);
    msgBB.putInt(101);  // ctId
    msgBB.putInt(102);  // domainId
    msgBB.putInt(103);  // ipAddr
    msgBB.putShort((short)104);  // requestPort
    msgBB.putInt(105);  // ConnId
    msgBB.put((byte)2);  // topic string len
    msgBB.put("b".getBytes());  // len too big.
    msgBB.flip();
    try {
      msgParser.parse(msgBB);
      fail("Expected exception");
    } catch (Exception e) {
      assertThat(e.toString(), containsString("Bad topic string len"));
    }

    msgBB.clear();
    msgBB.putInt(LbmCtHandshakeParser.MAGIC + LbmCtHandshakeParser.MSG_TYPE_CREQ);
    msgBB.putInt(101);  // ctId
    msgBB.putInt(102);  // domainId
    msgBB.putInt(103);  // ipAddr
    msgBB.putShort((short)104);  // requestPort
    msgBB.putInt(105);  // ConnId
    msgBB.put((byte)0);  // topic string len
    msgBB.put("b".getBytes());  // len byte too small.
    msgBB.flip();
    try {
      msgParser.parse(msgBB);
      fail("Expected exception");
    } catch (Exception e) {
      assertThat(e.toString(), containsString("Bad topic string len"));
    }

    msgBB.clear();
    msgBB.putInt(LbmCtHandshakeParser.MAGIC + LbmCtHandshakeParser.MSG_TYPE_CREQ);
    msgBB.putInt(101);  // ctId
    msgBB.putInt(102);  // domainId
    msgBB.putInt(103);  // ipAddr
    msgBB.putShort((short)104);  // requestPort
    msgBB.putInt(105);  // connId
    msgBB.put((byte)1);  // topic string len
    msgBB.put("b".getBytes());
    msgBB.flip();
    msgParser.parse(msgBB);
    assertThat(msgParser.getRcvCtId(), is(101));
    assertThat(msgParser.getRcvDomainId(), is(102));
    assertThat(msgParser.getRcvIpAddr(), is(103));
    assertThat(msgParser.getRcvRequestPort(), is(104));
    assertThat(msgParser.getRcvConnId(), is(105));
    assertThat(msgParser.getTopicStr(), is("b"));
    assertThat(msgParser.getRcvConnKey(), is("101,102:0.0.0.103:104,105"));

    msgBB.clear();
    msgBB.putInt(LbmCtHandshakeParser.MAGIC + LbmCtHandshakeParser.MSG_TYPE_CREQ);
    msgBB.putInt(101);  // ctId
    msgBB.putInt(102);  // domainId
    msgBB.putInt(103);  // ipAddr
    msgBB.putShort((short)104);  // requestPort
    msgBB.putInt(105);  // ConnId
    msgBB.put((byte)128);  // topic string len
    msgBB.put("12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678".getBytes());
    msgBB.flip();
    msgParser.parse(msgBB);
    assertThat(msgParser.getRcvCtId(), is(101));
    assertThat(msgParser.getRcvDomainId(), is(102));
    assertThat(msgParser.getRcvIpAddr(), is(103));
    assertThat(msgParser.getRcvRequestPort(), is(104));
    assertThat(msgParser.getTopicStr(), is("12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678"));

    assertThat(logStrings.size(), is(0));
  }


  // Test config object.
  @Test
  public void t0007() {
    System.out.println("Test t0007");
    logStrings.clear();
    LbmCtConfig config = new LbmCtConfig();

    assertThat(config.getTestBits(), is(LbmCtConfig.CT_CONFIG_DEFAULT_TEST_BITS));
    config.setTestBits(LbmCtConfig.CT_CONFIG_DEFAULT_TEST_BITS+1);
    assertThat(config.getTestBits(), is(LbmCtConfig.CT_CONFIG_DEFAULT_TEST_BITS+1));

    assertThat(config.getDomainId(), is(LbmCtConfig.CT_CONFIG_DEFAULT_DOMAIN_ID));
    config.setDomainId(LbmCtConfig.CT_CONFIG_DEFAULT_DOMAIN_ID+1);
    assertThat(config.getDomainId(), is(LbmCtConfig.CT_CONFIG_DEFAULT_DOMAIN_ID+1));

    assertThat(config.getDelayCreq(), is(LbmCtConfig.CT_CONFIG_DEFAULT_DELAY_CREQ));
    config.setDelayCreq(LbmCtConfig.CT_CONFIG_DEFAULT_DELAY_CREQ+1);
    assertThat(config.getDelayCreq(), is(LbmCtConfig.CT_CONFIG_DEFAULT_DELAY_CREQ+1));

    assertThat(config.getRetryIvl(), is(LbmCtConfig.CT_CONFIG_DEFAULT_RETRY_IVL));
    config.setRetryIvl(LbmCtConfig.CT_CONFIG_DEFAULT_RETRY_IVL+1);
    assertThat(config.getRetryIvl(), is(LbmCtConfig.CT_CONFIG_DEFAULT_RETRY_IVL+1));

    assertThat(config.getMaxTries(), is(LbmCtConfig.CT_CONFIG_DEFAULT_MAX_TRIES));
    config.setMaxTries(LbmCtConfig.CT_CONFIG_DEFAULT_MAX_TRIES+1);
    assertThat(config.getMaxTries(), is(LbmCtConfig.CT_CONFIG_DEFAULT_MAX_TRIES+1));

    assertThat(config.getPreDelivery(), is(LbmCtConfig.CT_CONFIG_DEFAULT_PRE_DELIVERY));
    config.setPreDelivery(LbmCtConfig.CT_CONFIG_DEFAULT_PRE_DELIVERY+1);
    assertThat(config.getPreDelivery(), is(LbmCtConfig.CT_CONFIG_DEFAULT_PRE_DELIVERY+1));

    assertThat(logStrings.size(), is(0));
  }

  // Verify the Test cmd can make a round-trip.  Test both exception and non-exception cases.
  @Test
  public void t0010() throws Exception {
    System.out.println("Test t0010");
    logStrings.clear();
    // Uncomment the following line to see an un-caught exception:
    //ct.ctTest(null);

    LbmCtConfig config = new LbmCtConfig();
    config.setMaxTries(3);
    ByteBuffer metadata = ByteBuffer.allocate(100);
    metadata.put("MyMeta".getBytes());
    LbmCt ct1 = new LbmCt();
    metadata.flip();
    ct1.start(ctx1, config, metadata);

    try {
      ct1.ctTest("Test Exception", -1);
      fail("Expected exception");
    } catch (Exception e) {
      assertThat(e.toString(), containsString("cmdTest: Test Exception"));
    }

    int i;
    for (i = 0; i < 2; i++) {
      ct1.ctTest("Testing..." + i, 0);

      Thread.sleep(500);
    }

    assertThat(logStrings.get(0), containsString("Testing...0"));
    assertThat(logStrings.get(1), containsString("Testing...1"));
    assertThat(logStrings.size(), is(2));

    ct1.stop();
  }  // t0010()

  // Experiment with various Java features.
  private static ByteBuffer t0013GlobalBB = null;

  @Test
  public void t0013() throws Exception {
    System.out.println("Test t0013");
    logStrings.clear();
    // This is just an experiment to see how the Java API works.
    t0013GlobalBB = ByteBuffer.allocate(100);

    LBMTopic topicObj = ctx1.lookupTopic("t0013", null);
    LBMReceiverCallback test0003RcvCb = new Test0013RcvCb();
    LBMReceiver umRcv = new LBMReceiver(ctx1, topicObj, test0003RcvCb, null);

    topicObj = ctx1.allocTopic("t0013", null);
    LBMSource umSrc = new LBMSource(ctx1, topicObj);
    Thread.sleep(150);  // Let TR happen.

    // Use the message prop created by an LbmCt object.
    LbmCt ct1 = new LbmCt();  ct1.start(ctx1, new LbmCtConfig(), null);

    // Send 3 messages with implicit batching so that they are in the same receive buffer.

    umSrc.send("Hello1".getBytes(), 6, 0);

    umSrc.send("Hello2".getBytes(), 6, 0, ct1.getSrcExInfo());

    umSrc.send("Hello3".getBytes(), 6, LBM.MSG_FLUSH);

    Thread.sleep(100);
    umSrc.close();
    Thread.sleep(100);
    umRcv.close();
    Thread.sleep(100);

    ct1.stop();

    assertThat(logStrings.get(0), containsString("type=20, position=0, limit=0"));
    assertThat(logStrings.get(1), containsString("type=0, position=0, limit=6"));
    assertThat(logStrings.get(2), containsString("t0013GlobalBB.toString=java.nio.HeapByteBuffer[pos=0 lim=6 cap=100], sb.toString=Hello1"));
    assertThat(logStrings.get(3), containsString("Test0013RcvCb: dataLength=6, position=6, limit=6"));
    assertThat(logStrings.get(4), containsString("type=0, position=0, limit=6"));
    assertThat(logStrings.get(5), containsString("t0013GlobalBB.toString=java.nio.HeapByteBuffer[pos=0 lim=6 cap=100], sb.toString=Hello2"));
    assertThat(logStrings.get(6), containsString("Test0013RcvCb: dataLength=6, position=6, limit=6"));
    assertThat(logStrings.get(7), containsString("type=0, position=0, limit=6"));
    assertThat(logStrings.get(8), containsString("t0013GlobalBB.toString=java.nio.HeapByteBuffer[pos=0 lim=6 cap=100], sb.toString=Hello3"));
    assertThat(logStrings.get(9), containsString("Test0013RcvCb: dataLength=6, position=6, limit=6"));
    assertThat(logStrings.get(10), containsString("type=1, position=0, limit=0"));
    assertThat(logStrings.size(), is(11));
  }

  private static class Test0013RcvCb implements LBMReceiverCallback {
    public int onReceive(Object cbArgs, LBMMessage umMsg) {
      LBMPubLog.pubLog(LBM.LOG_INFO, "Test0013RcvCb: type=" + umMsg.type() + ", position=" + umMsg.dataBuffer().position() + ", limit=" + umMsg.dataBuffer().limit() + "\n");
      if (umMsg.type() == LBM.MSG_DATA) {
        t0013GlobalBB.clear();

        ByteBuffer b = umMsg.dataBuffer();
        b.get(t0013GlobalBB.array(), b.position(), (int)umMsg.dataLength());
        t0013GlobalBB.limit((int)umMsg.dataLength());

        StringBuilder sb = new StringBuilder(99);
        int i;
        for (i = 0; i < umMsg.dataLength(); i++){
          try {
            char c = (char) t0013GlobalBB.get();
            sb.append(c);
          } catch (Exception e) {
            LBMPubLog.pubLog(LBM.LOG_WARNING, "globalB.get(): " + e.toString());
            i = 9999;
          }
        }
        t0013GlobalBB.position(0);
        LBMPubLog.pubLog(LBM.LOG_INFO, "Test0013RcvCb: t0013GlobalBB.toString=" + t0013GlobalBB.toString() + ", sb" + ".toString=" + sb.toString() + "\n");
        LBMPubLog.pubLog(LBM.LOG_INFO, "Test0013RcvCb: dataLength=" + umMsg.dataLength() + ", position=" + umMsg.dataBuffer().position() + ", limit=" + umMsg.dataBuffer().limit() + "\n");
      }

      umMsg.dispose();
      return 0;
    }
  }

  private static byte[] t0015GlobalBA;
  private static StringBuilder t0015GlobalSB;

  @Test
  public void t0015() throws Exception {
    System.out.println("Test t0015");
    logStrings.clear();
    // This is just an experiment to see how the Java API works.
    t0015GlobalBA = new byte[100];
    t0015GlobalSB = new StringBuilder(256);

    LBMTopic topicObj = ctx1.lookupTopic("t0015", null);
    LBMReceiverCallback test0005RcvCb = new Test0015RcvCb();
    LBMReceiver umRcv = new LBMReceiver(ctx1, topicObj, test0005RcvCb, null);

    topicObj = ctx1.allocTopic("t0015", null);
    LBMSource umSrc = new LBMSource(ctx1, topicObj);
    Thread.sleep(150);  // Let TR happen.

    byte[] ba = new byte[100];
    ByteBuffer bb = ByteBuffer.wrap(ba);

    bb.putInt(LbmCtHandshakeParser.MAGIC);
    String s = "Test0015 string";
    bb.put((byte)s.length());
    bb.put(s.getBytes());
    bb.put((byte)'0');

    umSrc.send(bb.array(), bb.position(), LBM.MSG_FLUSH);
    Thread.sleep(100);
    umSrc.close();
    umRcv.close();

    assertThat(logStrings.size(), is(0));
  }

  private static class Test0015RcvCb implements LBMReceiverCallback {
    public int onReceive(Object cbArgs, LBMMessage umMsg) {
      System.out.println("type=" + umMsg.type() + ", position=" + umMsg.dataBuffer().position() + ", limit=" + umMsg.dataBuffer().limit());
      if (umMsg.type() == LBM.MSG_DATA) {
        ByteBuffer b = umMsg.dataBuffer();
        b.get(t0015GlobalBA, b.position(), (int) umMsg.dataLength());
        String s = hexDump(t0015GlobalBA);

        ByteBuffer c = ByteBuffer.wrap(t0015GlobalBA);  c.limit((int) umMsg.dataLength());
        int magic = c.getInt();
        byte l = c.get();
        while (l > 0) {
          t0015GlobalSB.append((char)c.get());
          l--;
        }

        System.out.println("dataLength=" + umMsg.dataLength() + ", c=" + c.toString());
        System.out.println("magic=" + magic + ", l=" + l + ", sb.toString=" + t0015GlobalSB.toString() + ", s=" + s);
      }

      umMsg.dispose();
      return 0;
    }
  }

  // Verify multiple CtSrc objects can be created and then stopped in a different order.
  @Test
  public void t0020() throws Exception {
    System.out.println("Test t0020");
    logStrings.clear();

    LbmCtConfig config = new LbmCtConfig();
    config.setMaxTries(3);
    ByteBuffer metadata = ByteBuffer.allocate(100);
    metadata.put("MyMeta".getBytes());
    LbmCt ct1 = new LbmCt();
    metadata.flip();
    ct1.start(ctx1, config, metadata);

    LbmCtSrc ctSrcA = new LbmCtSrc();
    ctSrcA.start(ct1, "abc", null, null, null, null, "ctSrcA cbArg");
    LbmCtSrc ctSrcB = new LbmCtSrc();
    ctSrcB.start(ct1, "xyz", null, null, null, null, "ctSrcB cbArg");
    LbmCtSrc ctSrcC = new LbmCtSrc();
    ctSrcC.start(ct1, "123", null, null, null, null, "ctSrcC cbArg");

    ctSrcB.stop();
    ctSrcA.stop();
    ctSrcC.stop();

    Thread.sleep(100);
    ct1.stop();

    assertThat(logStrings.size(), is(0));
  }

  // Verify multiple CtSrc objects can be created and then stopped in another different order.
  @Test
  public void t0022() throws Exception {
    System.out.println("Test t0022");
    logStrings.clear();

    LbmCtConfig config = new LbmCtConfig();
    config.setMaxTries(3);
    ByteBuffer metadata = ByteBuffer.allocate(100);
    metadata.put("MyMeta".getBytes());
    LbmCt ct1 = new LbmCt();
    metadata.flip();
    ct1.start(ctx1, config, metadata);

    LbmCtSrc ctSrcA = new LbmCtSrc();
    ctSrcA.start(ct1, "abc", null, null, null, null, "ctSrcA cbArg");
    LbmCtSrc ctSrcB = new LbmCtSrc();
    ctSrcB.start(ct1, "xyz", null, null, null, null, "ctSrcB cbArg");
    LbmCtSrc ctSrcC = new LbmCtSrc();
    ctSrcC.start(ct1, "123", null, null, null, null, "ctSrcC cbArg");

    ctSrcB.stop();
    ctSrcC.stop();
    ctSrcA.stop();

    Thread.sleep(100);
    ct1.stop();

    assertThat(logStrings.size(), is(0));
  }

  // Verify multiple CtRcv objects can be created and then stopped in a different order.
  @Test
  public void t0030() throws Exception {
    System.out.println("Test t0030");
    logStrings.clear();

    LbmCtConfig config = new LbmCtConfig();
    config.setMaxTries(3);
    ByteBuffer metadata = ByteBuffer.allocate(100);
    metadata.put("MyMeta".getBytes());
    LbmCt ct1 = new LbmCt();
    metadata.flip();
    ct1.start(ctx1, config, metadata);

    LbmCtRcv ctRcvA = new LbmCtRcv();
    ctRcvA.start(ct1, "abc", null, null, null, null, "ctRcvA cbArg");

    LbmCtRcv ctRcvB = new LbmCtRcv();
    ctRcvB.start(ct1, "xyz", null, null, null, null, "ctRcvB cbArg");

    LbmCtRcv ctRcvC = new LbmCtRcv();
    ctRcvC.start(ct1, "123", null, null, null, null, "ctRcvC cbArg");

    ctRcvB.stop();
    ctRcvA.stop();
    ctRcvC.stop();

    Thread.sleep(100);
    ct1.stop();

    assertThat(logStrings.size(), is(0));
  }

  // Verify multiple CtRcv objects can be created and then stopped in another different order.
  @Test
  public void t0032() throws Exception {
    System.out.println("Test t0032");
    logStrings.clear();

    LbmCtConfig config = new LbmCtConfig();
    config.setMaxTries(3);
    ByteBuffer metadata = ByteBuffer.allocate(100);
    metadata.put("MyMeta".getBytes());
    LbmCt ct1 = new LbmCt();
    metadata.flip();
    ct1.start(ctx1, config, metadata);

    LbmCtRcv ctRcvA = new LbmCtRcv();
    ctRcvA.start(ct1, "abc", null, null, null, null, "CtRcvA cbArg");
    LbmCtRcv ctRcvB = new LbmCtRcv();
    ctRcvB.start(ct1, "xyz", null, null, null, null, "CtRcvB cbArg");
    LbmCtRcv ctRcvC = new LbmCtRcv();
    ctRcvC.start(ct1, "123", null, null, null, null, "CtRcvC cbArg");

    ctRcvB.stop();
    ctRcvC.stop();
    ctRcvA.stop();

    Thread.sleep(100);
    ct1.stop();

    assertThat(logStrings.size(), is(0));
  }

  // Try to connect a CtRcv to a CtSrc.
  @Test
  public void t0040() throws Exception {
    System.out.println("Test t0040");
    logStrings.clear();

    LbmCtConfig config = new LbmCtConfig();
    config.setMaxTries(3);
    ByteBuffer metadata = ByteBuffer.allocate(100);
    metadata.put("MyMeta".getBytes());
    LbmCt ct1 = new LbmCt();
    metadata.flip();
    ct1.start(ctx1, config, metadata);

    LBMReceiverCallback rcvCb = new Test0040RcvCb();
    Test0040RcvConnCreateCb rcvCreateCb = new Test0040RcvConnCreateCb();
    Test0040RcvConnDeleteCb rcvDeleteCb = new Test0040RcvConnDeleteCb();
    LbmCtRcv ctRcvA = new LbmCtRcv();
    ctRcvA.start(ct1, "abc", null, rcvCb, rcvCreateCb, rcvDeleteCb, "ctRcvA cbArg");
    LbmCtRcv ctRcvB = new LbmCtRcv();
    ctRcvB.start(ct1, "xyz", null, null, null, null, "ctRcvB cbArg");

    Test0040SrcConnCreateCb srcCreateCb = new Test0040SrcConnCreateCb();
    Test0040SrcConnDeleteCb srcDeleteCb = new Test0040SrcConnDeleteCb();
    LbmCtSrc ctSrcA = new LbmCtSrc();
    ctSrcA.start(ct1, "abc", null, null, srcCreateCb, srcDeleteCb, "ctSrcA cbArg");
    Thread.sleep(1000);

    LBMSource umSrc = ctSrcA.getUmSrc();
    umSrc.send("msg0040".getBytes(), 7, LBM.MSG_FLUSH);
    Thread.sleep(1000);

    // Verify exception if try to stop a ct that has srcs/rcvs.
    try {
      ct1.stop();
      fail("Expected exception");
    } catch (Exception e) {
      assertThat(e.toString(), containsString("Must delete sources"));
    }

    ctRcvA.stop();
    Thread.sleep(100);
    ctRcvB.stop();
    Thread.sleep(200);
    ctSrcA.stop();
    Thread.sleep(100);

    Thread.sleep(100);
    ct1.stop();

    assertThat(logStrings.get(0), containsString("Test0040RcvConnCreateCb::onRcvConnCreate: rcvCbArg=ctRcvA cbArg,"));
    assertThat(logStrings.get(0), containsString("0000: 4d 79 4d 65 74 61  "));
    assertThat(logStrings.get(1), containsString("Test0040RcvCb::onReceive: type=0, topic='abc', sqn=0, position=50, limit=50, prop=com"));
    assertThat(logStrings.get(1), containsString("rcvCbArg=ctRcvA cbArg, rcvConnCbArg=Test0040RcvConnCreateCb rtn,"));
    assertThat(logStrings.get(1), containsString("0040: 00 00 00 06 4d 79 4d 65 "));
    assertThat(logStrings.get(2), containsString("Test0040SrcConnCreateCb::onSrcConnCreate: srcCbArg=ctSrcA cbArg"));
    assertThat(logStrings.get(2), containsString("0000: 4d 79 4d 65 74 61  "));
    assertThat(logStrings.get(3), containsString("Test0040RcvCb::onReceive: type=0, topic='abc', sqn=1, position=7, limit=7, prop=null"));
    assertThat(logStrings.get(3), containsString("rcvCbArg=ctRcvA cbArg, rcvConnCbArg=Test0040RcvConnCreateCb rtn,"));
    assertThat(logStrings.get(3), containsString("0000: 6d 73 67 30 30 34 30  "));
    assertThat(logStrings.get(4), containsString("Test0040RcvCb::onReceive: type=0, topic='abc', sqn=2, position=40, limit=40, prop=com"));
    assertThat(logStrings.get(4), containsString("rcvCbArg=ctRcvA cbArg, rcvConnCbArg=Test0040RcvConnCreateCb rtn,"));
    assertThat(logStrings.get(5), containsString("Test0040RcvConnDeleteCb::onRcvConnDelete: rcvCbArg=ctRcvA cbArg, " +
        "rcvConnCbArg=Test0040RcvConnCreateCb rtn,"));
    assertThat(logStrings.get(6), containsString("Test0040SrcConnDeleteCb::onSrcConnDelete: rcvCbArg=ctSrcA cbArg, " +
        "srcConnCbArg=Test0040SrcConnCreateCb rtn,"));

    assertThat(logStrings.size(), is(7));
  }

  class Test0040RcvCb implements LBMReceiverCallback {
    @Override
    public int onReceive(Object cbArg, LBMMessage umMsg) {
      String rcvCbArg = (String)cbArg;
      LbmCtRcvConn rcvConn = (LbmCtRcvConn) umMsg.sourceClientObject();
      String rcvConnCbArg = (String) rcvConn.getRcvConnCbArg();
      byte[] msgData = new byte[(int)umMsg.dataLength()];
      umMsg.dataBuffer().get(msgData, 0, (int)umMsg.dataLength());
      boolean handshakeMessage = rcvConn.isHandshakeMessage();

      LBMPubLog.pubLog(LBM.LOG_INFO, "Test0040RcvCb::onReceive: type=" + umMsg.type() +
          ", topic='" + umMsg.topicName() + "', sqn=" + umMsg.sequenceNumber() +
          ", position=" + umMsg.dataBuffer().position() + ", limit=" + umMsg.dataBuffer().limit() +
          ", prop=" + umMsg.properties() + ", rcvCbArg=" + rcvCbArg + ", rcvConnCbArg=" + rcvConnCbArg +
          ", handshakeMessage=" + handshakeMessage + ", msg=\n" + hexDump(msgData));

      if (!handshakeMessage) {
        umMsg.dispose();
      }
      return 0;
    }
  }

  class Test0040RcvConnCreateCb implements LbmCtRcvConnCreateCallback {
    @Override
    public Object onRcvConnCreate(LbmCtRcv ctRcv, LbmCtPeerInfo peerInfo, Object cbArg) {
      String rcvCbArg = (String)cbArg;
      try {
        LBMPubLog.pubLog(LBM.LOG_INFO, "Test0040RcvConnCreateCb::onRcvConnCreate: rcvCbArg=" + rcvCbArg +
            ", metadata=\n" + hexDump(peerInfo.getSrcMetadata().array()));
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_INFO, "Test0040RcvConnCreateCb::onRcvConnCreate: e=" + e.toString() + "\n");
      }
      return "Test0040RcvConnCreateCb rtn";
    }
  }

  class Test0040RcvConnDeleteCb implements LbmCtRcvConnDeleteCallback {
    @Override
    public void onRcvConnDelete(LbmCtRcv ctRcv, LbmCtPeerInfo peerInfo, Object cbArg, Object connCbArg) {
      String rcvCbArg = (String)cbArg;
      String rcvConnCbArg = (String)connCbArg;
      try {
        LBMPubLog.pubLog(LBM.LOG_INFO, "Test0040RcvConnDeleteCb::onRcvConnDelete: rcvCbArg=" + rcvCbArg +
            ", rcvConnCbArg=" + rcvConnCbArg + ", metadata=\n" + hexDump(peerInfo.getSrcMetadata().array()));
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_INFO, "Test0040RcvConnDeleteCb::onRcvConnDelete: e=" + e.toString() + "\n");
      }
    }
  }

  class Test0040SrcConnCreateCb implements LbmCtSrcConnCreateCallback {
    @Override
    public Object onSrcConnCreate(LbmCtSrc ctSrc, LbmCtPeerInfo peerInfo, Object cbArg) {
      String srcCbArg = (String)cbArg;
      try {
        LBMPubLog.pubLog(LBM.LOG_INFO, "Test0040SrcConnCreateCb::onSrcConnCreate: srcCbArg=" + srcCbArg +
            ", metadata=\n" + hexDump(peerInfo.getSrcMetadata().array(), 0));
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_INFO, "Test0040SrcConnCreateCb::onSrcConnCreate: e=" + e.toString() + "\n");
      }
      return "Test0040SrcConnCreateCb rtn";
    }
  }

  class Test0040SrcConnDeleteCb implements LbmCtSrcConnDeleteCallback {
    @Override
    public void onSrcConnDelete(LbmCtSrc ctSrc, LbmCtPeerInfo peerInfo, Object cbArg, Object connCbArg) {
      String srcCbArg = (String)cbArg;
      String srcConnCbArg = (String)connCbArg;
      try {
        LBMPubLog.pubLog(LBM.LOG_INFO, "Test0040SrcConnDeleteCb::onSrcConnDelete: rcvCbArg=" + srcCbArg +
            ", srcConnCbArg=" + srcConnCbArg + ", metadata=\n" + hexDump(peerInfo.getSrcMetadata().array(), 0));
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_INFO, "Test0040SrcConnDeleteCb::onSrcConnDelete: e=" + e.toString() + "\n");
      }
    }
  }

  // Test the tmr system
  private static int t0052GlobalId;  // timer ID, used to let callback verify correct timer ID.

  @Test
  public void t0052() throws Exception {
    System.out.println("Test t0052");
    logStrings.clear();
    T0052TmrCb1 cb1 = new T0052TmrCb1("s cb1");
    Tmr tmr1 = new Tmr(ctx1);

    // T=0: start test
    globalE = null;
    globalS = null;
    t0052GlobalId = tmr1.schedule(cb1, "a cb1", 500);
    assertThat(t0052GlobalId, is(1));
    Thread.sleep(300);

    // T=.3: make sure timer hasn't fired yet.
    assertThat(globalS, is(nullValue()));  assertThat(globalE, is(nullValue()));
    Thread.sleep(400);
    // At T=.5: timer fires.

    // T=.7: make sure timer fired.  Set up for next timer run (will expire at T=1.2).
    assertThat(globalS, containsString("TmrCb1::onExpire s='s cb1', a='a cb1'"));  assertThat(globalE, is(nullValue()));
    globalS = null;
    t0052GlobalId = tmr1.schedule(cb1, "a2 cb1", 500);
    assertThat(t0052GlobalId, is(2));
    // Make sure trying to schedule a running timer throws exception.
    try {
      t0052GlobalId = tmr1.schedule(cb1, "A2 cb1", 500);
      fail("Expected exception");
    } catch (Exception e) {
      assertThat(e.toString(), containsString("Not idle"));
    }
    assertThat(t0052GlobalId, is(2));
    Thread.sleep(300);

    // T=1.0: make sure timer hasn't run yet.
    assertThat(globalS, is(nullValue()));  assertThat(globalE, is(nullValue()));
    Thread.sleep(400);
    // At T=1.2 the timer fires.

    // T=1.4: make sure the timer fired.
    assertThat(globalS, containsString("TmrCb1::onExpire s='s cb1', a='a2 cb1'"));  assertThat(globalE, is(nullValue()));
    // No timer running; cancel should do nothing.
    tmr1.cancelSync();
    // Cancel a running timer.  This timer scheduled to fire at T=1.9
    globalS = null;
    t0052GlobalId = tmr1.schedule(cb1, "a3 cb1", 500);
    Thread.sleep(300);

    // T=1.7: make sure timer hasn't fired yet.  Cancel it.
    assertThat(globalS, is(nullValue()));  assertThat(globalE, is(nullValue()));
    tmr1.cancelSync();
    Thread.sleep(400);

    // T=2.1: past scheduled time; make sure it hasn't run (was canceled).
    assertThat(globalS, is(nullValue()));  assertThat(globalE, is(nullValue()));

    // Now just do a bunch of zero-duration schedules/cancels in a tight loop
    // Some of them might run, some might not.  Really just trying to abuse it.
    int i;
    t0052GlobalId = -1;  // Suppress checking ID inside callback.
    for (i = 0; i < 50; i++) {
      tmr1.schedule(cb1, "i=" + i, 0);
      tmr1.cancelSync();
    }
    Thread.sleep(200);  // Wait for things to settle down.

    assertThat(globalE, is(nullValue()));

    assertThat(logStrings.size(), is(0));
  }

  private static class T0052TmrCb1 implements TmrCallback {
    String s;
    private T0052TmrCb1(String s) {
      this.s = s;
    }

    public void onExpire(Tmr tmr, LBMContext ctx, Object cbArg) {
      String a = (String)cbArg;
      if (t0052GlobalId == -1 || tmr.getId() == t0052GlobalId) {
        globalS = "TmrCb1::onExpire s='" + s + "', a='" + a + "'\n";
      } else {
        globalE = "TmrCb1::onExpire ERROR: globalI=" + t0052GlobalId + ", tmr.getId=" + tmr.getId() + "; s='" + s + "', a='" + a + "'\n";
      }
    }
  }

  // Test the tmr system
  // This test verifies that a context thread callback (a timer callback actually)
  // can restart itself, start another timer, and cancel another timer.
  // TmrCb2 infinitely restarts itself.  TmrCb3 cancels Cb2 and starts Cb1.
  @Test
  public void t0054() throws Exception {
    System.out.println("Test t0054");
    logStrings.clear();
    T0052TmrCb1 cb1 = new T0052TmrCb1("s cb1");
    Tmr tmr1 = new Tmr(ctx1);
    T0054TmrCb2 cb2 = new T0054TmrCb2("s cb1");
    Tmr tmr2 = new Tmr(ctx1);
    T0054TmrCb3 cb3 = new T0054TmrCb3("s cb2", cb1, tmr1);
    Tmr tmr3 = new Tmr(ctx1);

    globalE = null;
    globalS = null;

    // T=0: start .5-sec tmr2 that reschedules itself infinitely.
    t0052GlobalId = tmr2.schedule(cb2, "a cb2", 500);
    Thread.sleep(300);

    // T=.3: cb2 hasn't fired yet.
    assertThat(globalS, is(nullValue()));  assertThat(globalE, is(nullValue()));
    Thread.sleep(400);
    // At t=.5: timer fires, writes its string, and reschedules itself.

    // T=.7: make sure tmr2 fired (check string).  Start .5 sec tmr3 that will cancel cb2.
    assertThat(globalS, containsString("TmrCb2::onExpire s='s cb1', a='a cb2'"));  assertThat(globalE, is(nullValue()));
    globalS = null;
    int id = tmr3.schedule(cb3, tmr2, 500);
    assertThat(id, is(1));
    Thread.sleep(500);
    // At t=1.0: tmr2 fires, writes its string, and reschedules itself.

    // T=1.2: make sure tmr2 fired.  Also, in the background, tmr3 fires, kills tmr2, and starts tmr1.
    assertThat(globalS, containsString("TmrCb2::onExpire s='s cb1', a='a cb2'"));  assertThat(globalE, is(nullValue()));
    globalS = null;
    Thread.sleep(400);

    // T=1.6: cb1 hasn't fired yet.  cb2 would have fired had it not been canceled; make sure.
    assertThat(globalS, is(nullValue()));  assertThat(globalE, is(nullValue()));
    Thread.sleep(300);

    // T=1.9: cb1 has fired.
    assertThat(globalE, is(nullValue()));
    assertThat(globalS, containsString("TmrCb1::onExpire s='s cb1', a='a cb1'"));

    assertThat(logStrings.size(), is(0));
  }

  private static class T0054TmrCb2 implements TmrCallback {
    String s;
    private T0054TmrCb2(String s) {
      this.s = s;
    }

    public void onExpire(Tmr tmr, LBMContext ctx, Object cbArg) {
      String a = (String)cbArg;
      int id = tmr.getId();
      if (t0052GlobalId == -1 || id == t0052GlobalId) {
        globalS = "TmrCb2::onExpire s='" + s + "', a='" + a + "'\n";
      } else {
        globalE = "TmrCb2::onExpire ERROR: globalI=" + t0052GlobalId + ", id=" + id + "; s='" + s + "', a='" + a + "'\n";
      }
      try {
        t0052GlobalId = tmr.schedule(this, "a cb2", 500);
        if (t0052GlobalId != id + 1) {
          globalE = "TmrCb2::onExpire ERROR: globalId=" + t0052GlobalId + ", should be " + (id + 1);  // should never happen.
        }
      } catch (Exception e) {
        globalE = "TmrCb2::onExpire ERROR: " + LbmCt.exDetails(e);  // should never happen.
      }
    }
  }

  private static class T0054TmrCb3 implements TmrCallback {
    String s;
    T0052TmrCb1 cb1;
    Tmr tmr1;
    private T0054TmrCb3(String s, T0052TmrCb1 cb1, Tmr tmr1) {
      this.s = s;
      this.cb1 = cb1;
      this.tmr1 = tmr1;
    }

    public void onExpire(Tmr tmr, LBMContext ctx, Object cbArg) {
      Tmr tmrToCancel = (Tmr)cbArg;
      try {
        tmrToCancel.cancelCtxThread();
        t0052GlobalId = tmr1.schedule(cb1, "a cb1", 500);
        if (t0052GlobalId != 1) {
          globalE = "TmrCb3::onExpire ERROR: id=" + t0052GlobalId + ", should be 1";  // should never happen.
        }
      } catch (Exception e) {
        globalE = "TmrCb3::onExpire ERROR: " + e.toString();  // should never happen.
      }
    }
  }

  @Test
  public void t0100() throws Exception {
    System.out.println("Test t0100");
    logStrings.clear();
    msgStrings.clear();
    assertThat(syncSem.availablePermits(), is(0));

    LbmCtConfig config = new LbmCtConfig();
    config.setMaxTries(2);
    config.setPreDelivery(1);
    config.setRetryIvl(200);
    LbmCt rcvCt = new LbmCt();
    rcvCt.start(ctx2, config, null);

    LBMTopic topicObj = ctx1.allocTopic("CtOldSrc", null);
    LBMSource umSrc = new LBMSource(ctx1, topicObj);

    LBMReceiverCallback rcvCb = new Test01xxRcvCb();
    Test01xxRcvConnCreateCb rcvCreateCb = new Test01xxRcvConnCreateCb();
    Test01xxRcvConnDeleteCb rcvDeleteCb = new Test01xxRcvConnDeleteCb();
    LbmCtRcv ctRcv = new LbmCtRcv();
    ctRcv.start(rcvCt, "CtOldSrc", null, rcvCb, rcvCreateCb, rcvDeleteCb, rcvCbArg);
    Thread.sleep(70);
    assertThat(logStrings.size(), is(0));

    umSrc.send("msg0".getBytes(), 4, LBM.MSG_FLUSH);
    Thread.sleep(70);

    assertThat(msgStrings.get(0), containsString("test_rcv_cb: type=0, sqn=0, source='TCP:"));
    assertThat(msgStrings.get(0), containsString("source_clientd='(null)', handshakeMessage=false,"));
    assertThat(msgStrings.get(0), containsString("0000: 6d 73 67 30              msg0"));
    assertThat(msgStrings.size(), is(1));
    Thread.sleep(400);  // let receiver go to timewait.

    umSrc.send("msg1".getBytes(), 4, LBM.MSG_FLUSH);
    Thread.sleep(50);

    assertThat(msgStrings.get(1), containsString("test_rcv_cb: type=0, sqn=1, source='TCP:"));
    assertThat(msgStrings.get(1), containsString("source_clientd='(null)', handshakeMessage=false,"));
    assertThat(msgStrings.get(1), containsString("0000: 6d 73 67 31              msg1"));
    assertThat(msgStrings.size(), is(2));
    assertThat(logStrings.get(0), containsString("giving up connecting to source 'TCP:"));
    assertThat(logStrings.get(0), containsString("for topic 'CtOldSrc'"));
    assertThat(logStrings.size(), is(1));

    ctRcv.stop();
    Thread.sleep(50);

    rcvCt.stop();
    umSrc.close();
    assertThat(msgStrings.size(), is(2));
    assertThat(logStrings.size(), is(1));
    assertThat(syncSem.availablePermits(), is(0));
  }

  @Test
  public void t0110() throws Exception {
    System.out.println("Test t0110");
    logStrings.clear();
    msgStrings.clear();
    assertThat(syncSem.availablePermits(), is(0));

    LbmCtConfig config = new LbmCtConfig();
    config.setRetryIvl(500);
    config.setMaxTries(3);

    ByteBuffer metadata = ByteBuffer.allocate(100);
    metadata.clear();  metadata.put("Meta s_ct".getBytes());  metadata.flip();
    LbmCt srcCt = new LbmCt();
    srcCt.start(ctx1, config, metadata);

    metadata.clear();  metadata.put("Meta r_ct".getBytes());  metadata.flip();
    LbmCt rcvCt = new LbmCt();
    rcvCt.start(ctx2, config, metadata);

    rcvCt.getConfig().setTestBits(rcvCt.getConfig().getTestBits() | LbmCtConfig.TEST_BITS_NO_COK);

    // Create callback objects.
    LBMReceiverCallback rcvCb = new Test01xxRcvCb();
    Test01xxRcvConnCreateCb rcvCreateCb = new Test01xxRcvConnCreateCb();
    Test01xxRcvConnDeleteCb rcvDeleteCb = new Test01xxRcvConnDeleteCb();
    Test01xxSrcConnCreateCb srcCreateCb = new Test01xxSrcConnCreateCb();
    Test01xxSrcConnDeleteCb srcDeleteCb = new Test01xxSrcConnDeleteCb();

    LbmCtSrc ctSrc = new LbmCtSrc();
    ctSrc.start(srcCt, "CtRetryExceed2", null, null, srcCreateCb, srcDeleteCb, srcCbArg);
    Thread.sleep(10);
    assertThat(logStrings.size(), is(0));

    LbmCtRcv ctRcv = new LbmCtRcv();
    ctRcv.start(rcvCt, "CtRetryExceed2", null, rcvCb, rcvCreateCb, rcvDeleteCb, rcvCbArg);
    Thread.sleep(300);

    assertThat(logStrings.get(0), containsString("test_rcv_conn_create_cb, clientd='RcvClientd', peer: status=0, flags=0xf, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct', rcv_source_name='TCP:"));
    assertThat(logStrings.get(1), containsString("LBMCT_TEST_BITS_NO_COK"));
    assertThat(logStrings.size(), is(2));
    Thread.sleep(2000);

    syncSem.acquire();
    assertThat(syncSem.availablePermits(), is(0));
    assertThat(logStrings.get(2), containsString("LBMCT_TEST_BITS_NO_COK"));
    assertThat(logStrings.get(3), containsString("LBMCT_TEST_BITS_NO_COK"));
    assertThat(logStrings.get(4), containsString("giving up accepting connection from receiver "));
    assertThat(logStrings.size(), is(5));

    ctSrc.stop();
    Thread.sleep(500);
    // The source doesn't think the connection exists.  However, the deletion of the underlying UM source causes the
    // rcv per-source delete callback, abruptly deletes the connection without DREQ.
    assertThat(logStrings.get(5), containsString("test_rcv_conn_delete_cb, clientd='RcvClientd', conn_clientd='RcvConnClientd', peer: status=-1, flags=0xf, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct', rcv_source_name='TCP:"));
    assertThat(logStrings.size(), is(6));

    ctRcv.stop();
    Thread.sleep(200);

    assertThat(logStrings.size(), is(6));

    rcvCt.stop();
    srcCt.stop();

    assertThat(msgStrings.get(0), containsString("0000: 7b 13 8c 02 "));
    assertThat(msgStrings.get(1), containsString("0000: 7b 13 8c 02 "));
    assertThat(msgStrings.get(2), containsString("0000: 7b 13 8c 02 "));

    assertThat(msgStrings.size(), is(3));
    assertThat(logStrings.size(), is(6));
    assertThat(syncSem.availablePermits(), is(0));
  }

  @Test
  public void t0120() throws Exception {
    System.out.println("Test t0120");
    logStrings.clear();
    msgStrings.clear();
    assertThat(syncSem.availablePermits(), is(0));

    ByteBuffer metadata = ByteBuffer.allocate(100);

    LbmCtConfig srcConfig = new LbmCtConfig();
    srcConfig.setRetryIvl(500);
    srcConfig.setMaxTries(3);
    metadata.clear();
    metadata.put("Meta s_ct".getBytes());
    metadata.flip();
    LbmCt srcCt = new LbmCt();
    srcCt.start(ctx1, srcConfig, metadata);

    LbmCtConfig rcvConfig = new LbmCtConfig();
    rcvConfig.setRetryIvl(490);
    rcvConfig.setMaxTries(3);
    metadata.clear();
    metadata.put("Meta r_ct".getBytes());
    metadata.flip();
    LbmCt rcvCt = new LbmCt();
    rcvCt.start(ctx2, rcvConfig, metadata);

    srcCt.getConfig().setTestBits(srcCt.getConfig().getTestBits() | LbmCtConfig.TEST_BITS_NO_CRSP);

    // Create callback objects.
    LBMReceiverCallback rcvCb = new Test01xxRcvCb();
    Test01xxRcvConnCreateCb rcvCreateCb = new Test01xxRcvConnCreateCb();
    Test01xxRcvConnDeleteCb rcvDeleteCb = new Test01xxRcvConnDeleteCb();
    Test01xxSrcConnCreateCb srcCreateCb = new Test01xxSrcConnCreateCb();
    Test01xxSrcConnDeleteCb srcDeleteCb = new Test01xxSrcConnDeleteCb();

    LbmCtSrc ctSrc = new LbmCtSrc();
    ctSrc.start(srcCt, "CtRetryExceed2", null, null, srcCreateCb, srcDeleteCb, srcCbArg);
    Thread.sleep(10);
    assertThat(logStrings.size(), is(0));

    LbmCtRcv ctRcv = new LbmCtRcv();
    ctRcv.start(rcvCt, "CtRetryExceed2", null, rcvCb, rcvCreateCb, rcvDeleteCb, rcvCbArg);
    Thread.sleep(200);

    assertThat(logStrings.get(0), containsString("LBMCT_TEST_BITS_NO_CRSP"));
    assertThat(logStrings.get(1), containsString("LBMCT_TEST_BITS_NO_CRSP"));
    assertThat(logStrings.size(), is(2));

    Thread.sleep(2000);

    assertThat(syncSem.availablePermits(), is(0));
    assertThat(logStrings.get(2), containsString("LBMCT_TEST_BITS_NO_CRSP"));
    assertThat(logStrings.get(3), containsString("giving up connecting to source "));
    assertThat(logStrings.get(4), containsString("LBMCT_TEST_BITS_NO_CRSP"));
    assertThat(logStrings.get(5), containsString("LBMCT_TEST_BITS_NO_CRSP"));
    assertThat(logStrings.get(6), containsString("giving up accepting connection from receiver "));
    assertThat(logStrings.size(), is(7));

    assertThat(msgStrings.size(), is(0));

    Thread.sleep(200);
    ctSrc.stop();

    Thread.sleep(200);
    // The receiver doesn't think the connection exists.  However, the deletion of the underlying UM source causes
    // the per-source delete callback to be invoked.
    assertThat(logStrings.size(), is(7));
    ctRcv.stop();
    Thread.sleep(200);
    assertThat(logStrings.size(), is(7));

    rcvCt.stop();
    srcCt.stop();

    assertThat(msgStrings.size(), is(0));
    assertThat(logStrings.size(), is(7));
    assertThat(syncSem.availablePermits(), is(0));
  }

  @Test
  public void t0130() throws Exception {
    System.out.println("Test t0130");
    logStrings.clear();
    msgStrings.clear();
    assertThat(syncSem.availablePermits(), is(0));

    ByteBuffer metadata = ByteBuffer.allocate(100);

    LbmCtConfig srcConfig = new LbmCtConfig();
    srcConfig.setRetryIvl(500);
    srcConfig.setMaxTries(3);
    metadata.clear();
    metadata.put("Meta s_ct".getBytes());
    metadata.flip();
    LbmCt srcCt = new LbmCt();
    srcCt.start(ctx1, srcConfig, metadata);

    LbmCtConfig rcvConfig = new LbmCtConfig();
    rcvConfig.setRetryIvl(490);
    rcvConfig.setMaxTries(3);
    metadata.clear();
    metadata.put("Meta r_ct".getBytes());
    metadata.flip();
    LbmCt rcvCt = new LbmCt();
    rcvCt.start(ctx2, rcvConfig, metadata);

    // Create callback objects.
    LBMReceiverCallback rcvCb = new Test01xxRcvCb();
    Test01xxRcvConnCreateCb rcvCreateCb = new Test01xxRcvConnCreateCb();
    Test01xxRcvConnDeleteCb rcvDeleteCb = new Test01xxRcvConnDeleteCb();
    Test01xxSrcConnCreateCb srcCreateCb = new Test01xxSrcConnCreateCb();
    Test01xxSrcConnDeleteCb srcDeleteCb = new Test01xxSrcConnDeleteCb();

    LbmCtSrc ctSrc = new LbmCtSrc();
    ctSrc.start(srcCt, "CtRetryExceed2", null, null, srcCreateCb, srcDeleteCb, srcCbArg);
    Thread.sleep(10);
    assertThat(logStrings.size(), is(0));

    LbmCtRcv ctRcv = new LbmCtRcv();
    ctRcv.start(rcvCt, "CtRetryExceed2", null, rcvCb, rcvCreateCb, rcvDeleteCb, rcvCbArg);

    syncSem.acquire(2);

    assertThat(logStrings.get(0), containsString("test_rcv_conn_create_cb, clientd='RcvClientd', peer: status=0, flags=0xf, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct'"));
    assertThat(logStrings.get(1), containsString("test_src_conn_create_cb, clientd='SrcClientd', peer: status=0, flags=0xb, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct', no rcv_source_name, rcv_start_seq_num=0, no rcv_end_seq_num"));
    assertThat(logStrings.size(), is(2));

    assertThat(msgStrings.get(0), containsString("0040: 00 00 00 09 4d 65 74 61  ....Meta"));
    assertThat(msgStrings.get(0), containsString("0048: 20 73 5f 63 74            s_ct"));
    assertThat(msgStrings.get(0), containsString("props=LbmCt.h:1."));
    assertThat(msgStrings.size(), is(1));

    srcCt.getConfig().setTestBits(srcCt.getConfig().getTestBits() | LbmCtConfig.TEST_BITS_NO_DRSP);
    ctRcv.stop();
    Thread.sleep(3000);
    assertThat(logStrings.get(2), containsString("LBMCT_TEST_BITS_NO_DRSP"));
    assertThat(logStrings.get(3), containsString("LBMCT_TEST_BITS_NO_DRSP"));
    assertThat(logStrings.get(4), containsString("LBMCT_TEST_BITS_NO_DRSP"));
    assertThat(logStrings.get(5), containsString("giving up stopping connection to source "));
    assertThat(logStrings.get(6), containsString("test_rcv_conn_delete_cb, clientd='RcvClientd', conn_clientd='RcvConnClientd', peer: status=-1, flags=0xf, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct', rcv_source_name='TCP:"));
    assertThat(logStrings.get(7), containsString("LBMCT_TEST_BITS_NO_DRSP"));
    assertThat(logStrings.get(8), containsString("LBMCT_TEST_BITS_NO_DRSP"));
    assertThat(logStrings.get(9), containsString("giving up stopping connection from receiver"));
    assertThat(logStrings.get(10), containsString("test_src_conn_delete_cb, clientd='SrcClientd', conn_clientd='SrcConnClientd', peer: status=-1, flags=0xb, src_metadata='Meta s_ct', rcv_metadata='Meta r_ct', no rcv_source_name, rcv_start_seq_num=0, no rcv_end_seq_num"));

    assertThat(logStrings.size(), is(11));

    ctSrc.stop();

    rcvCt.stop();
    srcCt.stop();

    assertThat(msgStrings.size(), is(1));
    assertThat(logStrings.size(), is(11));
    assertThat(syncSem.availablePermits(), is(0));
  }


  @Test
  public void t0140() throws Exception {
    System.out.println("Test t0140");
    logStrings.clear();
    msgStrings.clear();
    assertThat(syncSem.availablePermits(), is(0));

    ByteBuffer metadata = ByteBuffer.allocate(100);

    LbmCtConfig config = new LbmCtConfig();
    // config.setTestBits(LbmCtConfig.TEST_BITS_DEBUG);  // enable debug recording
    metadata.clear();
    metadata.put("Meta ct1".getBytes());
    metadata.flip();
    LbmCt ct1 = new LbmCt();
    ct1.start(ctx1, config, metadata);

    metadata.clear();
    metadata.put("Meta ct2".getBytes());
    metadata.flip();
    LbmCt ct2 = new LbmCt();
    ct2.start(ctx2, config, metadata);

    // Create callback objects.
    LBMReceiverCallback rcvCb = new Test01xxRcvCb();
    Test01xxRcvConnCreateCb rcvCreateCb = new Test01xxRcvConnCreateCb();
    Test01xxRcvConnDeleteCb rcvDeleteCb = new Test01xxRcvConnDeleteCb();
    Test01xxSrcConnCreateCb srcCreateCb = new Test01xxSrcConnCreateCb();
    Test01xxSrcConnDeleteCb srcDeleteCb = new Test01xxSrcConnDeleteCb();

    // First stream: 1 src, 2 rcv.
    LbmCtSrc ctSrc1 = new LbmCtSrc();
    ctSrc1.start(ct1, "CtMultiStream1", null, null, srcCreateCb, srcDeleteCb, "src1CtMultiStream1");

    LbmCtRcv ctRcv1 = new LbmCtRcv();
    ctRcv1.start(ct1, "CtMultiStream1", null, rcvCb, rcvCreateCb, rcvDeleteCb, "rcv1CtMultiStream1");
    Thread.sleep(20);

    LbmCtRcv ctRcv2 = new LbmCtRcv();
    ctRcv2.start(ct2, "CtMultiStream1", null, rcvCb, rcvCreateCb, rcvDeleteCb, "rcv2CtMultiStream1");
    Thread.sleep(20);

    // Second stream: 2 src, 1 rcv.
    LbmCtRcv ctRcv3 = new LbmCtRcv();
    ctRcv3.start(ct1, "CtMultiStream2", null, rcvCb, rcvCreateCb, rcvDeleteCb, "rcv3CtMultiStream2");

    LbmCtSrc ctSrc2 = new LbmCtSrc();
    ctSrc2.start(ct1, "CtMultiStream2", null, null, srcCreateCb, srcDeleteCb, "src2CtMultiStream2");
    Thread.sleep(20);

    LbmCtSrc ctSrc3 = new LbmCtSrc();
    ctSrc3.start(ct2, "CtMultiStream2", null, null, srcCreateCb, srcDeleteCb, "src3CtMultiStream2");

    // Third stream: 2 src, 2 rcv.
    LbmCtSrc ctSrc4 = new LbmCtSrc();
    ctSrc4.start(ct1, "CtMultiStream3", null, null, srcCreateCb, srcDeleteCb, "src4CtMultiStream3");

    LbmCtRcv ctRcv4 = new LbmCtRcv();
    ctRcv4.start(ct1, "CtMultiStream3", null, rcvCb, rcvCreateCb, rcvDeleteCb, "rcv4CtMultiStream3");
    Thread.sleep(20);

    LbmCtSrc ctSrc5 = new LbmCtSrc();
    ctSrc5.start(ct2, "CtMultiStream3", null, null, srcCreateCb, srcDeleteCb, "src5CtMultiStream3");
    Thread.sleep(20);

    LbmCtRcv ctRcv5 = new LbmCtRcv();
    ctRcv5.start(ct2, "CtMultiStream3", null, rcvCb, rcvCreateCb, rcvDeleteCb, "rcv5CtMultiStream3");

    syncSem.acquire(16);
    Thread.sleep(20);

    StringBuilder allLogs = new StringBuilder(10000);
    for (String logString : logStrings) {
      allLogs.append(logString);
    }

    String allLogsS = allLogs.toString();

    assertThat(allLogsS, containsString("test_rcv_conn_create_cb, clientd='rcv1CtMultiStream1'," +
        " peer: status=0, flags=0xf, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_src_conn_create_cb, clientd='src1CtMultiStream1'," +
        " peer: status=0, flags=0xb, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_rcv_conn_create_cb, clientd='rcv2CtMultiStream1'," +
        " peer: status=0, flags=0xf, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString("test_src_conn_create_cb, clientd='src1CtMultiStream1'," +
        " peer: status=0, flags=0xb, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));

    assertThat(allLogsS, containsString("test_rcv_conn_create_cb, clientd='rcv3CtMultiStream2'," +
        " peer: status=0, flags=0xf, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_src_conn_create_cb, clientd='src2CtMultiStream2'," +
        " peer: status=0, flags=0xb, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_rcv_conn_create_cb, clientd='rcv3CtMultiStream2'," +
        " peer: status=0, flags=0xf, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_src_conn_create_cb, clientd='src3CtMultiStream2'," +
        " peer: status=0, flags=0xb, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));

    assertThat(allLogsS, containsString("test_rcv_conn_create_cb, clientd='rcv4CtMultiStream3'," +
        " peer: status=0, flags=0xf, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_src_conn_create_cb, clientd='src4CtMultiStream3'," +
        " peer: status=0, flags=0xb, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_rcv_conn_create_cb, clientd='rcv4CtMultiStream3'," +
        " peer: status=0, flags=0xf, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_src_conn_create_cb, clientd='src5CtMultiStream3'," +
        " peer: status=0, flags=0xb, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_rcv_conn_create_cb, clientd='rcv5CtMultiStream3'," +
        " peer: status=0, flags=0xf, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString("test_src_conn_create_cb, clientd='src4CtMultiStream3'," +
        " peer: status=0, flags=0xb, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString("test_rcv_conn_create_cb, clientd='rcv5CtMultiStream3'," +
        " peer: status=0, flags=0xf, src_metadata='Meta ct2', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString("test_src_conn_create_cb, clientd='src5CtMultiStream3'," +
        " peer: status=0, flags=0xb, src_metadata='Meta ct2', rcv_metadata='Meta ct2'"));

    allLogs.setLength(0);
    for (String msgString : msgStrings) {
      assertThat(msgString, containsString("source_clientd='RcvConnClientd', handshakeMessage=true"));
      allLogs.append(msgString);
    }
    allLogsS = allLogs.toString();

    assertThat(allLogsS, containsString("properties=(non-nil), clientd='rcv1CtMultiStream1', source_clientd='RcvConnClientd', handshakeMessage=true"));
    assertThat(allLogsS, containsString("0040: 00 00 00 08 4d 65 74 61  ....Meta\n0048: 20 63 74 31               ct1 "));
    assertThat(allLogsS, containsString("properties=(non-nil), clientd='rcv2CtMultiStream1', source_clientd='RcvConnClientd', handshakeMessage=true"));
    assertThat(allLogsS, containsString("0040: 00 00 00 08 4d 65 74 61  ....Meta\n0048: 20 63 74 31               ct1 "));

    int startingNumMsgs = msgStrings.size();

    ctSrc1.getUmSrc().send("msg0".getBytes(), 4, LBM.MSG_FLUSH);
    Thread.sleep(10);
    assertThat(msgStrings.get(startingNumMsgs) + msgStrings.get(startingNumMsgs+1), containsString(
        "properties=(nil), clientd='rcv1CtMultiStream1', source_clientd='RcvConnClientd', handshakeMessage=false, data:\n0000: 6d 73 67 30              msg0"));
    assertThat(msgStrings.get(startingNumMsgs) + msgStrings.get(startingNumMsgs+1), containsString(
        "properties=(nil), clientd='rcv1CtMultiStream1', source_clientd='RcvConnClientd', handshakeMessage=false, data:\n0000: 6d 73 67 30              msg0"));
    assertThat(msgStrings.size(), is(startingNumMsgs + 2));  // 2 receivers for this message

    LBMMessageProperties msgProps = new LBMMessageProperties();
    msgProps.set("CtOldSrcProp", 10);
    LBMSourceSendExInfo exInfo = new LBMSourceSendExInfo();
    exInfo.setMessageProperties(msgProps);

    ctSrc2.getUmSrc().send("msg1".getBytes(), 4, LBM.MSG_FLUSH, exInfo);
    Thread.sleep(10);
    assertThat(msgStrings.get(startingNumMsgs+2), containsString(
        "properties=(non-nil), clientd='rcv3CtMultiStream2', source_clientd='RcvConnClientd', handshakeMessage=false, data:\n0000: 6d 73 67 31              msg1    \n, props=CtOldSrcProp:10."));
    assertThat(msgStrings.size(), is(startingNumMsgs + 3));

    ctSrc3.getUmSrc().send("msg2".getBytes(), 4, LBM.MSG_FLUSH);
    Thread.sleep(10);
    assertThat(msgStrings.get(startingNumMsgs+3), containsString(
        "properties=(nil), clientd='rcv3CtMultiStream2', source_clientd='RcvConnClientd', handshakeMessage=false, data:\n0000: 6d 73 67 32              msg2"));
    assertThat(msgStrings.size(), is(startingNumMsgs + 4));

    ctSrc4.getUmSrc().send("msg3".getBytes(), 4, LBM.MSG_FLUSH);
    Thread.sleep(10);
    assertThat(msgStrings.get(startingNumMsgs+4) + msgStrings.get(startingNumMsgs+5), containsString(
        "properties=(nil), clientd='rcv4CtMultiStream3', source_clientd='RcvConnClientd', handshakeMessage=false, data:\n0000: 6d 73 67 33              msg3"));
    assertThat(msgStrings.get(startingNumMsgs+4) + msgStrings.get(startingNumMsgs+5), containsString(
        "properties=(nil), clientd='rcv5CtMultiStream3', source_clientd='RcvConnClientd', handshakeMessage=false, data:\n0000: 6d 73 67 33              msg3"));
    assertThat(msgStrings.size(), is(startingNumMsgs + 6));

    ctSrc5.getUmSrc().send("msg4".getBytes(), 4, LBM.MSG_FLUSH);
    Thread.sleep(10);
    assertThat(msgStrings.get(startingNumMsgs+6) + msgStrings.get(startingNumMsgs+7), containsString(
        "properties=(nil), clientd='rcv4CtMultiStream3', source_clientd='RcvConnClientd', handshakeMessage=false, data:\n0000: 6d 73 67 34              msg4"));
    assertThat(msgStrings.get(startingNumMsgs+6) + msgStrings.get(startingNumMsgs+7), containsString(
        "properties=(nil), clientd='rcv5CtMultiStream3', source_clientd='RcvConnClientd', handshakeMessage=false, data:\n0000: 6d 73 67 34              msg4"));
    assertThat(msgStrings.size(), is(startingNumMsgs + 8));

    assertThat(logStrings.size(), is(16));

    Thread.sleep(200);

    ctSrc1.stop();
    ctSrc2.stop();
    ctSrc3.stop();
    ctSrc4.stop();
    ctSrc5.stop();

    Thread.sleep(100);

    ctRcv1.stop();
    ctRcv2.stop();
    ctRcv3.stop();
    ctRcv4.stop();
    ctRcv5.stop();

    Thread.sleep(100);

    allLogs.setLength(0);  // empty string buffer.
    for (int i = 16; i < logStrings.size(); i++) {
      allLogs.append(logStrings.get(i));
    }
    allLogsS = allLogs.toString();

    assertThat(allLogsS, containsString("test_rcv_conn_delete_cb, clientd='rcv1CtMultiStream1'," +
        " conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_src_conn_delete_cb, clientd='src1CtMultiStream1'," +
        " conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_rcv_conn_delete_cb, clientd='rcv2CtMultiStream1'," +
        " conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString("test_src_conn_delete_cb, clientd='src1CtMultiStream1'," +
        " conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));

    assertThat(allLogsS, containsString("test_rcv_conn_delete_cb, clientd='rcv3CtMultiStream2'," +
        " conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_src_conn_delete_cb, clientd='src2CtMultiStream2'," +
        " conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_rcv_conn_delete_cb, clientd='rcv3CtMultiStream2'," +
        " conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_src_conn_delete_cb, clientd='src3CtMultiStream2'," +
        " conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));

    assertThat(allLogsS, containsString("test_rcv_conn_delete_cb, clientd='rcv4CtMultiStream3'," +
        " conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_src_conn_delete_cb, clientd='src4CtMultiStream3'," +
        " conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_rcv_conn_delete_cb, clientd='rcv4CtMultiStream3'," +
        " conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_src_conn_delete_cb, clientd='src5CtMultiStream3'," +
        " conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString("test_rcv_conn_delete_cb, clientd='rcv5CtMultiStream3'," +
        " conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString("test_src_conn_delete_cb, clientd='src4CtMultiStream3'," +
        " conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString("test_rcv_conn_delete_cb, clientd='rcv5CtMultiStream3'," +
        " conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct2', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString("test_src_conn_delete_cb, clientd='src5CtMultiStream3'," +
        " conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct2', rcv_metadata='Meta ct2'"));

    ct2.stop();
    ct1.stop();

    //???assertThat(logStrings.size(), is(32));
    assertThat(syncSem.availablePermits(), is(0));
  }

  @Test
  public void t0150() throws Exception {
    System.out.println("Test t0150, t=" + System.nanoTime());
    logStrings.clear();
    msgStrings.clear();
    assertThat(syncSem.availablePermits(), is(0));

    ByteBuffer metadata = ByteBuffer.allocate(100);

    LbmCtConfig config1 = new LbmCtConfig();
    config1.setRetryIvl(500);
    config1.setMaxTries(3);
    metadata.clear();
    metadata.put("Meta ct1".getBytes());
    metadata.flip();
    LbmCt ct1 = new LbmCt();
    ct1.start(ctx1, config1, metadata);

    LbmCtConfig config2 = new LbmCtConfig();
    config2.setRetryIvl(490);
    config2.setMaxTries(3);
    metadata.clear();
    metadata.put("Meta ct2".getBytes());
    metadata.flip();
    LbmCt ct2 = new LbmCt();
    ct2.start(ctx2, config2, metadata);

    // Create callback objects.
    LBMReceiverCallback rcvCb = new Test01xxRcvCb();
    Test01xxRcvConnCreateCb rcvCreateCb = new Test01xxRcvConnCreateCb();
    Test01xxRcvConnDeleteCb rcvDeleteCb = new Test01xxRcvConnDeleteCb();
    Test01xxSrcConnCreateCb srcCreateCb = new Test01xxSrcConnCreateCb();
    Test01xxSrcConnDeleteCb srcDeleteCb = new Test01xxSrcConnDeleteCb();

    LbmCtSrc ctSrc = new LbmCtSrc();
    ctSrc.start(ct1, "CtRetryExceed2", null, null, srcCreateCb, srcDeleteCb, srcCbArg);

    LbmCtSrc ctSrc2 = new LbmCtSrc();
    ctSrc2.start(ct2, "CtRetryExceed2", null, null, srcCreateCb, srcDeleteCb, src2CbArg);
    Thread.sleep(10);
    assertThat(logStrings.size(), is(0));

    LbmCtRcv ctRcv = new LbmCtRcv();
    ctRcv.start(ct2, "CtRetryExceed2", null, rcvCb, rcvCreateCb, rcvDeleteCb, rcvCbArg);

    LbmCtRcv ctRcv2 = new LbmCtRcv();
    ctRcv2.start(ct1, "CtRetryExceed2", null, rcvCb, rcvCreateCb, rcvDeleteCb, rcv2CbArg);

    syncSem.acquire(8);

    StringBuilder allLogs = new StringBuilder(10000);
    for (String logString : logStrings) {
      allLogs.append(logString);
    }
    String allLogsS = allLogs.toString();
    assertThat(allLogsS, containsString(
        "test_rcv_conn_create_cb, clientd='Rcv2Clientd', peer: status=0, flags=0xf, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString(
        "test_rcv_conn_create_cb, clientd='Rcv2Clientd', peer: status=0, flags=0xf, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString(
        "test_rcv_conn_create_cb, clientd='RcvClientd', peer: status=0, flags=0xf, src_metadata='Meta ct2', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString(
        "test_rcv_conn_create_cb, clientd='Rcv2Clientd', peer: status=0, flags=0xf, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString(
        "test_rcv_conn_create_cb, clientd='RcvClientd', peer: status=0, flags=0xf, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString(
        "test_src_conn_create_cb, clientd='SrcClientd', peer: status=0, flags=0xb, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString(
        "test_src_conn_create_cb, clientd='Src2Clientd', peer: status=0, flags=0xb, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString(
        "test_src_conn_create_cb, clientd='Src2Clientd', peer: status=0, flags=0xb, src_metadata='Meta ct2', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString(
        "test_src_conn_create_cb, clientd='SrcClientd', peer: status=0, flags=0xb, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));

    //???assertThat(logStrings.size(), is(8));
    //assertThat(msgStrings.size(), is(6));
    Thread.sleep(200);

    T0150TmrCb t1Cb = new T0150TmrCb();
    Tmr t1 = new Tmr(ctx1);
    // Give DREQ a chance to be sent (and dropped).  Then let retry succeed.
    t1.schedule(t1Cb, ct2.getConfig(), 20);
    ct2.getConfig().setTestBits(ct2.getConfig().getTestBits() | LbmCtConfig.TEST_BITS_NO_DREQ);

    ctRcv.stop();
    ctSrc.stop();
    ctSrc2.stop();
    ctRcv2.stop();

    allLogs.setLength(0);  // empty string buffer.
    for (int i = 8; i < logStrings.size(); i++) {
      allLogs.append(logStrings.get(i));
    }
    allLogsS = allLogs.toString();

    assertThat(allLogsS, containsString(
        "LBMCT_TEST_BITS_NO_DREQ, skipping send of DREQ"));
    assertThat(allLogsS, containsString(
        "test_rcv_conn_delete_cb, clientd='Rcv2Clientd', conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString(
        "test_rcv_conn_delete_cb, clientd='RcvClientd', conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct2', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString(
        "test_rcv_conn_delete_cb, clientd='Rcv2Clientd', conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString(
        "test_rcv_conn_delete_cb, clientd='RcvClientd', conn_clientd='RcvConnClientd', peer: status=0, flags=0x1f, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString(
        "test_src_conn_delete_cb, clientd='SrcClientd', conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct1', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString(
        "test_src_conn_delete_cb, clientd='Src2Clientd', conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct2', rcv_metadata='Meta ct1'"));
    assertThat(allLogsS, containsString(
        "test_src_conn_delete_cb, clientd='Src2Clientd', conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct2', rcv_metadata='Meta ct2'"));
    assertThat(allLogsS, containsString(
        "test_src_conn_delete_cb, clientd='SrcClientd', conn_clientd='SrcConnClientd', peer: status=0, flags=0x1b, src_metadata='Meta ct1', rcv_metadata='Meta ct2'"));

    ct2.stop();
    ct1.stop();

    //???assertThat(logStrings.size(), is(18));
    //???assertThat(msgStrings.size(), is(14));
    assertThat(syncSem.availablePermits(), is(0));
  }

  private static class T0150TmrCb implements TmrCallback {
    public void onExpire(Tmr tmr, LBMContext ctx, Object cbArg) {
      LbmCtConfig cfg = (LbmCtConfig)cbArg;
      cfg.setTestBits(cfg.getTestBits() & (~LbmCtConfig.TEST_BITS_NO_DREQ));
    }
  }


  class Test01xxRcvCb implements LBMReceiverCallback {
    @Override
    public int onReceive(Object cbArg, LBMMessage umMsg) {
      LbmCtRcvConn rcvConn = (LbmCtRcvConn) umMsg.sourceClientObject();
      boolean handshakeMessage = rcvConn.isHandshakeMessage();
      recordMsg(umMsg, (String)cbArg, handshakeMessage);
      if (!handshakeMessage) {
        umMsg.dispose();
      }

      return 0;
    }
  }

  class Test01xxSrcConnCreateCb implements LbmCtSrcConnCreateCallback {
    @Override
    public Object onSrcConnCreate(LbmCtSrc ctSrc, LbmCtPeerInfo peerInfo, Object cbArg) {
      String srcCbArg = (String)cbArg;
      LBMPubLog.pubLog(LBM.LOG_INFO, "test_src_conn_create_cb, clientd='" + srcCbArg +
          "', peer:" + peer2str(peerInfo) + "\n");
      syncSem.release();
      return srcConnCbArg;
    }
  }

  class Test01xxSrcConnDeleteCb implements LbmCtSrcConnDeleteCallback {
    @Override
    public void onSrcConnDelete(LbmCtSrc ctSrc, LbmCtPeerInfo peerInfo, Object cbArg, Object connCbArg) {
      String srcCbArg = (String)cbArg;
      String srcConnCbArg = (String)connCbArg;
      LBMPubLog.pubLog(LBM.LOG_INFO, "test_src_conn_delete_cb, clientd='" + srcCbArg +
          "', conn_clientd='" + srcConnCbArg + "', peer:" + peer2str(peerInfo) + "\n");
    }
  }

  class Test01xxRcvConnCreateCb implements LbmCtRcvConnCreateCallback {
    @Override
    public Object onRcvConnCreate(LbmCtRcv ctRcv, LbmCtPeerInfo peerInfo, Object cbArg) {
      try { Thread.sleep(2); } catch (Exception e) { LBMPubLog.pubLog(LBM.LOG_INFO, "Test01xxRcvConnCreateCb: sleep exception\n"); }
      String rcvCbArg = (String)cbArg;
      LBMPubLog.pubLog(LBM.LOG_INFO, "test_rcv_conn_create_cb, clientd='" + rcvCbArg +
          "', peer:" + peer2str(peerInfo) + "\n");
      syncSem.release();
      return rcvConnCbArg;
    }
  }

  class Test01xxRcvConnDeleteCb implements LbmCtRcvConnDeleteCallback {
    @Override
    public void onRcvConnDelete(LbmCtRcv ctRcv, LbmCtPeerInfo peerInfo, Object cbArg, Object connCbArg) {
      String rcvCbArg = (String)cbArg;
      String rcvConnCbArg = (String)connCbArg;
      LBMPubLog.pubLog(LBM.LOG_INFO, "test_rcv_conn_delete_cb, clientd='" + rcvCbArg +
          "', conn_clientd='" + rcvConnCbArg + "', peer:" + peer2str(peerInfo) + "\n");
    }
  }

  // Code executed after the tests run.
  @Test
  public void t9999() {
    System.out.println("Test t9999");
    logStrings.clear();

    System.out.println("Stopping contexts");
    ctx1.close();
    ctx2.close();

    assertThat(logStrings.size(), is(0));
  }

  private static String hexDump(byte[] inBuf) {
    return hexDump(inBuf, inBuf.length);
  }

  private static String hexDump(byte[] inBuf, int limit) {
    StringBuilder sb = new StringBuilder(5000);
    int numBytes = inBuf.length;
    if ((limit > 0) && (numBytes > limit)) {
      numBytes = limit;
    }

    // Each line will show 8 bytes of dump.  The last line might not be complete.
    int numRows = (numBytes+7)/8;  // round up.
    int numLoops = numRows * 8;  // May loop past the end of data; code below handles it.

    // First thought was to loop rows, but the code is cleaner if you loop
    // the bytes and handle lines at the right spots.
    for (int byteOffset = 0; byteOffset < numLoops; byteOffset++) {
      if ((byteOffset %8) == 0) {  // Start of a line?
        sb.append(String.format("%04d: ", byteOffset));
      }
      if (byteOffset >= numBytes) {
        sb.append("   ");  // Past the end of real data.
      } else {
        sb.append(String.format("%02x ", ((int)inBuf[byteOffset]) & 0xff));  // Get rid of sign extension.
      }
      if (byteOffset %8 == 7) {  // End of line?
        sb.append(' ');
        // Do the ascii dump of this row's bytes.
        for (int asciiOffset = (byteOffset -7); asciiOffset <= byteOffset; asciiOffset++) {
          if (asciiOffset >= numBytes) {
            sb.append(' ');  // Past the end of real data.
          } else {
            if ((inBuf[asciiOffset] >= 32) && (inBuf[asciiOffset] <= 126)) {
              sb.append(String.format("%c", inBuf[asciiOffset]));
            } else {  // not printable.
              sb.append('.');
            }
          }
        }
        sb.append('\n');
      }
    }
    return sb.toString();
  }

}  // LbmCtTest
