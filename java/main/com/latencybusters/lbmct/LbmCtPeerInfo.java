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
 * A passive object that provides useful information to a Connected Topics application.
 * The object is offered to the application at the connect and disconnect events.
 * There are multiple fields of potential interest in the object, but they are not all valid at all times.
 * For example, the "end" sequence number is not known when a connection is first made.
 * The {@link #getFlags} method allows retrieval of which fields are available.
 */
@SuppressWarnings("WeakerAccess")  // public API.
public class LbmCtPeerInfo {
  /**
   * Value for {@link #getStatus} indicating that the connection operation was successful.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int STATUS_OK = 0;
  /**
   * Value for {@link #getStatus} indicating that the connection disconnected without proper handshaking.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int STATUS_BAD_DISCONNECT = -1;


  /**
   * Bit mask indicating that it is valid to call {@link #getSrcMetadata} to retrieve the source's metadata.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int FLAGS_SRC_METADATA      = 0x01;
  /**
   * Bit mask indicating that it is valid to call {@link #getRcvMetadata} to retrieve the receiver's metadata.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int FLAGS_RCV_METADATA      = 0x02;
  /**
   * Bit mask indicating that it is valid to call {@link #getRcvSourceStr} to retrieve the source string (from the
   * receiver's point of view).
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int FLAGS_RCV_SOURCE_STR    = 0x04;
  /**
   * Bit mask indicating that it is valid to call {@link #getRcvStartSequenceNumber} to retrieve the message sequence
   * number of the first message delivered to the receiver after the connection has been established.
   * Note that this will be the sequence number of the CRSP handshake message, not an application message.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int FLAGS_RCV_START_SEQ_NUM = 0x08;
  /**
   * Bit mask indicating that it is valid to call {@link #getRcvStartSequenceNumber} to retrieve the message sequence
   * number of the last message delivered to the receiver before the connection was deleted.
   * Note that this will be the sequence number of the DRSP handshake message, not an application message.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int FLAGS_RCV_END_SEQ_NUM   = 0x10;

  private int status = STATUS_BAD_DISCONNECT;
  private int flags = 0;
  private ByteBuffer srcMetadata = null;
  private ByteBuffer rcvMetadata = null;
  private long rcvStartSequenceNumber = 0;
  private long rcvEndSequenceNumber = 0;
  private String rcvSourceStr = null;

  // No constructor needed.

  /**
   * Get the status of the connection.
   * Possible values: {@link #STATUS_OK}, {@link #STATUS_BAD_DISCONNECT}.
   * @return  Status of the connection.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public int getStatus() {
    return status;
  }

  /**
   * Get a bitmask of flags indicating which peer data elements are available to be read.
   * The return value can be ANDed with the the following bit masks corresponding to the individual data elements:
   * {@link #FLAGS_SRC_METADATA}, {@link #FLAGS_RCV_METADATA}, {@link #FLAGS_RCV_SOURCE_STR},
   * {@link #FLAGS_RCV_START_SEQ_NUM}, {@link #FLAGS_RCV_END_SEQ_NUM}.
   * @return  Bitmask of flags.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public int getFlags() { return flags; }

  /**
   * Get the connected source's metadata.
   * @return  Reference to ByteBuffer containing the source's metadata.
   * @throws Exception  Throws LBMException.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public ByteBuffer getSrcMetadata() throws Exception {
    if ((flags & FLAGS_SRC_METADATA) == 0) {
      throw new LBMException("srcMetadata undefined");
    }
    return srcMetadata;
  }

  /**
   * Get the connected receiver's metadata.
   * @return  Reference to ByteBuffer containing the source's metadata.
   * @throws Exception  Throws LBMException.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public ByteBuffer getRcvMetadata() throws Exception {
    if ((flags & FLAGS_RCV_METADATA) == 0) {
      throw new LBMException("rcvMetadata undefined");
    }
    return rcvMetadata;
  }

  /**
   * Get the message sequence number of the first message delivered to the receiver when the connection was created.
   * Note that this will be a CT handshake message (CRSP), not an application message.
   * @return  Message sequence number.
   * @throws Exception  Throws LBMException.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public long getRcvStartSequenceNumber() throws Exception {
    if ((flags & FLAGS_RCV_START_SEQ_NUM) == 0) {
      throw new LBMException("rcvStartSequenceNumber undefined");
    }
    return rcvStartSequenceNumber;
  }

  /**
   * Get the message sequence number of the last message delivered to the receiver as the connection was deleted.
   * Note that this will be a CT handshake message (DRSP), not an application message.
   * @return  Message sequence number.
   * @throws Exception  Throws LBMException.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public long getRcvEndSequenceNumber() throws Exception {
    if ((flags & FLAGS_RCV_END_SEQ_NUM) == 0) {
      throw new LBMException("rcvEndSequenceNumber undefined");
    }
    return rcvEndSequenceNumber;
  }

  /**
   * Get the source string associated with the connected source.
   * This information is only available to the receiver, and is the same string as the <tt>sourceName</tt> parameter
   * to <a href="https://ultramessaging.github.io/currdoc/doc/JavaAPI/interfacecom_1_1latencybusters_1_1lbm_1_1LBMSourceCreationCallback.html#abd0aa98651acef597b50963ddc6b8a8d">onNewSource</a>
   * UM callback and the string returned by
   * <a href="https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMMessage.html#a5691adf8b8b740c7813d2f8d605c394d">LBMMessage.source</a>.
   * <p>
   * @return  UM source string.
   * @throws Exception  throws LBMException.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public String getRcvSourceStr() throws Exception {
    if ((flags & FLAGS_RCV_SOURCE_STR) == 0) {
      throw new LBMException("rcvSourceStr undefined");
    }
    return rcvSourceStr;
  }

  void setStatus(int status) { this.status = status; }

  void setSrcMetadata(ByteBuffer inMetadata) {
    // Make a deep copy of the metadata.
    int oldPosition = inMetadata.position();
    int len = inMetadata.remaining();
    srcMetadata = ByteBuffer.allocate(len);
    inMetadata.get(srcMetadata.array(), 0, len);
    srcMetadata.position(len);
    srcMetadata.flip();

    inMetadata.position(oldPosition);  // Restore input metadata byte buffer.
    flags = flags | FLAGS_SRC_METADATA;
  }
  void setRcvMetadata(ByteBuffer inMetadata) {
    // Make a deep copy of the metadata.
    int oldPosition = inMetadata.position();
    int len = inMetadata.limit();
    rcvMetadata = ByteBuffer.allocate(len);
    inMetadata.get(rcvMetadata.array(), 0, len);
    rcvMetadata.position(len);
    rcvMetadata.flip();

    inMetadata.position(oldPosition);  // Restore input metadata byte buffer.
    flags = flags | FLAGS_RCV_METADATA;
  }
  void setRcvStartSequenceNumber(long inStartSequenceNumber) {
    rcvStartSequenceNumber = inStartSequenceNumber;
    flags = flags | FLAGS_RCV_START_SEQ_NUM;
  }
  void setRcvEndSequenceNumber(long inEndSequenceNumber) {
    rcvEndSequenceNumber = inEndSequenceNumber;
    flags = flags | FLAGS_RCV_END_SEQ_NUM;
  }
  void setRcvSourceStr(String inSourceStr) {
    rcvSourceStr = inSourceStr;
    flags = flags | FLAGS_RCV_SOURCE_STR;
  }
}
