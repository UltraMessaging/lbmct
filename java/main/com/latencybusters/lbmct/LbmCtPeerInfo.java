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

@SuppressWarnings("WeakerAccess")  // public API.
public class LbmCtPeerInfo {
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int STATUS_OK=0;
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int STATUS_BAD_STOP =-1;


  // Flag bits for "flags" field.
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int FLAGS_SRC_METADATA      = 0x01;
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int FLAGS_RCV_METADATA      = 0x02;
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int FLAGS_RCV_SOURCE_STR    = 0x04;
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int FLAGS_RCV_START_SEQ_NUM = 0x08;
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int FLAGS_RCV_END_SEQ_NUM   = 0x10;

  private int status = STATUS_BAD_STOP;
  private int flags = 0;
  private ByteBuffer srcMetadata = null;
  private ByteBuffer rcvMetadata = null;
  private long rcvStartSequenceNumber = 0;
  private long rcvEndSequenceNumber = 0;
  private String rcvSourceStr = null;

  // No constructor needed.

  @SuppressWarnings("WeakerAccess")  // public API.
  public int getStatus() {
    return status;
  }
  @SuppressWarnings("WeakerAccess")  // public API.
  public int getFlags() { return flags; }
  @SuppressWarnings("WeakerAccess")  // public API.
  public ByteBuffer getSrcMetadata() throws Exception {
    if ((flags & FLAGS_SRC_METADATA) == 0) {
      throw new LBMException("srcMetadata undefined");
    }
    return srcMetadata;
  }
  @SuppressWarnings("WeakerAccess")  // public API.
  public ByteBuffer getRcvMetadata() throws Exception {
    if ((flags & FLAGS_RCV_METADATA) == 0) {
      throw new LBMException("rcvMetadata undefined");
    }
    return rcvMetadata;
  }
  @SuppressWarnings("WeakerAccess")  // public API.
  public long getRcvStartSequenceNumber() throws Exception {
    if ((flags & FLAGS_RCV_START_SEQ_NUM) == 0) {
      throw new LBMException("rcvStartSequenceNumber undefined");
    }
    return rcvStartSequenceNumber;
  }
  @SuppressWarnings("WeakerAccess")  // public API.
  public long getRcvEndSequenceNumber() throws Exception {
    if ((flags & FLAGS_RCV_END_SEQ_NUM) == 0) {
      throw new LBMException("rcvEndSequenceNumber undefined");
    }
    return rcvEndSequenceNumber;
  }
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
