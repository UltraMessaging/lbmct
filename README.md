# lbmct v0.6 README - Connected Topics for Ultra Messaging

## LBMCT IS IN TRANSITION

As part of creating a Java version of connected topics, the github repo is in
a state of transition.
The C and Java version do not interoperate at this time.
The C version will be updated to conform to changes made to the wire protocol.

Also, some changes will be made to the C API which will be incompatible with
the existing (0.5) version of the API.

## Introduction

See [Release Notes](doc/Release_notes.md) for change log.

The "lbmct" package is an API wrapper around the Ultra Messaging library which
adds the concept of end-to-end connectedness to UM's normal pub/sub messaging
model.
These "Connected Topics" (CTs) help Publisher and Subscriber applications
keep track of each other.

Instead of creating normal topic Sources and Receivers,
applications create CT Sources and CT Receivers.
The lbmct layer exchanges handshake messages between the CT Source and
Receivers to establish shared state.
Once state is synchronized, callbacks are made to the
applications to deliver connection information and indicate liveness.

Once the connection is live, the application uses the normal methods for
sending and receiving messages.
There is almost no overhead added in the message path.

When the connection is deleted, either gracefully (via API call) or
abruptly (e.g. by application or network failure),
callbacks are made to the publishing and receiving applications to
deliver deletion events.

The rest of this page provides a high-level description of the CT package.
You can also jump to other pages:
* [Release Notes](doc/Release_notes.md)
* [User Guide](doc/Userguide.md) - explains how to write Connected
Topic applications.
* [API Reference](doc/API.md) - details on each API.
* [Internal Design](doc/Internal_Design.md) - details of the CT implementation.

### Source Code

The lbmct package is provided in source code form at
[Ultra Messaging GitHub](https://github.com/UltraMessaging).

Note that CT is not officially part of the UM product family.
Informatica's position is that that UM conforms to pub/sub messaging
semantics, and that the concept of "connectedness" is an application concern.
Users who needed connection-orientation are expected to use the pub/sub
messaging primitives to implement their desired behaviors.

This implementation of CT should be considered "example" code,
to be incorporated into the user's application and modified to meet
the user's individual requirements.
If those modifications are reasonably general in nature,
the user is invited to submit "pull" requests on GitHub.
Informatica will review the proposed changes and will usually accept the
pull request, incorporating the improvements.

Additionally, Informatica is committed to fixing bugs.
Please raise issues using the normal GitHub issue tracking functionality,
or through the normal Informatica support organization.

In other words, although CT is not officially part of the UM product family,
Informatical intends to support it.
However, we don't currently plan to include CT as part of the product package.

Finally, be aware that that:

* Informatica does not include CT in its QA testing.
Note however that CT includes a reasonably extensive automated self-test.

* Informatica does not guarantee that future UM development will
be compatible with CT.
Note however that CT was developed with knowledge of the current UM roadmap,
and every effort has been taken to make it forward compatible with known
product evolution directions.

Naturally, the amount of effort we invest in CT will be proportional to
the level of user interest.
If you decide to use CT, please let us know through the support organization!
We will keep you informed of the package's continued evolution.

---

## Features

* Publishing and subscribing applications can provide a block of user-defined
metadata which will be delivered to the connection peer via the connection
create callback.
This is typically used for application identification.

* As part of connection creation and deletion, both the publisher and
subscriber are informed of the first and last topic sequence numbers
delivered to the subscriber.

* A single Connected Source can establish connections with
multiple Connected Receivers (on the same topic).
Similarly, a single Connected Receiver can establish connections with
multiple Connected Sources (on the same topic).
Each connection is independent and is maintained separately.

* Once the connection create callback is called, the sending application
can safely send messages without concern of "head loss" for that receiver.

* CT is compatible with most UM "streaming" features.
But see [Limitations](#limitations).

* Since CT uses external interfaces of UM, it is not tied to a specific
version of UM.
However, it does make use of the [Sending to Sources](https://ultramessaging.github.io/currdoc/doc/Design/umfeatures.html#sendingtosources) feature,
so CT does require at least UM version 6.10.

* Both normal and Connected Sources and Receivers can share the same
UM context, and even the same transport sessions.
But see [Interoperability](#interoperability).

---

## Opportunities and Limitations

Most of the limitations that follow could be implemented,
with varying degrees of effort.

* CT is not compatible with UM’s queuing features, including ULB.

* CT is not compatible with the
[Late Join](https://ultramessaging.github.io/currdoc/doc/Design/fundamentalconcepts.html#latejoin)
feature.
This is mostly because the Source's handshake message needs to be live,
and needs to be the first sequence number delivered.
In contrast, late join messages are delivered prior to live messages,
before the connection is established.

* CT does not work with Persistence.
I see some fundamental problems with a general Persistence solution,
partly due to CT's incompatibility with Late Join, and also because
a recovering subscriber may not have access to a live publisher with which
to exchange handshakes.
But there may be some kind of limited version of it that could be implemented.
If you're interested, please help brainstorm how it might work.

* CT does not yet work with wildcard receivers.
But it should work fine with appropriate CT API additions.

* CT does not yet work with Smart Sources.
This work is planned, but there are some threading concerns.
Please let Informatica know if this is an area of your interest.

* CT does not yet have connection-level keepalive functionality.
This will require discussion as it could introduce undesirable side effects
like latency jitter and scalability problems.
Please let Informatica know if this is an area of your interest.

* CT does not have a "sequential mode" whereby an application can
create and donate its thread for CT's internal controller.
(However, be aware that CT's internal controller thread does not get involved
in the latency-sensitive main message path.)

* CT does not yet have a .NET wrapper.

* Do we need a "Force Quit" flag for connection and CT objects?

* In a DRO environment, you need to configure CT what the domain ID is.
CT does not discover the domain ID dynamically.
If using DRO, see [Domain ID](doc/Domain_ID.md).

---

## Caveats

* Each connection is reasonably light-weight, but might not scale into
the thousands.

* The receiver-side BOS/EOS events are suppressed.
Their delivery could not be made reliable due to their
“transport session-level” scope.
However, CT’s per-connection create/delete callbacks are reliable
replacements.

   * Note that source-side connect/disconnect events are *not* suppressed,
but those events are not correlated to a CT connection.
Users are encouraged to ignore those UM connect/disconnect events and
should use CT’s per-connection create/delete callbacks instead.

* CT makes use of receive-side, per-source, delivery controller create/delete
notification (“receiver source_notification_function”),
so the application must not use it.
However, the per-connection create/delete callbacks are reliable replacements.

* CT control handshake messages are sent on the same data path as application
messages, and are therefore delivered to the receiving application as messages.
They are easy to differentiate from application messages by an API.

   * Note that receiving applications can get control messages that are
intended for other connections.
The user is advised to ignore CT control handshake messages,
except that they consume topic-level sequence numbers.

* When connected sources and receivers are deleted by API call,
there is a handshake between the source and receiver to do a clean disconnect.
The API call to delete a CT source or receiver is a blocking call that does
not return until the handshake is complete, or the attempt times out.

   * Attempting to delete the parent CT object will fail if there are still
connections waiting to be fully disconnected.
There is no “force quit” API, although the process can simply exit.

---

## Interoperability

* CT Receivers make use of the “send to source” feature,
which first became available in UM 6.10.
Note that pre-6.10 versions of UM are still able to receive messages from
a CT Source, as “observers” (see next item).

* A CT Source can be subscribed by a non-CT Receiver.
This is a valid “observer” use case and will not trigger any CT-oriented
activities on the source.
Note that the source will be nominally unaware of non-CT Receivers and will
not be able to issue any kind of warning if this use case is not desired.

* A CT Receiver subscribing to a non-CT Source is not recommended.
The CT Receiver will repeatedly send CT control handshake messages to the
source context.
Since it will not receive the proper handshake response,
the receiver will normally be “deaf” to the non-CT Source.
However, the [pre_delivery](doc/API.md#lbmct_config_tpre_delivery) option can
be configured to force delivery of messages received from non-connected sources.
The receiver will still attempt (and fail, which will be logged) to connect to
the source, but at least the messages from the non-CT Source will be delivered.

* When Smart Source is added, the sender will need to be at least UM 6.10,
although the Smart Source code could be conditionally compiled.

---

You can now jump to other pages:
* [Release Notes](doc/Release_notes.md)
* [C User Guide](doc/Userguide.md) - explains how to write C-language Connected
Topic applications.
* [Java User Guide](doc/Java_Userguide.md) - explains how to write Java-language
Connected Topic applications.
* [C API Reference](doc/API.md) - details on each C API function and structure.
* [Java API Reference](doc/Java_API.md) - details on each Java API function
and structure.
* [Internal Design](doc/Internal_Design.md) - details of the CT implementation.
