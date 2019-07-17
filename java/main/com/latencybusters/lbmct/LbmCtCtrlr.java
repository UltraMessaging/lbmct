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
import com.latencybusters.lbm.*;

class LbmCtCtrlr extends Thread {
  enum States {
    STARTING, RUNNING, EXITING
  }

  private final static int CTRLR_NUM_CMD_NODES = 32;

  private LbmCt ct;
  private States ctrlrState;
  private LinkedBlockingQueue<LbmCtCtrlrCmd> cmdWorkQ;
  private LinkedBlockingQueue<LbmCtCtrlrCmd> cmdFreeQ;
  // The "debugQ" is typically used interactively from a debugger.  A test application can set TEST_BITS_DEBUG in the
  // config, and various places in the code will record interesting information in the debugQ.  I just use the
  // debugger to examine its contents.
  LinkedBlockingQueue<String> debugQ;

  // Constructor.
  LbmCtCtrlr(LbmCt inCt) {
    ct = inCt;
    ctrlrState = States.STARTING;

    // Create work and free queues.
    cmdWorkQ = new LinkedBlockingQueue<>();
    cmdFreeQ = new LinkedBlockingQueue<>();
    for (int i = 0; i < CTRLR_NUM_CMD_NODES; i++) {
      cmdFreeQ.add(new LbmCtCtrlrCmd(inCt));
    }
    debugQ = new LinkedBlockingQueue<>();
  }

  // Get a command object from the free pool.
  LbmCtCtrlrCmd cmdGet() {
    LbmCtCtrlrCmd cmd = cmdFreeQ.poll();
    if (cmd == null) {
      // Ran out of commands; allocate another.
      cmd = new LbmCtCtrlrCmd(ct);
    }
    return cmd;
  }

  // Return a command object to the free pool.
  void cmdFree(LbmCtCtrlrCmd cmd) {
    cmd.clear();  // Release any resources held by this command.
    cmdFreeQ.add(cmd);
  }

  // Send a command object to the controller thread without waiting for completion.
  void submitNowait(LbmCtCtrlrCmd cmd) {
    cmd.setCmdCompletion(CmdCompletions.NOWAIT);
    cmdWorkQ.add(cmd);
  } // submitNowait()

  // Send a command object to the controller thread; wait for completion.
  void submitWait(LbmCtCtrlrCmd cmd) throws Exception {
    cmd.setCmdCompletion(CmdCompletions.WAIT);
    cmdWorkQ.add(cmd);

    // Wait for completion.
    cmd.getCompleteSem().acquire();
  } // submitWait()

  // General command completion function (called from ctrlr thread).
  // THREAD: ctrlr
  void cmdComplete(LbmCtCtrlrCmd cmd) {
    switch (cmd.getCmdCompletion()) {
      case NOWAIT:
        if (cmd.isErr()) {
          LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtCtrlr.complete: async cmd error: " + cmd.getErrmsg() + "\n");
        }
        cmdFreeQ.add(cmd);  // Return command object to free pool.
        break;

      case WAIT:
        // Wake up application thread.
        cmd.getCompleteSem().release();
        break;

      default:
        if (cmd.isErr()) {
          LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtCtrlrCmd.complete: cmd error: " + cmd.getErrmsg() + "\n");
        }
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtCtrlrCmd.complete: ERROR!!! Unimplemented cmdCompletion: " + cmd.getCmdCompletion() + "\n");
    } // switch
  } // complete()

  void exit() throws Exception {
    LbmCtCtrlrCmd cmd = cmdGet();
    cmd.setQuit();
    submitWait(cmd);  // This "calls" cmdQuit below.

    Exception e = cmd.getE();
    cmdFree(cmd);  // return command object to free pool.
    if (e != null) {
      throw(new Exception(e));
    }
  }

  // THREAD: ctrlr
  private boolean cmdQuit(@SuppressWarnings("unused") LbmCtCtrlrCmd cmd) {
    ctrlrState = States.EXITING;
    return true;
  }

  // Not part of public API
  // THREAD: ctrlr
  @Override
  public void run() {
    ctrlrState = States.RUNNING;
    LbmCtCtrlrCmd cmd;
    ct.dbg("LbmCtCtrlr::run");

    while (ctrlrState == States.RUNNING) {
      // Get a command to execute.
      try {
        cmd = cmdWorkQ.take();
      } catch (InterruptedException e) {
        // Some other thread iterrupted us; exit.
        LBMPubLog.pubLog(LBM.LOG_WARNING, "LbmCtCtrlr::run: error taking from work queue: " + LbmCt.exDetails(e) + "\n");
        ctrlrState = States.EXITING;
        break;
      }

      boolean cmdComplete;
      try {
        switch (cmd.getCmdType()) {
          case TEST:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getCt());
            cmdComplete = cmd.getCt().cmdTest(cmd);
            break;
          case QUIT:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + this);
            cmdComplete = cmdQuit(cmd);
            break;
          case CT_SRC_START:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getCtSrc());
            cmdComplete = cmd.getCtSrc().cmdCtSrcStart(cmd);
            break;
          case CT_SRC_STOP:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getCtSrc());
            cmdComplete = cmd.getCtSrc().cmdCtSrcStop(cmd);
            break;
          case CT_SRC_UM_SOURCE_STOP:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getCtSrc());
            cmdComplete = cmd.getCtSrc().cmdCtSrcUmSourceStop(cmd);
            break;
          case CT_SRC_FINAL_STOP:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getCtSrc());
            cmdComplete = cmd.getCtSrc().cmdCtSrcFinalStop(cmd);
            break;
          case SRC_HANDSHAKE:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getCt());
            cmdComplete = cmd.getCt().cmdSrcHandshake(cmd);
            break;
          case SRC_CONN_TMR_EXPIRE:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getSrcConn());
            cmdComplete = cmd.getSrcConn().cmdSrcConnTmrExpire(cmd);
            break;
          case SRC_CONN_FINAL_STOP:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getSrcConn());
            cmdComplete = cmd.getSrcConn().cmdSrcConnFinalStop(cmd);
            break;
          case CT_RCV_START:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getCtRcv());
            cmdComplete = cmd.getCtRcv().cmdCtRcvStart(cmd);
            break;
          case CT_RCV_STOP:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getCtRcv());
            cmdComplete = cmd.getCtRcv().cmdCtRcvStop(cmd);
            break;
          case RCV_CONN_START:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getRcvConn());
            cmdComplete = cmd.getRcvConn().cmdRcvConnStart(cmd);
            break;
          case RCV_CONN_TMR_EXPIRE:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getRcvConn());
            cmdComplete = cmd.getRcvConn().cmdRcvConnTmrExpire(cmd);
            break;
          case RCV_CONN_STOP:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getRcvConn());
            cmdComplete = cmd.getRcvConn().cmdRcvConnStop(cmd);
            break;
          case RCV_CONN_FINAL_STOP:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getRcvConn());
            cmdComplete = cmd.getRcvConn().cmdRcvConnFinalStop(cmd);
            break;
          case RCV_SEND_COK:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getRcvConn());
            cmdComplete = cmd.getRcvConn().cmdRcvSendCok(cmd);
            break;
          case RCV_SEND_DOK:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getRcvConn());
            cmdComplete = cmd.getRcvConn().cmdRcvSendDok(cmd);
            break;
          case RCV_CONN_DISCONNECT:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getRcvConn());
            cmdComplete = cmd.getRcvConn().cmdRcvConnDisconnect(cmd);
            break;
          case CT_RCV_UM_RECEIVER_STOP:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getCtRcv());
            cmdComplete = cmd.getCtRcv().cmdCtRcvUmReceiverStop(cmd);
            break;
          case CT_RCV_FINAL_STOP:
            ct.dbg("dequeue cmd: " + cmd.getCmdType() + ", " + cmd.getCtRcv());
            cmdComplete = cmd.getCtRcv().cmdCtRcvFinalStop(cmd);
            break;
          default:
            throw new LBMException("LbmCt: Unkown command: " + cmd.getCmdType());
        }
      } catch (Exception e) {
        cmd.setE(e);
        cmdComplete = true;
      }

      if (cmdComplete) {
        cmdComplete(cmd);
      }
    } // while state running
  } // run()
} // class LbmCtCtrlr