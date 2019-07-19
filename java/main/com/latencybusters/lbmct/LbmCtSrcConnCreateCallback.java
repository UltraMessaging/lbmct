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

/**
 * Callback interface to deliver the "connection created" event to a source application.
 * The application supplies an implementation of this interface to {@link LbmCtSrc#start}.
 */
public interface LbmCtSrcConnCreateCallback {
  /**
   * Source application callback method, called by CT, to indicate that a new connection is created.
   * <p>
   * @param src  The CT Source object that the new connection applies to.
   * @param peerInfo  Useful information about the connection, such as the metadata of the connected receiver.
   * @param cbArg  Application-specific object supplied by the application to {@link LbmCtSrc#start}.
   * @return Application-specific object to be associated with the connection, which is made available to the
   *     application when messages are delivered, and {@link LbmCtSrcConnDeleteCallback#onSrcConnDelete}.
   *     This allows per-connection state to be maintained.
   */
  Object onSrcConnCreate(LbmCtSrc src, LbmCtPeerInfo peerInfo, Object cbArg);
}  // LbmCtSrcConnCreateCallback
