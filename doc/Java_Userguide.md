# lbmct v0.6 Java User Guide - Connected Topics for Ultra Messaging

This page explains how to write Java-language Connected Topic application.
For C, see the [C User Guide](Userguide.md).

## Publisher Code Outline

See [MinCtSrc.java](../java/MinCtSrc.java) for a compilable and runnable example.

A publisher of a connected topic needs to perform the following:

**Initialization**

* Create a context object with the standard UM constructor
[LBMContext](https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMContext.html#ad6173b302534ee9011ce56897a6952ac).

* Create a CT object with the constructor
[LbmCt](https://ultramessaging.github.io/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCt.html#a4f632556d3bc12700fff65cc260bf4f5).
This only creates the object.
It still needs to be initialized, which is done with the method
[LbmCt.start](https://ultramessaging.github.io/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCt.html#a7e3e89cbf93427d5d7595207a4424574).
This CT object is associated with a context and should be thought of as
analogous to a context.
For example, most applications have only one CT object which can host any
number of CT Sources and CT Receivers.

* Create a CT Source object with the constructor
[LbmCtSrc](https://ultramessaging.github.io/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCtSrc.html#aa1acb15c5da336056f614342a8b731e0).
This only creates the object.
It still needs to be initialized, which is done with the method
[LbmCtSrc.start](https://ultramessaging.github.io/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCtSrc.html#a7f81963b12b3122f03ee3e2675b362d0).
Note that the normal UM source APIs involve allocating a source topic,
and then creating the source object.
The CT API combines those two operations into a single API.
Also, the publisher provides two application callback functions
that the CT Source executes as connections are established and closed.

* Get the underlying UM Source object with the method
[LbmCtSrc.getUmSrc](https://ultramessaging.github.io/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCtSrc.html#a5338bd2f7377d9190c6b0e18c9117524).

* Optionally wait for a connection to be created.
This is signaled by the CT Source executing the connection creation application
callback function.

**Steady State**

* Send messages with one of the standard UM <tt>send</tt> methods of the
[LBMSource](https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMSource.html)
class.
(The example uses the method
[send(byte[], int, int)](https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMSource.html#ab2f7919b79f87d18beac4d26843afed7).)

**Cleanup**

* Delete the CT source with the method
[LbmCtSrc.stop](https://ultramessaging.github.io/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCtSrc.html#a8ad1eb28a391b343cffe72fcd011df0f).
This API will gracefully close any open connections associated with that
source.

* Delete the CT object with the method
[LbmCt.stop](https://ultramessaging.github.io/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCt.html#a33aca22355565f7c05e189248e23ee95).
Note that if there are still existing CT Sources (or Receivers),
the call to delete will fail.

* Delete the context with the normal UM method
[LBMContext.close](https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMContext.html#ac65cc7421c5e241b478ab51fdd17b71b).

---

## Subscriber Code Outline

See [MinCtRcv.java](../java/MinCtRcv.java) for a compilable and runnable example.

A subscriber of a connected topic needs to perform the following:

**Initialization**

* Create a context object with the standard UM constructor
[LBMContext](https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMContext.html#ad6173b302534ee9011ce56897a6952ac).

* Create a CT object with the constructor
[LbmCt](https://ultramessaging.github.io/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCt.html#a4f632556d3bc12700fff65cc260bf4f5).
This only creates the object.
It still needs to be initialized, which is done with the method
[LbmCt.start](https://ultramessaging.github.io/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCt.html#a7e3e89cbf93427d5d7595207a4424574).
This CT object is associated with a context and should be thought of as
analogous to a context.
For example, most applications have only one CT object which can host any
number of CT Sources and CT Receivers.

* Create a CT Receiver object with the constructor
[LbmCtRcv](file:///Users/sford/Documents/GitHub/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCtRcv.html#a53b8fa9db0ee57ab4e00d31dbb4f5c88).
This only creates the object.
It still needs to be initialized, which is done with the method
[LbmCtRcv.start](file:///Users/sford/Documents/GitHub/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCtRcv.html#a7a6a9246fc34dbaf21f6e5fba5eab548).
Note that the normal UM source APIs involve looking up a source topic,
and then creating the receiver object.
The CT API combines those two operations into a single API.
Also, the subscriber provides two application callback functions
that the CT Source executes as connections are established and closed.

* As the CT Receiver discovers and connects to CT Sources,
it executes the connection creation application callback function.

**Steady State**

* As messages are received, the CT Receiver calls the receiver
application callback function.
The receiver callback is coded and behaves the same as the normal UM API.

**Cleanup**

* Delete a CT Receiver object with the method
[LbmCtRcv.stop](file:///Users/sford/Documents/GitHub/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCtRcv.html#a164eb12ed3bf67bb1bc4442e07ed1195).
This API will gracefully close any open connections associated with that
receiver.

* Delete the CT object with the method
[LbmCt.stop](https://ultramessaging.github.io/lbmct/doc/java/classcom_1_1latencybusters_1_1lbmct_1_1LbmCt.html#a33aca22355565f7c05e189248e23ee95).
Note that if there are still existing CT Sources (or Receivers),
the call to delete will fail.

* Delete the context with the normal UM method
[LBMContext.close](https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMContext.html#ac65cc7421c5e241b478ab51fdd17b71b).

---

## Application Metadata

When a publisher or subscriber create a CT object
(with [lbmct_create()](#lbmct_create)),
the application can optionally pass in a block of binary application metadata.
This block of data is delivered to the connected peer in its connection
create application callback.
This is done symmetrically: publisher metadata is delivered to subscriber,
and subscriber metadata is delivered to publisher.
Application would normally provide some sort of application-specific
identification information so that each application knows "who" the peer is.

No attempt is made to interpret this data.
Many applications supply a simple string.
If C binary structures are supplied, be aware that the fields will not be
marshalled; if platforms of different "endian" are used,
it is the application's responsibility to convert between host and network
order.

## Debugging

The [LbmCtConfig](https://ultramessaging.github.io/lbmct/javadoc/com/latencybusters/lbmct/LbmCtConfig.html)
object has an option for debugging which is set by
[setTestBits](https://ultramessaging.github.io/lbmct/javadoc/com/latencybusters/lbmct/LbmCtConfig.html#setTestBits-int-).
This is a bitmap of values which enable various behavior changes used for testing and debugging.
A normal application should have no need of using this feature.
It is intended for a maintainer of CT to diagnose problems with the package and for writing automated tests.

In particular, the bitmap value
[TEST_BITS_DEBUG](https://ultramessaging.github.io/lbmct/javadoc/com/latencybusters/lbmct/LbmCtConfig.html#TEST_BITS_DEBUG)
enables a simple internal unbounded event recorder which can show the sequence of events that lead up to
a misbehavior.
The LbmCtCtrlr object contains a blocking queue named `debugQ` into which debugging strings are enqueued
using the method <tt>LbmCt.dbg(String)</tt>.
Since it is an unbounded queue, it should not be enabled for long periods of time.

To examine the queue, I've just been setting a breakpoint and using the debugger.
It would be straight-forward to print it as well.
