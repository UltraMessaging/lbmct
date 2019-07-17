/*
"MinCtSrc.java - Minimal Connected Topics source program.
 * See https://github.com/UltraMessaging/lbmct for code and documentation.
 *
 * Copyright (c) 2018-2019 Informatica Corporation. All Rights Reserved.
 * Permission is granted to licensees to use or alter this software for
 * any purpose, including commercial applications, according to the terms
 * laid out in the Software License Agreement.
 -
 - This source code example is provided by Informatica for educational
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
import java.nio.charset.StandardCharsets;
import com.latencybusters.lbm.*;
import com.latencybusters.lbmct.*;

class MinCtSrc {
  // Have "main" create an object instance for this program and run it.
  private void run(String[] args) throws Exception {
    // Create context, making sure "response_tcp_nodelay" is set (speed up).
    LBMContextAttributes myCtxAttr = new LBMContextAttributes();
    myCtxAttr.setValue("response_tcp_nodelay", "1");
    LBMContext myCtx = new LBMContext(myCtxAttr);
    myCtxAttr.dispose();  // Recommended UM best practice.

    // Create a ByteBuffer of application metadata.
    ByteBuffer myMetadata = ByteBuffer.allocate(100);
    myMetadata.put("MySrcMeta".getBytes());  // Use a simple string as metadata.
    myMetadata.flip();    // Done writing metadata, present it to CT for reading.

    // Create CT object with a minimal configuration.
    LbmCtConfig myConfig = new LbmCtConfig();
    LbmCt myCt = new LbmCt();  // This just creates the object.
    myCt.start(myCtx, myConfig, myMetadata);  // Initialize the CT object.

    // Prepare to create the connected source.
    LBMSourceAttributes mySrcAttr = null;  // Or new LBMSourceAttributes();
    // Instantiate your source event, connect, disconnect callback objects.
    LBMSourceEventCallback myUmSrcEventCb = null;  // Not used in this example.
    MySrcConnCreateCbClass mySrcCreateCbObj = new MySrcConnCreateCbClass();
    MySrcConnDeleteCbClass mySrcDeleteCbObj = new MySrcConnDeleteCbClass();
    // Object passed as "cbArg" to myUmSrcEventCb, mySrcCreateCbObj, and mySrcDeleteCbObj.
    Object mySrcCbArg = null;
    // Create and initialize the connected source.
    LbmCtSrc myCtSrc = new LbmCtSrc();  // This just creates the object.
    myCtSrc.start(myCt, "myTopic", mySrcAttr, myUmSrcEventCb, mySrcCreateCbObj, mySrcDeleteCbObj, mySrcCbArg);

    for (int i = 0; i < 10; i++) {
      Thread.sleep(1000);
      System.out.println("Sending...");
      myCtSrc.getUmSrc().send("TestMsg".getBytes(), 7, LBM.MSG_FLUSH);
    }

    // Clean up.
    myCtSrc.stop();  // Important to stop the CT Source when finished.
    myCt.stop();  //  Important to stop the CT object when finished.
  }


  // Callback for when a CT receiver connects to this source.
  class MySrcConnCreateCbClass implements LbmCtSrcConnCreateCallback {
    @Override
    public Object onSrcConnCreate(LbmCtSrc ctSrc, LbmCtPeerInfo peerInfo, Object cbArg) {
      // Put your connect code here.
      try {
        System.out.println("onSrcConnCreate: rcv metadata=" +
            new String(peerInfo.getRcvMetadata().array(), StandardCharsets.ISO_8859_1));
      } catch (Exception e) { System.out.println("Exception: " + e.toString()); }

      // It is typical to return an application-specific, per-connection state object here.
      // It is passed to the connection delete callback.
      return null;
    }
  }


  // Callback for when a CT receiver disconnects from this source.
  class MySrcConnDeleteCbClass implements LbmCtSrcConnDeleteCallback {
    @Override
    public void onSrcConnDelete(LbmCtSrc ctSrc, LbmCtPeerInfo peerInfo, Object cbArg, Object connCbArg) {
      // Put your disconnect code here.  Note that connCbArg is the object returned by onSrcConnCreate.
      try {
        System.out.println("onSrcConnDelete: rcv metadata=" +
            new String(peerInfo.getRcvMetadata().array(), StandardCharsets.ISO_8859_1));
      } catch (Exception e) { System.out.println("Exception: " + e.toString()); }
    }
  }


  public static void main(String[] args) throws Exception {
    MinCtSrc application = new MinCtSrc();
    application.run(args);
  }
}
