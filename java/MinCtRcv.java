/*
"MinCtRcv.java - Minimal Connected Topics receiver program.
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
import java.nio.charset.StandardCharsets;
import com.latencybusters.lbm.*;
import com.latencybusters.lbmct.*;

class MinCtRcv {
  public static void main(String[] args) throws Exception {
    // The body of the program is in the "run" method.
    MinCtRcv application = new MinCtRcv();
    application.run(args);
  }


  private void run(String[] args) throws Exception {
    // Create context, making sure "response_tcp_nodelay" is set (speed up).
    LBMContextAttributes myCtxAttr = new LBMContextAttributes();
    myCtxAttr.setValue("response_tcp_nodelay", "1");
    LBMContext myCtx = new LBMContext(myCtxAttr);
    myCtxAttr.dispose();  // Recommended UM best practice.

    // Create a ByteBuffer of application metadata.
    ByteBuffer myMetadata = ByteBuffer.allocate(100);
    myMetadata.put("MyRcvMeta".getBytes());  // Use a simple string as metadata.
    myMetadata.flip();    // Done writing metadata, present it to CT for reading.

    // Create CT object with a minimal configuration.
    LbmCtConfig myConfig = new LbmCtConfig();
    LbmCt myCt = new LbmCt();  // This just creates the object.
    myCt.start(myCtx, myConfig, myMetadata);  // Initialize the CT object.

    // Prepare to create the connected receiver.
    LBMReceiverAttributes myRcvAttr = null;  // Or new LBMReceiverAttributes();
    // Instantiate your receiver event, connect, disconnect callback objects.
    MyRcvCbClass myRcvCbObj = new MyRcvCbClass();
    MyRcvConnCreateCbClass myRcvCreateCbObj = new MyRcvConnCreateCbClass();
    MyRcvConnDeleteCbClass myRcvDeleteCbObj = new MyRcvConnDeleteCbClass();
    // Object passed as "cbArg" to myRcvCbObj, myRcvCreateCbObj, and myRcvDeleteCbObj.
    Object myRcvCbArg = null;
    // Create and initialize the connected receiver.
    LbmCtRcv myCtRcv = new LbmCtRcv();  // This just creates the object.
    myCtRcv.start(myCt, "myTopic", myRcvAttr, myRcvCbObj, myRcvCreateCbObj, myRcvDeleteCbObj, myRcvCbArg);

    for (int i = 0; i < 10; i++) {
      Thread.sleep(1000);
      System.out.println("Waiting...");
    }

    // Clean up.
    myCtRcv.stop();  // Important to stop the CT Receiver when finished.
    myCt.stop();  //  Important to stop the CT object when finished.
  }


  // Callback for received messages (and other receiver events).
  class MyRcvCbClass implements LBMReceiverCallback {
    @Override
    public int onReceive(Object cbArg, LBMMessage msg) {
      LbmCtRcvConn rcvConn = (LbmCtRcvConn) msg.sourceClientObject();  // CT uses the per-source client obj.
      String perConnectionStateObj = (String)rcvConn.getRcvConnCbArg();

      System.out.println("onRecieve: type=" + msg.type() +
          ", sequenceNumber=" + msg.sequenceNumber() +
          ", isHandshakeMessage=" + rcvConn.isHandshakeMessage() +
          ", perConnectionStateObj=" + perConnectionStateObj);
      return 0;
    }
  }


  // Callback for when a CT receiver connects to this receiver.
  class MyRcvConnCreateCbClass implements LbmCtRcvConnCreateCallback {
    @Override
    public Object onRcvConnCreate(LbmCtRcv ctRcv, LbmCtPeerInfo peerInfo, Object cbArg) {
      // Put your "new connection" code here.
      try {
        // Print the metadata of the source.
        System.out.println("onRcvConnCreate: src metadata=" +
            new String(peerInfo.getSrcMetadata().array(), StandardCharsets.ISO_8859_1));
        // Print the starting sequence number received (the "CRSP" handshake).
        System.out.println("onRcvConnCreate: rcv starting sequence num=" +
            peerInfo.getRcvStartSequenceNumber());
      } catch (Exception e) { System.out.println("Exception: " + e.toString()); }

      // It is typical to return an application-specific, per-connection state object here.
      // It can be retrieved in the message receiver and is passed to the connection delete callback.
      String perConnectionStateObj = "connection state";
      return perConnectionStateObj;
    }
  }


  // Callback for when a CT receiver disconnects from this receiver.
  class MyRcvConnDeleteCbClass implements LbmCtRcvConnDeleteCallback {
    @Override
    public void onRcvConnDelete(LbmCtRcv ctRcv, LbmCtPeerInfo peerInfo, Object cbArg, Object connCbArg) {
      // Put your disconnect code here.  Note that connCbArg is the object returned by onRcvConnCreate.
      try {
        // Print the ending sequence number received (the "DRSP" handshake).
        System.out.println("onRcvConnDelete: rcv ending sequence num=" +
            peerInfo.getRcvEndSequenceNumber());
      } catch (Exception e) { System.out.println("Exception: " + e.toString()); }
    }
  }
}
