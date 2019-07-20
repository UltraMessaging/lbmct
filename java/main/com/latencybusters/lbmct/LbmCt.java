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
import java.security.SecureRandom;
import java.io.StringWriter;
import java.io.PrintWriter;

import com.latencybusters.lbm.*;

/**
 * An instance of the Connected Topics for Ultra Messaging (UM).
 * A Connected Topics instance is associated with a UM context.
 * A given UM context must not have more than one Connected Topics instance associated with it.
 * When an {@link #LbmCt()} object is created, its full initialization is deferred until its {@link #start} method is
 * called.
 * <p>
 * A Connected Topics instance has an independent thread associated with it.
 * Since it is an active object, it must be explicitly stopped when it is no longer needed, using the {@link #stop}
 * method.
 * All CT receivers and sources associated with the <tt>LbmCt</tt> must be stopped before the <tt>LbmCt</tt> itself
 * can be stopped.
 */
public class LbmCt {
  final static String HANDSHAKE_TOPIC_STR = "LbmCt.h";

  private LBMContext ctx = null;
  private LbmCtConfig config = null;
  private ByteBuffer metadata = null;
  private LBMSourceSendExInfo srcExInfo = null;
  private LbmCtCtrlr ctrlr = null;

  private int localRequestPort = 0;
  private int localDomainId = -1;
  private int localIpAddr = 0;
  private LBMReceiver umHandshakeRcv = null;
  private int ctId = 0;
  private int nextConnId = 0;

  private LbmCtHandshakeParser handshakeParser = null;

  private Map<String, LbmCtSrc> ctSrcMap = null;
  private Map<String, LbmCtSrcConn> srcConnMap = null;
  private Set<LbmCtSrc> ctSrcSet = null;
  private Set<LbmCtRcv> ctRcvSet = null;

  /**
   * Creates a CT (Connected Topics) object.
   * A CT object is similar in scope to a UM Context object, and is associated with a context.
   * Many applications have only one context, and therefore one CT.
   * A single CT can support multiple connected sources and receivers.
   * <p>
   * This constructor only creates the object.
   * Its full initialization is deferred until its {@link #start} method is called.
   */
  public LbmCt() {
  }

  // Getters.
  LBMContext getCtx() { return ctx; }
  LbmCtConfig getConfig() { return config; }
  ByteBuffer getMetadata() { return metadata; }
  LBMSourceSendExInfo getSrcExInfo() { return srcExInfo; }
  LbmCtCtrlr getCtrlr() { return ctrlr; }
  int getLocalRequestPort() { return localRequestPort; }
  int getLocalDomainId() { return localDomainId; }
  int getLocalIpAddr() { return localIpAddr; }
  int getCtId() { return ctId; }

  /**
   * Initializes a CT object.
   * This is typically called immediately after the object is created ({@link #LbmCt()} constructor).
   * When <tt>start</tt> returns, the CT is ready for connected source and receiver creation.
   * <p>
   * When the application is finished using Connected Topics functionality, the CT should be stopped
   * ({@link #stop} API).
   * <p>
   * @param inCtx  UM context associated with the CT.
   * @param inConfig  Configuration options for CT.
   *     The CT object retains a reference to the passed-in configuration object.
   *     If multiple CT objects are created in a single process, separate configuration object instances should be
   *     used for each CT.
   * @param inMetadata  Application-specific data, delivered to the remote connecting peers.
   *     This metadata must be wrapped in a ByteBuffer and flipped so that it can be read by the CT.
   *     CT does not retain a reference to the inMetadata object; a deep copy of the data is made.
   *     If the application makes changes to the contents of <tt>inMetadata</tt>, those changes will not be
   *     seen by CT.
   * @throws Exception  LBMException thrown.
   */
  public void start(LBMContext inCtx, LbmCtConfig inConfig, ByteBuffer inMetadata) throws Exception {
    ctx = inCtx;
    if (inConfig == null) {
      throw new LBMException("config must be non-null");
    }
    config = inConfig;

    // Generate a random 32-bit ID from system entropy.  Used to make a unique ID for this LbmCt instance.
    SecureRandom secRand = new SecureRandom();
    byte[] randomSeed = secRand.generateSeed(4);
    // Make the 4 byte into an int.
    ByteBuffer randomByteBuf = ByteBuffer.wrap(randomSeed);
    ctId = randomByteBuf.getInt();
    if (ctId == 0) {
      throw new LBMException("generateSeed failed");
    }

    if (inMetadata == null) {  // Empty metadata.
      metadata = ByteBuffer.allocate(1);
      metadata.clear();
      metadata.flip();
    } else {
      if (inMetadata.position() != 0) {
        throw new LBMException("Metadata byte buffer position should be 0, missing flip?");
      }

      // Make a deep copy of the user's metadata.  Don't assume position is zero.  Data
      // is between position and limit, which would normally be 0..limit.
      int oldPosition = inMetadata.position();
      int metadataLen = inMetadata.remaining();
      metadata = ByteBuffer.allocate(metadataLen);
      // Read the data out of the passed-in metadata.
      inMetadata.get(metadata.array(), 0, metadataLen);
      metadata.position(metadataLen);
      inMetadata.position(oldPosition);  // Restore input metadata byte buffer (don't "consume" it).
      metadata.flip();
    }

    localUimAddr();

    srcExInfo = new LBMSourceSendExInfo();
    LBMMessageProperties msgProps = new LBMMessageProperties();
    msgProps.set(HANDSHAKE_TOPIC_STR, 1);
    srcExInfo.setMessageProperties(msgProps);

    handshakeParser = new LbmCtHandshakeParser(this.metadata.remaining());

    ctSrcMap = new HashMap<>();
    srcConnMap = new HashMap<>();

    ctSrcSet = new HashSet<>();
    ctRcvSet = new HashSet<>();

    // Create and start ct control thread.
    ctrlr = new LbmCtCtrlr(this);
    ctrlr.start();

    // Create UM receiver for source-side reception of handshake messages.
    srcHandshakeRcvCreate();
  }


  /**
   * Stops processing Connected Topics messages and frees resources.
   * All CT sources and receivers must already be stopped.
   * <p>
   * @throws Exception  LBMException thrown.
   */
  // THREAD: user
  public void stop() throws Exception {
    if (! ctSrcSet.isEmpty()) {
      throw new LBMException("Must delete sources and receivers");
    }
    if (! ctRcvSet.isEmpty()) {
      throw new LBMException("Must delete receives and sources");
    }

    // CT sources and receivers are gone.  Tear down everything else.
    umHandshakeRcv.close();  // Delete the UM receiver.
    ctrlr.exit();
    ctrlr.join();

    // Remove references to everything.
    ctx = null;  // Initialized in constructor.
    config = null;
    metadata = null;
    srcExInfo = null;
    ctrlr = null;
    umHandshakeRcv = null;
    handshakeParser = null;
    ctSrcMap = null;
    srcConnMap = null;
    ctSrcSet = null;
    ctRcvSet = null;
  }


  // Send a test message into the ct control thread and get the response back.
  // THREAD: user
  void ctTest(String logText, int testErr) throws Exception {
    LbmCtCtrlrCmd cmd = ctrlr.cmdGet();
    cmd.setTest(this, logText, testErr);
    ctrlr.submitWait(cmd);  // This "calls" cmdTest below.

    Exception e = cmd.getE();
    ctrlr.cmdFree(cmd);  // Return command object to free pool.
    if (e != null) {
      throw (new Exception(e));
    }
  }

  // THREAD: ctrlr
  boolean cmdTest(LbmCtCtrlrCmd cmd) throws Exception {
    // For testing, throw an exception if the string is null.  This lets the test program verify proper exception flow.
    int testErr = cmd.getTestErr();
    String testStr = cmd.getTestStr();
    if (testErr != 0) {
      throw new LBMException("cmdTest: " + testStr);
    }
    LBMPubLog.pubLog(LBM.LOG_INFO, "cmdTest: " + cmd.getTestStr() + "\n");
    return true;
  }

  void addToSrcMap(LbmCtSrc ctSrc) {
    ctSrcMap.put(ctSrc.getTopicStr(), ctSrc);
  }

  void removeFromSrcMap(LbmCtSrc ctSrc) {
    ctSrcMap.remove(ctSrc.getTopicStr(), ctSrc);
  }

  // When a source-side connection to a remote receiver is established, it is added to the map.
  private void addToSrcConnMap(LbmCtSrcConn srcConn) {
    srcConnMap.put(srcConn.getRcvConnKey(), srcConn);
  }

  // When a source-side connection to a remote receiver is stopped, it is removed from the map.
  void removeFromSrcConnMap(LbmCtSrcConn srcConn) {
    srcConnMap.remove(srcConn.getRcvConnKey());
  }

  void addToCtSrcSet(LbmCtSrc ctSrc) {
    ctSrcSet.add(ctSrc);
  }

  void removeFromCtSrcSet(LbmCtSrc ctSrc) {
    ctSrcSet.remove(ctSrc);
  }

  void addToCtRcvSet(LbmCtRcv ctRcv) {
    ctRcvSet.add(ctRcv);
  }

  void removeFromCtRcvSet(LbmCtRcv ctRcv) {
    ctRcvSet.remove(ctRcv);
  }

  // Determine our context's UIM destination address by looking at the context config attrlbutes.
  // The destination is loaded into binary variables "localDomainId", "localRequestPort", and "localIpAddr".
  // THREAD: user
  private void localUimAddr() throws Exception {
    String is_bound = ctx.getAttributeValue("request_tcp_bind_request_port");
    if (!is_bound.contentEquals("1")) {
      throw new LBMException("context request_tcp_bind_request_port must not be 0");
    }

    localRequestPort = Integer.parseInt(ctx.getAttributeValue("request_tcp_port"));

    localDomainId = config.getDomainId();

    String localIpAddrStr = ctx.getAttributeValue("request_tcp_interface");
    if (localIpAddrStr.contentEquals("0.0.0.0")) {
      // No unicast interface specified, so any "should" work.  Use the
      // multicast resolver interface, since that defaults to *something*.
      localIpAddrStr = ctx.getAttributeValue("resolver_multicast_interface");
    }

    // Have a string form of ip address.  Change to big endian integer (first octect as MSB).
    // Write each octet as a byte to a byte buffer, then read it back as an int.
    Scanner ipScanner = new Scanner(localIpAddrStr).useDelimiter("\\.");
    ByteBuffer ipByteBuf = ByteBuffer.allocate(4);
    for (int i = 0; i < 4; i++) {
      ipByteBuf.put((byte)ipScanner.nextInt());
    }
    ipByteBuf.flip();  // Prepare to re-read it back as an int.
    localIpAddr = ipByteBuf.getInt();
  }

  // As ctRcv and ctSrc connections are created, assign them simple incrementing connection IDs.
  // THREAD: ctrlr
  int nextConnId() {
    int connId;
    synchronized (this) {
      connId = nextConnId;
      nextConnId ++;
    }
    return connId;
  }


  /*
   * Create the UM receiver for reception of handshake messages sent to the source.
   * Since these messages are sent as UIMs, disable all for this receiver.
   * THREAD: user
   */
  private void srcHandshakeRcvCreate() throws Exception {
    LBMReceiverAttributes rcvAttr = new LBMReceiverAttributes();
    rcvAttr.setProperty("resolver_query_minimum_initial_interval", "0");
    rcvAttr.setProperty("resolver_query_maximum_initial_interval", "0");
    rcvAttr.setProperty("resolver_query_sustain_interval", "0");
    rcvAttr.setProperty("resolution_number_of_sources_query_threshold", "0");
    LBMTopic topicObj = ctx.lookupTopic(LbmCt.HANDSHAKE_TOPIC_STR, rcvAttr);

    LBMReceiverCallback srcSideMsgRcvCb = new SrcSideMsgRcvCb(this, ctrlr);
    umHandshakeRcv = new LBMReceiver(ctx, topicObj, srcSideMsgRcvCb, null);
    rcvAttr.dispose();
  }

  /*
   * Implementation of LBMReceiver callback interface for UM receiver events.
   * See https://ultramessaging.github.io/currdoc/doc/JavaAPI/interfacecom_1_1latencybusters_1_1lbm_1_1LBMReceiverCallback.html
   */
  private static class SrcSideMsgRcvCb implements LBMReceiverCallback {
    LbmCt ct;
    LbmCtCtrlr ctrlr;

    private SrcSideMsgRcvCb(LbmCt ct, LbmCtCtrlr ctrlr) {
      this.ct = ct;
      this.ctrlr = ctrlr;
    }

    /*
     * Not part of public API.  Declared public to conform to interface.
     * THREAD: ctx
     */
    public int onReceive(Object cbArgs, LBMMessage umMsg) {
      try {
        ct.srcHandshake(umMsg);
      } catch (Exception e) {
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCt::onReceive: ct.srcHandshake: " + LbmCt.exDetails(e) + "\n");
      }
      umMsg.dispose();  // The message data was copied out of msg, so the msg can be disposed (for ZOD).
      return 0;
    }
  }

  /*
   * The code below conceptually belongs with LbmCtSrc, but first we need to determine
   * which instance this message belongs to (or a new instance).
   * THREAD: ctx
   */
  private void srcHandshake(LBMMessage umMsg) {
    if (umMsg.type() == LBM.MSG_DATA) {
      LbmCtCtrlrCmd cmd = ctrlr.cmdGet();
      cmd.setSrcHandshake(this, umMsg);  // Copy the message data out of msg.
      ctrlr.submitNowait(cmd);  // This "calls" cmdSrcHandshake below.
    }
  }

  // THREAD: ctrlr
  boolean cmdSrcHandshake(LbmCtCtrlrCmd cmd) throws Exception {
    ByteBuffer msgData = cmd.getMsgData();
    int handshakeType = handshakeParser.parse(msgData);
    switch (handshakeType) {
      case LbmCtHandshakeParser.MSG_TYPE_CREQ:
        srcHandshakeCreq(handshakeParser);
        break;
      case LbmCtHandshakeParser.MSG_TYPE_COK:
        srcHandshakeCok(handshakeParser);
        break;
      case LbmCtHandshakeParser.MSG_TYPE_DREQ:
        srcHandshakeDreq(handshakeParser);
        break;
      case LbmCtHandshakeParser.MSG_TYPE_DOK:
        srcHandshakeDok(handshakeParser);
        break;
      default:
        throw (new LBMException("ct source got unexpected handshake " + handshakeType));
    }
    handshakeParser.clear();
    return true;
  }

  // THREAD: ctrlr
  private void srcHandshakeCreq(LbmCtHandshakeParser handshakeParser) throws Exception {
    LbmCtSrcConn srcConn;

    if (srcConnMap.containsKey(handshakeParser.getRcvConnKey())) {
      srcConn = srcConnMap.get(handshakeParser.getRcvConnKey());
      if (srcConn.getCtSrc().isStopping()) {
        throw new LBMException("Got creq on exiting src");
      }
    } else { // No src connection yet, create one.
      // Find ctSrc object for this topic.
      String topicStr = handshakeParser.getTopicStr();
      if (! ctSrcMap.containsKey(topicStr)) {
        throw new LBMException("CREQ recvd for topic '" + topicStr + "' with no src");
      }
      LbmCtSrc ctSrc = ctSrcMap.get(topicStr);
      if (ctSrc.isStopping()) {
        throw new LBMException("Got creq on exiting src");
      }

      // Create srcConn object and register it.
      srcConn = new LbmCtSrcConn(ctSrc);
      srcConn.start(handshakeParser);

      addToSrcConnMap(srcConn);
    }

    // Have srcConn.  Continue handling CREQ message from inside the conn object.
    srcConn.handleCreq(handshakeParser);
  }

  // THREAD: ctrlr
  private void srcHandshakeCok(LbmCtHandshakeParser handshakeParser) throws Exception {
    LbmCtSrcConn srcConn;

    if (srcConnMap.containsKey(handshakeParser.getRcvConnKey())) {
      srcConn = srcConnMap.get(handshakeParser.getRcvConnKey());
      // Have srcConn.  Continue handling COK message from inside the conn object.
      srcConn.handleCok(handshakeParser);
    } else {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LbtCt::srcHandshakeCok: connection not found  for COK handshake received on topic " + handshakeParser.getTopicStr() + "\n");
    }
  }

  // THREAD: ctrlr
  private void srcHandshakeDreq(LbmCtHandshakeParser handshakeParser) throws Exception {
    LbmCtSrcConn srcConn;

    if (srcConnMap.containsKey(handshakeParser.getRcvConnKey())) {
      srcConn = srcConnMap.get(handshakeParser.getRcvConnKey());
      // Have srcConn.  Continue handling DREQ message from inside the conn object.
      srcConn.handleDreq(handshakeParser);
    } else {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LbtCt::srcHandshakeDreq: DREQ handshake received for topic " + handshakeParser.getTopicStr() + " with no connection\n");
    }
  }

  // THREAD: ctrlr
  private void srcHandshakeDok(LbmCtHandshakeParser handshakeParser) throws Exception {
    LbmCtSrcConn srcConn;

    if (srcConnMap.containsKey(handshakeParser.getRcvConnKey())) {
      srcConn = srcConnMap.get(handshakeParser.getRcvConnKey());
      // Have srcConn.  Continue handling DOK message from inside the conn object.
      srcConn.handleDok(handshakeParser);
    } else {
      LBMPubLog.pubLog(LBM.LOG_INFO, "LbtCt::srcHandshakeDok: DOK handshake received for topic " + handshakeParser.getTopicStr() + " with no connection\n");
    }
  }

  // Utility to get an exception's error message and stack trace as a string.
  static String exDetails(Exception e) {
    // Put stack trace into string writer "sw".
    StringWriter sw = new StringWriter();
    PrintWriter pw = new PrintWriter(sw);
    e.printStackTrace(pw);
    // Return error message and stack trace.
    return sw.toString();
  }

  void dbg(String msg) {
    if ((config.getTestBits() & LbmCtConfig.TEST_BITS_DEBUG) == LbmCtConfig.TEST_BITS_DEBUG) {
      ctrlr.debugQ.add(msg);
    }
  }
}
