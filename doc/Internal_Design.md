# lbmct v0.5 Internal Design - Connected Topics for Ultra Messaging

This page provides details on the CT implementation.

## Files

Here are the main files for CT:
* **[lbmct.h](lbmct.h)** - Public API definitions.
Should be include by applications.
* **[lbmct_private.h](lbmct_private.h)** - Private definitions used internally
by CT.
* **[prt.h](prt.h)** - Portability definitions.
Helps support CT across different platforms, especially Unix and Windows.
* **[lbm_internal.h](lbm_internal.h)** - Definitions for UM internal
functionalty used by CT.
These are used in spite of them not being official UM APIs.
Informatica will support their use within the UM GitHub version of CT.
* **[tmr.h](tmr.h)** - Public API definitions for a small wrapper around UM
timers to make them easier to manage.
This wrapper will be made available separately on GitHub as a UM example.

---
* **[lbmct.c](lbmct.c)** - Code common between sources and receivers.
Contains the main CT controller thread.
* **[lbmct_rcv.c](lbmct_rcv.c)** - Code specific to CT receivers.
* **[lbmct_src.c](lbmct_src.c)** - Code specific to CT sources.
* **[tmr.c](tmr.c)** - Code for the small wrapper around UM timers.
* **[main.cc](main.cc)** - Google Test file.

---
* **[min_ct_src.c](min_ct_src.c)** - Minimal example of a CT publisher.
* **[min_ct_rcv.c](min_ct_rcv.c)** - Minimal example of a CT subscriber.

---
* **[dllmain.cpp](dllmain.cpp)** - WINDOWS ONLY - support code for DLL.
(This file normally goes in the

* **[lbmct.vcxproj](lbmct.vcxproj)** - WINDOWS ONLY -
Project file for Microsoft Visual Studio.
Supports construction of DLL.

## Handshake Operation

### Connection Creation:

A CT Receiver discovers a CT Source and sends it a UIM handshake:
* CREQ - Connect Request.
The CT Source responds by sending:
* CRSP - Connect Response (contains publisher's metadata).
It sends CRSP over the normal data transport.
If the transport is taking an unusually long time to fully connect, that CRSP
handshake will be lost due to "head loss".
The CT Receiver will time it out and retry its CREQ.
When the CT Receiver does get the CRSP,
it calls the subscriber's connection create callback,
delivers the CRSP handshake to the subscriber,
and it replies back to the Source with a UIM handshake:
* C_OK - Connect OK (contains subscriber's metadata).
The CT Source will also time out and retry until it gets a C_OK,
at which it will call the publisher's connection create callback,
signalling to the publisher that the receiver is ready to receive handshake.

### Connection Deletion:

A normal "graceful" connection deletion starts with either the Source
or the Receiver deletes its CT source/receiver object.
Let's say the Receiver starts the process:

A CT Receiver is deleted and sends it a UIM handshake:
* DREQ - Disconnect Request.
The CT Source responds by sending:
* DRSP - Disconnect Response.
It sends DRSP over the normal data transport.
The CT Receiver delivers the DRSP handshake to the subscriber,
calls the subscriber's connection delete callback,
and it replies back to the Source with a UIM handshake:
* D_OK - Disconnect OK (contains subscriber's metadata).
When the CT source gets a D_OK,
it calls the publisher's connection delete callback.

## Software Architecture

### CT Controller Thread
Central to CT is a main CT controller thread in [lbmct.c](lbmct.c):
```
PRT_THREAD_ENTRYPOINT lbmct_ctrlr(void *arg)
```

This thread waits on a work queue for commands:
```
err = lbm_tl_queue_dequeue(ct->ctrlr_cmd_work_tlq, (void **)&cmd, 1);
```

The primary purpose of this thread and work queue is to serialize most of
non-message-path work to implement CT.
Commands structures are enqueued with the information needed to
execute the command.
Results are optionally retuned via the same command structure.

Here is an example flow of execution that illustrates the work flow and also
the naming conventions.
Starting in file [lbmct_src.c](lbmct_src.c):

```
int lbmct_src_create(lbmct_src_t **ct_srcp, ...
...
{
  lbmct_ctrlr_cmd_ct_src_create_t ct_src_create;
...
  ct_src_create.topic_str = topic_str;
...
    /* Waits for the ct control thread to complete the operation. */
  err = lbmct_ctrlr_cmd_submit_and_wait(ct,
    LBMCT_CTRLR_CMD_TYPE_CT_SRC_CREATE, &ct_src_create);
```
The `lbmct_ctrlr_cmd_submit_and_wait()` function enqueues the command
structure and waits on a semaphore for the command thread to execute
the command and signal completion.
The result is returned in the same command structure:
```
  *ct_srcp = ct_src_create.ct_src;
...
}  /* lbmct_src_create */
```

Note the naming conventions with the command.
The command is "ct_src_create".
The command structure name is constructed around it as
"`lbmct_ctrlr_cmd_ct_src_create_t`", and a command type is
constructed as "`LBMCT_CTRLR_CMD_TYPE_CT_SRC_CREATE`".

Continuing in file [lbmct.c](lbmct.c), inside the main loop, is:
```
      switch(cmd->cmd_type) {
...
      case LBMCT_CTRLR_CMD_TYPE_CT_SRC_CREATE:
        err = lbmct_ctrlr_cmd_ct_src_create(ct, cmd);
        break;
```
So the function which actually does most of the work to create a CT source
is `lbmct_ctrlr_cmd_ct_src_create()`,
which is back in file [lbmct_src.c](lbmct_src.c):
```
int lbmct_ctrlr_cmd_ct_src_create(lbmct_t *ct, lbmct_ctrlr_cmd_t *cmd)
{
  lbmct_ctrlr_cmd_ct_src_create_t *ct_src_create = cmd->cmd_data;
```

In the future, there's no need to trace through the controller thread
main loop.
The use of `LBMCT_CTRLR_CMD_TYPE_CT_SRC_CREATE` simply leads to removing
the "_TYPE_" and converting to lower case to get the function
`lbmct_ctrlr_cmd_ct_src_create()`.


## Testing and Troubleshooting

### Google Test

A Google Test unit test program is included in "main.cpp".
This has been run on MacOS, Linux, and Windows.

It is beyond the scope of this documentation to describe how to set up
the build and runtime environments for Google Test.

### Valgrind

The Google Test has been run under Valgrind with no errors.

### Lcov

The Google Test has been run under the "lcov" line coverage tool with
the lbmct files achieving over 96% line coverage.
(The tmr section is lower, but is more-thoroughly tested elsewhere.)

### Recent Events

There is a circular recording of recent "events" stored with the ct_t structure
(lbmct_private.h):
```
  ct->num_recent_events;
  ct->recent_events[LBMCT_MAX_RECENT_EVENTS];
  LBMCT_MAX_RECENT_EVENTS 256
```

There is an API which prints the contents to the UM logging callback:
```
  lbmct_debug_dump()
```

Alternatively, a core dump can be examined and the contents of a ct displayed.

This record of events is only useful to somebody familiar with the
design and coding of CT, and has access to the source code.
The advantage of this record of events is that it is very low impact,
and is therefore left enabled, even in production.
For example, there is no locking or atomic operations.
As a result, it is possible for events to be lost if multiple threads are
recording events simultaneously.
But this is a small price to pay for it being available in core dumps.

Each event stored in recent_events[] as a 32-bit unsigned integer where
the most-significant byte (MSB) indicates the event type and the next
3 bytes are an argument for that event type.

Interpret the values as follows:

MSB:
```
  0: command processed by CT controller thread (cmd type)
  1: src side handle handshake from rcv (msg type)
  2: rcv side handle handshake from src (msg type)
  3: src side send handshake to rcv (msg type)
  4: rcv side send handshake to src (msg type)
```

Command types (for MSB 0):
```
1  LBMCT_CTRLR_CMD_TYPE_TEST = 1,
2  LBMCT_CTRLR_CMD_TYPE_QUIT,
3  LBMCT_CTRLR_CMD_TYPE_SRC_CONN_TICK,
4  LBMCT_CTRLR_CMD_TYPE_CT_SRC_CREATE,
5  LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE,
6  LBMCT_CTRLR_CMD_TYPE_CT_SRC_DELETE,
7  LBMCT_CTRLR_CMD_TYPE_RCV_CONN_TICK,
8  LBMCT_CTRLR_CMD_TYPE_CT_RCV_CREATE,
9  LBMCT_CTRLR_CMD_TYPE_RCV_CONN_CREATE,
a  LBMCT_CTRLR_CMD_TYPE_CT_RCV_DELETE,
b  LBMCT_CTRLR_CMD_TYPE_RCV_CONN_DELETE,
c  LBMCT_CTRLR_CMD_TYPE_RCV_SEND_C_OK,
d  LBMCT_CTRLR_CMD_TYPE_RCV_SEND_D_OK,
```

Message types (for MSB 1-4):
```
  CREQ: 1
  CRSP: 2
  C_OK: 3
  DREQ: 4
  DRSP: 5
  D_OK: 6
```

**Example**

This dump of recent events is from the `CtSimpleMessages1` test in
main.cc.  It is from the `s_ct` object.

```
num_recent_events: 14
recent_events:
[0]  0x00000004  - LBMCT_CTRLR_CMD_TYPE_CT_SRC_CREATE
[1]  0x00000005  - LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE
[2]  0x01000001  - src side handle handshake from rcv (CREQ)
[3]  0x00000005  - LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE
[4]  0x01000001  - src side handle handshake from rcv (CREQ)
    This happens because of the intentional retry forced by the test.
[5]  0x03000002  - src side send handshake to rcv (CRSP)
[6]  0x00000005  - LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE
[7]  0x01000003  - src side handle handshake from rcv (C_OK)
[8]  0x00000006  - LBMCT_CTRLR_CMD_TYPE_CT_SRC_DELETE
[9]  0x03000005  - src side send handshake to rcv (DRSP)
[10] 0x00000003  - LBMCT_CTRLR_CMD_TYPE_SRC_CONN_TICK
[11] 0x03000005  - src side send handshake to rcv (DRSP)
    This happens because of the intentional retry forced by the test.
[12] 0x00000005  - LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE
[13] 0x01000006  - src side handle handshake from rcv (D_OK)
```

## Windows DLL Build

For those users who wish to create a Windows DLL for the Connected Topics
package, a set of files for Microsoft Visual Studio are included.

This procedure assumes that 64-bit UM is being used.

To use these files, perform the following steps.

### Create Project

* Start Visual Studio.
* Pull down "File", select "New" -> "Project..."
  * In left pane, expand "Installed" and "Visual C++" if necessary.
Select "Windows Desktop".
  * In center pane, select "Widows Desktop Wizard".
  * In lower pane, change name "Project1" to "lbmct".
  * Click "OK".
* In "Windows Desktop Project" pop-up dialog box comes up.
  * Change Application type to "Dynamic Link Library (.dll)".
  * Uncheck "Precompiled Header".
  * Click "OK".
* Close the "lbmct.cpp" file editor.
This should display the "Solution Explorer".
* Change the build type from "x86" to "x64".
* Pull down "File", select "Save All".
* ***Exit Visual Studio.***

### Copy GitHub Files

Make sure Visual Studio is ***NOT*** running.

* Open File Explorer window in the GitHub "c" directory.
* Open File Explorer in the new Visual Studio "lbmct" source directory.
* In Visual Studio directory, delete the "header.h", "lbmct.cpp", "targetver.h".
* Copy GitHub "lbmct.vcxproj" **over** the same file in Visual Studio.
* Copy all of the GitHub ".c", ".h", and ".cpp" files to Visual Studio.
Only "dllmain.cpp" will need to be overwritten.

At this point, the Visual Studio project directory should contain the following files:
```
dllmain.cpp           lbmct.vcxproj.filters      min_ct_rcv.c
lbm_internal.h        lbmct.vcxproj.user         min_ct_src.c
lbmct.c               lbmct_private.h            prt.h
lbmct.h               lbmct_rcv.c                tmr.c
lbmct.vcxproj         lbmct_src.c                tmr.h
```

### Fix Paths

* Start Visual Studio.
* Open lbmct Solution file.
(I have sometimes seen it hang on "Preparing Solution File".
It never seems to complete, so I just exit Visual Studio and restart.)
* Right-click the "Solution 'lbmct'" and select "Properties".
* In the left pane, expand "Configuration Properties", "C/C++", and "Linker".
* Select "General" under "C/C++".
  * In the right pane, the content for "Additional Include Directories"
contains the path to the installation of UMS 6.10.
This should be changed to the proper path for your UM product.
(Note: if no UM path exists, make sure your build type is "x64".)
* Select "General" under "Linker".
  * In the right pane, the content for "Additional Library Directories"
contains the path to the installation of UMS 6.10.
This should be changed to the proper path for your UM product.
* Pull down "File", select "Save All".

## Build

* Pull down "Build", select "Rebuild solution".
