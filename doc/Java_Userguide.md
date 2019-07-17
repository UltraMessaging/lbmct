# lbmct v0.6 Java User Guide - Connected Topics for Ultra Messaging

This page explains how to write Java-language Connected Topic application.
For C, see the [C User Guide](Userguide.md).

## Publisher Code Outline

See [MinCtSrc.java](../java/MinCtSrc.java) for a compilable example.

A publisher of a connected topic needs to perform the following:

**Initialization**

* Create a context object with the normal API:
[LBMContext](https://ultramessaging.github.io/currdoc/doc/JavaAPI/classcom_1_1latencybusters_1_1lbm_1_1LBMContext.html#ad6173b302534ee9011ce56897a6952ac).

* Create a CT object with the CT API:
[LbmCt](https://ultramessaging.github.io/lbmct/javadoc/com/latencybusters/lbmct/LbmCt.html#LbmCt--).
This only creates the object; it still needs to be initialized, which is done with the CT API:
[LbmCt::start](https://ultramessaging.github.io/lbmct/javadoc/com/latencybusters/lbmct/LbmCt.html#start-com.latencybusters.lbm.LBMContext-com.latencybusters.lbmct.LbmCtConfig-java.nio.ByteBuffer-).
This CT object is associated with a context and should be thought of as
analogous to a context.
For example, most applications have only one CT object which can host any
number of CT Sources and CT Receivers.

* Create a CT Source object with a CT API:
[lbmct_src_create()](#lbmct_src_create).
Note that the normal source APIs involve allocating a source topic,
and then creating the source object.
The CT API combines those two operations into a single API.
Also, the publisher provides two application callback functions
that the CT Source executes as connections are established and closed.

* Get the underlying UM Source object with the CT API:
[lbmct_src_get_um_src()](#lbmct_src_get_um_src).

* Optionally wait for a connection to be created.
This is signaled by the CT Source executing the connection creation application
callback function.

**Steady State**

* Send messages with the normal API:
[lbm_src_send()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a91f4b9cb04fe1323ec56833211cc5cb7)
(or other normal UM send methods).

**Cleanup**

* Delete a CT Source object with a CT API:
[lbmct_src_delete()](#lbmct_src_delete).
This API will gracefully close any open connections associated with that
source.

* Optionally wait for any open connections to be deleted.
This is signaled by the CT Source executing the connection deletion application
callback function.

* Delete the CT object with the CT API:
[lbmct_delete()](#lbmct_delete).
Note that if there are still existing CT Sources (or Receivers),
or if CT connections are still in the process of gracefully closing,
the call to delete will fail,
requiring the application to wait and re-try.

* Delete the context with the normal API:
[lbm_context_delete()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a962bfceb336c65191ba08497ac70602b).

---

## Subscriber Code Outline

See [min_ct_rcv.c](../c/min_ct_rcv.c) for a compilable example.

A subscriber of a connected topic needs to perform the following:

**Initialization**

* Create a context object with the normal API:
[lbm_context_create()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a8058947690bd0995bc2c59d4a61b462f).

* Create a CT object with the CT API:
[lbmct_create()](#lbmct_create).
This CT object is associated with a context and should be thought of as
analogous to a context.
For example, one CT object can host any number of CT Receivers (and CT Sources).

* Create a CT Receiver object with a CT API:
[lbmct_rcv_create()](#lbmct_rcv_create).
Note that the normal receiver APIs involve looking up a receiver topic,
and then creating the receiver object.
The CT API combines those two operations into a single API.
Also, the subscriber provides two application callback functions
that the CT Receiver executes as connections are established and closed.

* As the CT Receiver discovers and connects to CT Sources,
it executes the connection creation application callback function.

**Steady State**

* As messages are received, the CT Receiver calls the receiver
application callback function.
The receiver callback is coded and behaves the same as the normal API.

**Cleanup**

* Delete a CT Receiver object with a CT API:
[lbmct_rcv_delete()](#lbmct_rcv_delete).
This API will gracefully close any open connections associated with that
receiver.

* Optionally wait for any open connections to be deleted.
This is signaled by the CT Source executing the connection deletion application
callback function.

* Delete the CT object with the CT API:
[lbmct_delete()](#lbmct_delete).
Note that if there are still existing CT Receivers (or Sources),
or if CT connections are still in the process of gracefully closing,
the call to delete will fail,
requiring the application to wait and re-try.

* Delete the context with the normal API:
[lbm_context_delete()](https://ultramessaging.github.io/currdoc/doc/API/lbm_8h.html#a962bfceb336c65191ba08497ac70602b).

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

The LbmCtCtrlr object contains a blocking queue named "debugQ" which is used
as a simple unbounded event recorder.
Its operation is controlled by setting the

